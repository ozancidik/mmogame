// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSupabaseCharacterStore.h"

#if !UE_BUILD_SHIPPING
#include "Async/Async.h"
#include "Core/MMONameRules.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/SecureHash.h"
#include "Async/TaskGraphInterfaces.h"

// Supabase karakter deposu native testi (docs/DESIGN_SUPABASE.md 4, Dilim 2). Sahte ulasim + sahte sunucu
// (mmo_api RPC sozlesmesinin kucuk bir modeli); gercek ag, gercek Supabase, diske yazma YOK.
// Sozlesme varsayimlari (supabase/tests/vectors.json henuz yok, DESIGN 3.3/3.6'ya gore burada gomulu):
//  - acquire_lease ok yaniti: {outcome:"ok", data:<kayit nesnesi>, revision, lease_epoch, schema_version}
//  - save_character: {outcome:"committed", revision}; get_op: {outcome:"ok", found:bool, result:{...}}
//  - character_exists: {outcome:"ok", exists:bool}; int64 alanlar ondalik-siz METIN (sayi da tolere edilir).
namespace
{
	using SupabaseOut = EMMOSupabaseStoreOutcome;

	constexpr int32 SupabaseTestExpectedChecks = 147;
	const TCHAR* SupabaseTestKey = TEXT("sb_secret_TESTKEYDONOTLEAK0123456789abcdef");
	const TCHAR* SupabaseTestHost = TEXT("zzunittestref.supabase.co");
	const TCHAR* SupabaseTestUrl = TEXT("https://zzunittestref.supabase.co");
	const TCHAR* SupabaseEnvUrlName = TEXT("MMO_SUPABASE_URL");
	const TCHAR* SupabaseEnvKeyName = TEXT("MMO_SUPABASE_SECRET_KEY");

	// ---- sahte sunucu ----
	struct FSupabaseRecord
	{
		FString DataJson;
		int64 Revision = 0;
		int64 Epoch = 0;
		FString Holder;
	};

	struct FSupabaseOpRecord
	{
		FString Key;
		FString Digest;
		FString Result;
	};

	TSharedPtr<FJsonObject> SupabaseParse(const FString& Text)
	{
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj))
		{
			Obj.Reset();
		}
		return Obj;
	}

	FString SupabaseText(const TSharedRef<FJsonObject>& Obj)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Obj, Writer);
		Writer->Close();
		return Text;
	}

	FString SupabaseI64(int64 V)
	{
		return FString::Printf(TEXT("%lld"), static_cast<long long>(V));
	}

	class FSupabaseFakeServer
	{
	public:
		TMap<FString, FSupabaseRecord> Records;
		TMap<FString, FSupabaseOpRecord> Ops;
		int32 Commits = 0;
		bool bEmitNumbers = false;
		bool bGarbageData = false;
		TSharedPtr<FJsonValue> RevisionOverride;

		void Seed(const FString& Key, const FMMOCharacterSaveData& Data, int64 Revision, int64 Epoch)
		{
			FSupabaseRecord R;
			FMMOCharacterSaveJson::ToJson(Data, R.DataJson);
			R.Revision = Revision;
			R.Epoch = Epoch;
			Records.Add(Key, R);
		}

		void Steal(const FString& Key, const FString& NewHolder)
		{
			if (FSupabaseRecord* R = Records.Find(Key))
			{
				++R->Epoch;
				R->Holder = NewHolder;
			}
		}

		TSharedPtr<FJsonValue> Int(int64 V) const
		{
			if (bEmitNumbers)
			{
				return MakeShared<FJsonValueNumber>(static_cast<double>(V));
			}
			return MakeShared<FJsonValueString>(SupabaseI64(V));
		}

		static FString Outcome(const TCHAR* O)
		{
			const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("outcome"), O);
			return SupabaseText(Obj);
		}

		FString Handle(const FString& Fn, const TSharedPtr<FJsonObject>& B, int32& OutHttp)
		{
			OutHttp = 200;
			if (!B.IsValid())
			{
				OutHttp = 400;
				return TEXT("{}");
			}
			if (Fn == TEXT("acquire_lease"))
			{
				FSupabaseRecord* R = Records.Find(B->GetStringField(TEXT("p_key")));
				const FString Holder = B->GetStringField(TEXT("p_holder"));
				if (!R)
				{
					return Outcome(TEXT("not_found"));
				}
				if (!R->Holder.IsEmpty() && R->Holder != Holder)
				{
					return Outcome(TEXT("ownership_lost"));
				}
				if (R->Holder != Holder)
				{
					++R->Epoch;
					R->Holder = Holder;
				}
				const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("outcome"), TEXT("ok"));
				if (bGarbageData)
				{
					const TSharedRef<FJsonObject> Junk = MakeShared<FJsonObject>();
					Junk->SetStringField(TEXT("junk"), TEXT("x"));
					Obj->SetObjectField(TEXT("data"), Junk);
				}
				else
				{
					Obj->SetObjectField(TEXT("data"), SupabaseParse(R->DataJson));
				}
				Obj->SetField(TEXT("revision"), RevisionOverride.IsValid() ? RevisionOverride : Int(R->Revision));
				Obj->SetField(TEXT("lease_epoch"), Int(R->Epoch));
				Obj->SetNumberField(TEXT("schema_version"), 2);
				return SupabaseText(Obj);
			}
			if (Fn == TEXT("create_character"))
			{
				const FString Key = B->GetStringField(TEXT("p_key"));
				if (Records.Contains(Key))
				{
					return Outcome(TEXT("already_exists"));
				}
				FSupabaseRecord R;
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (B->TryGetObjectField(TEXT("p_data"), DataObj) && DataObj)
				{
					R.DataJson = SupabaseText((*DataObj).ToSharedRef());
				}
				Records.Add(Key, R);
				return Outcome(TEXT("committed"));
			}
			if (Fn == TEXT("character_exists"))
			{
				const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("outcome"), TEXT("ok"));
				Obj->SetBoolField(TEXT("exists"), Records.Contains(B->GetStringField(TEXT("p_key"))));
				return SupabaseText(Obj);
			}
			if (Fn == TEXT("get_op"))
			{
				const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("outcome"), TEXT("ok"));
				if (const FSupabaseOpRecord* Op = Ops.Find(B->GetStringField(TEXT("p_op_id"))))
				{
					Obj->SetBoolField(TEXT("found"), true);
					Obj->SetObjectField(TEXT("result"), SupabaseParse(Op->Result));
				}
				else
				{
					Obj->SetBoolField(TEXT("found"), false);
				}
				return SupabaseText(Obj);
			}
			if (Fn == TEXT("save_character"))
			{
				const FString Key = B->GetStringField(TEXT("p_key"));
				FSupabaseRecord* R = Records.Find(Key);
				if (!R)
				{
					return Outcome(TEXT("not_found"));
				}
				const FString OpId = B->GetStringField(TEXT("p_op_id"));
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				B->TryGetObjectField(TEXT("p_data"), DataObj);
				const FString DataText = DataObj ? SupabaseText((*DataObj).ToSharedRef()) : FString();
				const FString Expected = B->GetStringField(TEXT("p_expected_revision"));
				const FString Digest = FMD5::HashAnsiString(*(Key + TEXT("|") + Expected + TEXT("|") + DataText));
				if (const FSupabaseOpRecord* Prev = Ops.Find(OpId))
				{
					// Kayitli islem: kira/revision'a bakmadan kayitli sonucu don.
					return (Prev->Key == Key && Prev->Digest == Digest) ? Prev->Result : Outcome(TEXT("idempotency_conflict"));
				}
				if (R->Holder != B->GetStringField(TEXT("p_holder")) || SupabaseI64(R->Epoch) != B->GetStringField(TEXT("p_epoch")))
				{
					return Outcome(TEXT("ownership_lost"));
				}
				if (SupabaseI64(R->Revision) != Expected)
				{
					const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
					Obj->SetStringField(TEXT("outcome"), TEXT("revision_conflict"));
					Obj->SetField(TEXT("revision"), Int(R->Revision));
					return SupabaseText(Obj);
				}
				R->DataJson = DataText;
				++R->Revision;
				++Commits;
				const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("outcome"), TEXT("committed"));
				Obj->SetField(TEXT("revision"), Int(R->Revision));
				const FString Result = SupabaseText(Obj);
				Ops.Add(OpId, {Key, Digest, Result});
				return Result;
			}
			OutHttp = 404;
			return TEXT("{\"code\":\"PGRST202\"}");
		}
	};

	// ---- sahte ulasim ----
	struct FSupabaseFault
	{
		enum class EKind : uint8 { Normal, CommitThenTimeout, TimeoutNoCommit, NetworkError, Http, RawBody };
		EKind Kind = EKind::Normal;
		int32 Code = 0;
		FString Body;
		double Elapsed = 0.05;
	};

	FSupabaseFault SupabaseFaultOf(FSupabaseFault::EKind Kind, double Elapsed = 0.05, int32 Code = 0, const FString& Body = FString())
	{
		FSupabaseFault F;
		F.Kind = Kind;
		F.Elapsed = Elapsed;
		F.Code = Code;
		F.Body = Body;
		return F;
	}

	class FSupabaseFakeTransport : public IMMOHttpTransport
	{
	public:
		FSupabaseFakeServer Server;
		TArray<FMMOHttpRequest> Requests;
		TArray<FSupabaseFault> Script;
		TArray<float> ScheduledDelays;
		double Now = 1000.0;
		bool bDeliverOnWorker = false;

		virtual void Send(const FMMOHttpRequest& Request, FResponseHandler&& OnResponse) override
		{
			Requests.Add(Request);
			FSupabaseFault Fault;
			if (Script.Num() > 0)
			{
				Fault = Script[0];
				Script.RemoveAt(0);
			}
			FString Fn;
			Request.Url.Split(TEXT("/rpc/"), nullptr, &Fn);
			FPending P;
			P.Handler = MoveTemp(OnResponse);
			P.Elapsed = Fault.Elapsed;
			switch (Fault.Kind)
			{
			case FSupabaseFault::EKind::Normal:
			{
				int32 Http = 200;
				P.Resp.Body = Server.Handle(Fn, SupabaseParse(Request.Body), Http);
				P.Resp.Result = EMMOHttpResult::Ok;
				P.Resp.HttpCode = Http;
				break;
			}
			case FSupabaseFault::EKind::CommitThenTimeout:
			{
				int32 Http = 200;
				Server.Handle(Fn, SupabaseParse(Request.Body), Http); // uygulanir, yanit kaybolur
				P.Resp.Result = EMMOHttpResult::Timeout;
				break;
			}
			case FSupabaseFault::EKind::TimeoutNoCommit:
				P.Resp.Result = EMMOHttpResult::Timeout;
				break;
			case FSupabaseFault::EKind::NetworkError:
				P.Resp.Result = EMMOHttpResult::NetworkError;
				break;
			case FSupabaseFault::EKind::Http:
				P.Resp.Result = EMMOHttpResult::Ok;
				P.Resp.HttpCode = Fault.Code;
				P.Resp.Body = Fault.Body;
				break;
			case FSupabaseFault::EKind::RawBody:
				P.Resp.Result = EMMOHttpResult::Ok;
				P.Resp.HttpCode = 200;
				P.Resp.Body = Fault.Body;
				break;
			}
			Pending.Add(MoveTemp(P));
		}

		virtual double NowSeconds() const override { return Now; }

		virtual void ScheduleAfter(float DelaySeconds, TFunction<void()>&& Task) override
		{
			ScheduledDelays.Add(DelaySeconds);
			FScheduled S;
			S.Delay = DelaySeconds;
			S.Task = MoveTemp(Task);
			Scheduled.Add(MoveTemp(S));
		}

		/** Bekleyen yanitlari ve zamanlanmis isleri sirayla calistirir (saati ilerleterek). */
		void Pump()
		{
			int32 Guard = 0;
			while ((Pending.Num() > 0 || Scheduled.Num() > 0) && ++Guard < 2000)
			{
				if (Pending.Num() > 0)
				{
					FPending P = MoveTemp(Pending[0]);
					Pending.RemoveAt(0);
					Now += P.Elapsed;
					if (bDeliverOnWorker)
					{
						FResponseHandler H = P.Handler;
						FMMOHttpResponse R = P.Resp;
						Async(EAsyncExecution::ThreadPool, [H, R]() mutable { H(MoveTemp(R)); }).Wait();
						FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
					}
					else
					{
						P.Handler(MoveTemp(P.Resp));
					}
				}
				else
				{
					FScheduled S = MoveTemp(Scheduled[0]);
					Scheduled.RemoveAt(0);
					Now += S.Delay;
					S.Task();
				}
			}
		}

		int32 CountCalls(const TCHAR* Fn) const
		{
			int32 N = 0;
			for (const FMMOHttpRequest& R : Requests)
			{
				if (R.Url.EndsWith(FString(TEXT("/rpc/")) + Fn))
				{
					++N;
				}
			}
			return N;
		}

		TSharedPtr<FJsonObject> LastBody(const TCHAR* Fn) const
		{
			for (int32 I = Requests.Num() - 1; I >= 0; --I)
			{
				if (Requests[I].Url.EndsWith(FString(TEXT("/rpc/")) + Fn))
				{
					return SupabaseParse(Requests[I].Body);
				}
			}
			return nullptr;
		}

	private:
		struct FPending
		{
			FResponseHandler Handler;
			FMMOHttpResponse Resp;
			double Elapsed = 0.0;
		};
		struct FScheduled
		{
			float Delay = 0.f;
			TFunction<void()> Task;
		};
		TArray<FPending> Pending;
		TArray<FScheduled> Scheduled;
	};

	class FSupabaseLogCapture : public FOutputDevice
	{
	public:
		FCriticalSection Lock;
		FString All;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			FScopeLock Guard(&Lock);
			All += V;
			All += TEXT("\n");
		}
	};

	struct FSupabaseLogScope
	{
		FSupabaseLogCapture& Cap;
		ELogVerbosity::Type Prev;
		explicit FSupabaseLogScope(FSupabaseLogCapture& InCap) : Cap(InCap), Prev(LogMMOSupabase.GetVerbosity())
		{
			LogMMOSupabase.SetVerbosity(ELogVerbosity::VeryVerbose);
			GLog->AddOutputDevice(&Cap);
		}
		~FSupabaseLogScope()
		{
			GLog->RemoveOutputDevice(&Cap);
			LogMMOSupabase.SetVerbosity(Prev);
		}
	};

	FMMOCharacterSaveData SupabaseFixture(int32 Gold, int32 Level = 7)
	{
		FMMOCharacterSaveData D;
		D.bValid = true;
		D.Level = Level;
		D.Gold = Gold;
		return D;
	}

	FMMOSupabaseConfig SupabaseGoodConfig()
	{
		FMMOSupabaseConfig C;
		C.BaseUrl = SupabaseTestUrl;
		C.SecretKey = SupabaseTestKey;
		return C;
	}

	struct FSupabaseCtx
	{
		TSharedRef<FSupabaseFakeTransport> Tr = MakeShared<FSupabaseFakeTransport>();
		TSharedPtr<FMMOSupabaseRestClient> Client;
		TUniquePtr<FMMOSupabaseCharacterStore> Store;

		explicit FSupabaseCtx(const TCHAR* Holder = TEXT("srv-a"))
		{
			FString Error;
			Client = FMMOSupabaseRestClient::Create(Tr, SupabaseGoodConfig(), Error);
			if (Client.IsValid())
			{
				Store = MakeUnique<FMMOSupabaseCharacterStore>(Client.ToSharedRef(), Holder);
			}
		}
	};

	struct FSupabaseRes
	{
		int32 Calls = 0;
		SupabaseOut O = SupabaseOut::InternalError;
		FMMOCharacterSaveData D;
		bool bGame = false;
		bool bExists = false;
	};

	TSharedRef<FSupabaseRes> SupabaseLoad(FSupabaseCtx& C, const TCHAR* Slot)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->LoadEx(Slot, [R](SupabaseOut O, FMMOCharacterSaveData&& D)
		{
			++R->Calls;
			R->O = O;
			R->D = MoveTemp(D);
			R->bGame = IsInGameThread();
		});
		C.Tr->Pump();
		return R;
	}

	TSharedRef<FSupabaseRes> SupabaseSave(FSupabaseCtx& C, const TCHAR* Slot, const FMMOCharacterSaveData& Data, bool bPump = true)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->SaveEx(Slot, Data, [R](SupabaseOut O)
		{
			++R->Calls;
			R->O = O;
			R->bGame = IsInGameThread();
		});
		if (bPump)
		{
			C.Tr->Pump();
		}
		return R;
	}

	TSharedRef<FSupabaseRes> SupabaseCreate(FSupabaseCtx& C, const TCHAR* Slot, const FMMOCharacterSaveData& Data)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->CreateEx(Slot, Data, [R](SupabaseOut O) { ++R->Calls; R->O = O; });
		C.Tr->Pump();
		return R;
	}

	TSharedRef<FSupabaseRes> SupabaseExists(FSupabaseCtx& C, const TCHAR* Slot)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->ExistsEx(Slot, [R](SupabaseOut O, bool bExists) { ++R->Calls; R->O = O; R->bExists = bExists; });
		C.Tr->Pump();
		return R;
	}

	TSharedRef<FSupabaseRes> SupabaseReconcile(FSupabaseCtx& C, const TCHAR* Slot)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->ReconcileEx(Slot, [R](SupabaseOut O) { ++R->Calls; R->O = O; });
		C.Tr->Pump();
		return R;
	}

	// Kodda yalniz ASCII: ASCII disi harf (U+00D6) kod noktasindan uretilir.
	FString SupabaseNonAsciiSlot()
	{
		return FString(TEXT("MMOSave_")) + FString::Chr(static_cast<TCHAR>(0x00D6)) + TEXT("mer");
	}

	bool SupabaseLeaks(const FString& S)
	{
		return S.Contains(SupabaseTestKey, ESearchCase::CaseSensitive) || S.Contains(SupabaseTestHost, ESearchCase::IgnoreCase);
	}

	bool SupabaseRunSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone || IsRunningDedicatedServer()
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{
			OutError = TEXT("disposable authority headless game world required");
			return false;
		}
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SUPABASE_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK)
			{
				++Failures;
				if (OutError.IsEmpty())
				{
					OutError = FString(Case) + TEXT("/") + Label;
				}
			}
		};

		FSupabaseLogCapture Cap;
		const FSupabaseLogScope LogScope(Cap);

		// ---------------- cfg ----------------
		{
			FString E;
			FMMOSupabaseConfig C = SupabaseGoodConfig();
			C.SecretKey.Reset();
			Check(TEXT("cfg"), TEXT("empty_key_rejected"), !FMMOSupabaseConfig::Validate(C, E));
			C = SupabaseGoodConfig();
			C.BaseUrl.Reset();
			Check(TEXT("cfg"), TEXT("empty_url_rejected"), !FMMOSupabaseConfig::Validate(C, E));
			C = SupabaseGoodConfig();
			C.BaseUrl = TEXT("http://zzunittestref.supabase.co");
			Check(TEXT("cfg"), TEXT("http_rejected"), !FMMOSupabaseConfig::Validate(C, E));
			C = SupabaseGoodConfig();
			C.BaseUrl = TEXT("https://user:pw@zzunittestref.supabase.co");
			Check(TEXT("cfg"), TEXT("userinfo_rejected"), !FMMOSupabaseConfig::Validate(C, E));
			Check(TEXT("cfg"), TEXT("valid_accepted"), FMMOSupabaseConfig::Validate(SupabaseGoodConfig(), E));

			C = SupabaseGoodConfig();
			C.SecretKey.Reset();
			FString CreateError;
			const TSharedPtr<FMMOSupabaseRestClient> NoKey = FMMOSupabaseRestClient::Create(MakeShared<FSupabaseFakeTransport>(), C, CreateError);
			Check(TEXT("cfg"), TEXT("create_without_key_null"), !NoKey.IsValid());
			Check(TEXT("cfg"), TEXT("create_error_set"), !CreateError.IsEmpty() && !SupabaseLeaks(CreateError));

			const FString OldUrl = FPlatformMisc::GetEnvironmentVariable(SupabaseEnvUrlName);
			const FString OldKey = FPlatformMisc::GetEnvironmentVariable(SupabaseEnvKeyName);
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvUrlName, TEXT(""));
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvKeyName, TEXT(""));
			FMMOSupabaseConfig FromEnv;
			FString EnvError;
			Check(TEXT("cfg"), TEXT("env_missing_rejected"), !FMMOSupabaseConfig::FromEnvironment(FromEnv, EnvError) && !EnvError.IsEmpty());
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvUrlName, SupabaseTestUrl);
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvKeyName, SupabaseTestKey);
			const bool bEnvOk = FMMOSupabaseConfig::FromEnvironment(FromEnv, EnvError);
			Check(TEXT("cfg"), TEXT("env_ok_valid"), bEnvOk);
			Check(TEXT("cfg"), TEXT("env_ok_values"), FromEnv.BaseUrl == SupabaseTestUrl && FromEnv.SecretKey == SupabaseTestKey);
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvUrlName, *OldUrl);
			FPlatformMisc::SetEnvironmentVar(SupabaseEnvKeyName, *OldKey);

			FString Json;
			Check(TEXT("cfg"), TEXT("fixture_serializable"), FMMOCharacterSaveJson::ToJson(SupabaseFixture(1), Json));
		}

		// ---------------- basic ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(500), 3, 0);
			const TSharedRef<FSupabaseRes> L = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("basic"), TEXT("load_ok"), L->O == SupabaseOut::Ok && L->Calls == 1);
			Check(TEXT("basic"), TEXT("load_gold"), L->D.Gold == 500);
			Check(TEXT("basic"), TEXT("load_game_thread"), L->bGame);
			Check(TEXT("basic"), TEXT("req_count"), C.Tr->Requests.Num() == 1);
			const FMMOHttpRequest Req = C.Tr->Requests.Num() > 0 ? C.Tr->Requests[0] : FMMOHttpRequest();
			Check(TEXT("basic"), TEXT("req_url"), Req.Url == FString(SupabaseTestUrl) + TEXT("/rest/v1/rpc/acquire_lease"));
			const auto HeaderOf = [&](const TCHAR* Name)
			{
				for (const FMMOHttpHeader& H : Req.Headers)
				{
					if (H.Name == Name) return H.Value;
				}
				return FString();
			};
			Check(TEXT("basic"), TEXT("hdr_apikey"), HeaderOf(TEXT("apikey")) == SupabaseTestKey);
			Check(TEXT("basic"), TEXT("hdr_auth"), HeaderOf(TEXT("Authorization")) == FString(TEXT("Bearer ")) + SupabaseTestKey);
			Check(TEXT("basic"), TEXT("hdr_profile"), HeaderOf(TEXT("Content-Profile")) == TEXT("mmo_api"));
			const TSharedPtr<FJsonObject> Body = SupabaseParse(Req.Body);
			Check(TEXT("basic"), TEXT("body_key"), Body.IsValid() && Body->GetStringField(TEXT("p_key")) == TEXT("ali"));
			Check(TEXT("basic"), TEXT("body_holder"), Body.IsValid() && Body->GetStringField(TEXT("p_holder")) == TEXT("srv-a"));
			Check(TEXT("basic"), TEXT("body_opid"), Body.IsValid() && Body->GetStringField(TEXT("p_op_id")).Len() >= 32);
			int64 Epoch = 0, Rev = 0;
			const bool bInfo = C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("basic"), TEXT("info_epoch"), bInfo && Epoch == 1);
			Check(TEXT("basic"), TEXT("info_rev"), bInfo && Rev == 3);

			const TSharedRef<FSupabaseRes> S1 = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(700));
			Check(TEXT("basic"), TEXT("save1_ok"), S1->O == SupabaseOut::Ok && S1->Calls == 1 && S1->bGame);
			Check(TEXT("basic"), TEXT("save1_server_rev"), C.Tr->Server.Records[TEXT("ali")].Revision == 4);
			const TSharedPtr<FJsonObject> B1 = C.Tr->LastBody(TEXT("save_character"));
			FString ExpText, EpochText;
			if (B1.IsValid())
			{
				B1->TryGetStringField(TEXT("p_expected_revision"), ExpText);
				B1->TryGetStringField(TEXT("p_epoch"), EpochText);
			}
			Check(TEXT("basic"), TEXT("save1_expected_rev_text"), ExpText == TEXT("3"));
			Check(TEXT("basic"), TEXT("save1_epoch_text"), EpochText == TEXT("1"));
			double SchemaV = 0;
			Check(TEXT("basic"), TEXT("save1_schema_number"), B1.IsValid() && B1->TryGetNumberField(TEXT("p_schema_version"), SchemaV) && SchemaV == 2.0);
			const TSharedRef<FSupabaseRes> S2 = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(900));
			Check(TEXT("basic"), TEXT("save2_ok"), S2->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B2 = C.Tr->LastBody(TEXT("save_character"));
			FString Exp2;
			if (B2.IsValid()) B2->TryGetStringField(TEXT("p_expected_revision"), Exp2);
			Check(TEXT("basic"), TEXT("save2_expected_rev_text"), Exp2 == TEXT("4"));
			FMMOCharacterSaveData Stored;
			FMMOCharacterSaveJson::FromJson(C.Tr->Server.Records[TEXT("ali")].DataJson, Stored);
			Check(TEXT("basic"), TEXT("save2_server_gold"), Stored.Gold == 900);
			const TSharedRef<FSupabaseRes> L2 = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			int64 Epoch2 = 0, Rev2 = 0;
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch2, Rev2);
			Check(TEXT("basic"), TEXT("reload_ok"), L2->O == SupabaseOut::Ok);
			Check(TEXT("basic"), TEXT("reload_epoch_same"), Epoch2 == 1);
			Check(TEXT("basic"), TEXT("reload_rev"), Rev2 == 5);
			Check(TEXT("basic"), TEXT("reload_gold"), L2->D.Gold == 900);
		}

		// ---------------- notfound ----------------
		{
			FSupabaseCtx C;
			const TSharedRef<FSupabaseRes> Absent = SupabaseLoad(C, TEXT("MMOSave_Nobody"));
			Check(TEXT("notfound"), TEXT("absent_notfound"), Absent->O == SupabaseOut::NotFound);

			FSupabaseCtx Down;
			for (int32 I = 0; I < 3; ++I) Down.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::NetworkError));
			const TSharedRef<FSupabaseRes> Out = SupabaseLoad(Down, TEXT("MMOSave_Nobody"));
			Check(TEXT("notfound"), TEXT("outage_unavailable"), Out->O == SupabaseOut::Unavailable);
			Check(TEXT("notfound"), TEXT("outage_not_notfound"), Out->O != SupabaseOut::NotFound);
			Check(TEXT("notfound"), TEXT("outage_three_attempts"), Down.Tr->Requests.Num() == 3);

			const TSharedRef<FSupabaseRes> Cr = SupabaseCreate(C, TEXT("MMOSave_Newbie"), SupabaseFixture(10));
			Check(TEXT("notfound"), TEXT("create_ok"), Cr->O == SupabaseOut::Ok);
			const TSharedRef<FSupabaseRes> Cr2 = SupabaseCreate(C, TEXT("MMOSave_Newbie"), SupabaseFixture(10));
			Check(TEXT("notfound"), TEXT("create_again_exists"), Cr2->O == SupabaseOut::AlreadyExists);
			Check(TEXT("notfound"), TEXT("exists_true"), SupabaseExists(C, TEXT("MMOSave_Newbie"))->bExists);
			const TSharedRef<FSupabaseRes> Ex = SupabaseExists(C, TEXT("MMOSave_Ghost"));
			Check(TEXT("notfound"), TEXT("exists_false"), Ex->O == SupabaseOut::Ok && !Ex->bExists);
			Check(TEXT("notfound"), TEXT("load_after_create_ok"), SupabaseLoad(C, TEXT("MMOSave_Newbie"))->O == SupabaseOut::Ok);
		}

		// ---------------- timeout_retry (ayni op_id, tek etki) ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::CommitThenTimeout, 8.0));
			const int32 ReqBefore = C.Tr->Requests.Num();
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("timeout_retry"), TEXT("ok"), S->O == SupabaseOut::Ok && S->Calls == 1);
			Check(TEXT("timeout_retry"), TEXT("effect_once"), C.Tr->Server.Commits == 1);
			Check(TEXT("timeout_retry"), TEXT("two_requests"), C.Tr->Requests.Num() - ReqBefore == 2);
			const FString Op1 = SupabaseParse(C.Tr->Requests[ReqBefore].Body)->GetStringField(TEXT("p_op_id"));
			const FString Op2 = SupabaseParse(C.Tr->Requests[ReqBefore + 1].Body)->GetStringField(TEXT("p_op_id"));
			Check(TEXT("timeout_retry"), TEXT("same_opid"), !Op1.IsEmpty() && Op1 == Op2);
			Check(TEXT("timeout_retry"), TEXT("same_body"), C.Tr->Requests[ReqBefore].Body == C.Tr->Requests[ReqBefore + 1].Body);
			const float Delay = C.Tr->ScheduledDelays.Num() > 0 ? C.Tr->ScheduledDelays.Last() : -1.f;
			Check(TEXT("timeout_retry"), TEXT("retry_delay_in_range"), Delay >= 0.5f && Delay <= 0.75f);
			Check(TEXT("timeout_retry"), TEXT("rev_incremented"), C.Tr->Server.Records[TEXT("ali")].Revision == 4);
			Check(TEXT("timeout_retry"), TEXT("ops_single"), C.Tr->Server.Ops.Num() == 1);
		}

		// ---------------- unknown_freeze ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::NetworkError));
			const int32 ReqBefore = C.Tr->Requests.Num();
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("freeze"), TEXT("unknown_outcome"), S->O == SupabaseOut::Unknown && S->Calls == 1);
			Check(TEXT("freeze"), TEXT("frozen_flag"), C.Store->IsSlotFrozen(TEXT("MMOSave_Ali")));
			Check(TEXT("freeze"), TEXT("no_effect"), C.Tr->Server.Commits == 0);
			Check(TEXT("freeze"), TEXT("six_requests"), C.Tr->Requests.Num() - ReqBefore == 6);
			const int32 ReqFrozen = C.Tr->Requests.Num();
			const TSharedRef<FSupabaseRes> Blocked = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(3));
			Check(TEXT("freeze"), TEXT("frozen_outcome"), Blocked->O == SupabaseOut::Frozen);
			Check(TEXT("freeze"), TEXT("frozen_no_request"), C.Tr->Requests.Num() == ReqFrozen);
			const TSharedRef<FSupabaseRes> Rec = SupabaseReconcile(C, TEXT("MMOSave_Ali"));
			Check(TEXT("freeze"), TEXT("reconcile_notfound_unknown"), Rec->O == SupabaseOut::Unknown);
			Check(TEXT("freeze"), TEXT("still_frozen"), C.Store->IsSlotFrozen(TEXT("MMOSave_Ali")));
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("freeze"), TEXT("load_unfreezes"), !C.Store->IsSlotFrozen(TEXT("MMOSave_Ali")));
			Check(TEXT("freeze"), TEXT("save_after_unfreeze_ok"), SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(4))->O == SupabaseOut::Ok);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::CommitThenTimeout, 8.0));
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("autoreconcile"), TEXT("ok"), S->O == SupabaseOut::Ok);
			Check(TEXT("autoreconcile"), TEXT("single_effect"), C.Tr->Server.Commits == 1);
			Check(TEXT("autoreconcile"), TEXT("not_frozen"), !C.Store->IsSlotFrozen(TEXT("MMOSave_Ali")));
			int64 Epoch = 0, Rev = 0;
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("autoreconcile"), TEXT("revision"), Rev == 4);
			Check(TEXT("autoreconcile"), TEXT("getop_called"), C.Tr->CountCalls(TEXT("get_op")) == 1);
		}

		// ---------------- stale revision ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.Records[TEXT("ali")].Revision = 10;
			const int32 ReqBefore = C.Tr->Requests.Num();
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("stale"), TEXT("conflict_outcome"), S->O == SupabaseOut::RevisionConflict);
			Check(TEXT("stale"), TEXT("conflict_no_commit"), C.Tr->Server.Commits == 0 && C.Tr->Requests.Num() - ReqBefore == 1);
			Check(TEXT("stale"), TEXT("conflict_server_untouched"), C.Tr->Server.Records[TEXT("ali")].Revision == 10);
			Check(TEXT("stale"), TEXT("conflict_not_frozen"), !C.Store->IsSlotFrozen(TEXT("MMOSave_Ali")));
			int64 Epoch = 0, Rev = 0;
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("stale"), TEXT("conflict_client_rev_unchanged"), Rev == 3);
		}

		// ---------------- dead epoch ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.Steal(TEXT("ali"), TEXT("srv-b"));
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("epoch"), TEXT("dead_epoch_outcome"), S->O == SupabaseOut::OwnershipLost);
			Check(TEXT("epoch"), TEXT("dead_epoch_no_commit"), C.Tr->Server.Commits == 0);
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("epoch"), TEXT("lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			const int32 ReqBefore = C.Tr->Requests.Num();
			const TSharedRef<FSupabaseRes> S2 = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(3));
			Check(TEXT("epoch"), TEXT("notleased_outcome"), S2->O == SupabaseOut::NotLeased);
			Check(TEXT("epoch"), TEXT("notleased_no_request"), C.Tr->Requests.Num() == ReqBefore);
			Check(TEXT("epoch"), TEXT("reacquire_blocked"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::OwnershipLost);
		}

		// ---------------- http failures ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::Http, 0.05, 503, TEXT("oops")));
			const TSharedRef<FSupabaseRes> R503 = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("http"), TEXT("h503_unavailable"), R503->O == SupabaseOut::Unavailable);
			Check(TEXT("http"), TEXT("h503_three_attempts"), C.Tr->Requests.Num() == 3);

			FSupabaseCtx C2;
			C2.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			C2.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::Http, 0.05, 401, TEXT("{\"message\":\"Invalid API key\"}")));
			const TSharedRef<FSupabaseRes> R401 = SupabaseLoad(C2, TEXT("MMOSave_Ali"));
			Check(TEXT("http"), TEXT("h401_unavailable"), R401->O == SupabaseOut::Unavailable);
			Check(TEXT("http"), TEXT("h401_no_retry"), C2.Tr->Requests.Num() == 1);

			FSupabaseCtx C3;
			C3.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			C3.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::Http, 0.05, 429, TEXT("slow down")));
			const TSharedRef<FSupabaseRes> R429 = SupabaseLoad(C3, TEXT("MMOSave_Ali"));
			Check(TEXT("http"), TEXT("h429_retry_ok"), R429->O == SupabaseOut::Ok);
			Check(TEXT("http"), TEXT("h429_two_requests"), C3.Tr->Requests.Num() == 2);

			FSupabaseCtx C4;
			C4.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			for (int32 I = 0; I < 3; ++I) C4.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 12.0));
			const TSharedRef<FSupabaseRes> RB = SupabaseLoad(C4, TEXT("MMOSave_Ali"));
			Check(TEXT("http"), TEXT("budget_unavailable"), RB->O == SupabaseOut::Unavailable);
			Check(TEXT("http"), TEXT("budget_two_attempts"), C4.Tr->Requests.Num() == 2);
		}

		// ---------------- leak (anahtar/URL sizmaz) ----------------
		{
			FSupabaseCtx C;
			const FString Echo = FString(TEXT("echo ")) + SupabaseTestKey + TEXT(" at ") + SupabaseTestUrl;
			FMMORpcResult Got;
			const auto CallOnce = [&]()
			{
				Got = FMMORpcResult();
				bool bDone = false;
				const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
				P->SetStringField(TEXT("p_key"), TEXT("ali"));
				C.Client->Call(TEXT("character_exists"), P, TEXT("op-leak"), [&](FMMORpcResult&& R) { Got = MoveTemp(R); bDone = true; });
				C.Tr->Pump();
				return bDone;
			};
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::Http, 0.05, 500, Echo));
			const bool bDone1 = CallOnce();
			Check(TEXT("leak"), TEXT("5xx_unknown"), bDone1 && Got.Status == EMMORpcStatus::Unknown && Got.Attempts == 3);
			Check(TEXT("leak"), TEXT("5xx_msg_clean"), !Got.Message.IsEmpty() && !SupabaseLeaks(Got.Message));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::RawBody, 0.05, 0, FString(TEXT("not json ")) + SupabaseTestKey));
			const bool bDone2 = CallOnce();
			Check(TEXT("leak"), TEXT("badjson_status"), bDone2 && Got.Status == EMMORpcStatus::Unknown);
			Check(TEXT("leak"), TEXT("badjson_error"), Got.Error == EMMORpcError::BadResponse);
			Check(TEXT("leak"), TEXT("badjson_msg_clean"), !Got.Message.IsEmpty() && !SupabaseLeaks(Got.Message));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::Http, 0.05, 401, Echo));
			const bool bDone3 = CallOnce();
			Check(TEXT("leak"), TEXT("4xx_failed"), bDone3 && Got.Status == EMMORpcStatus::Failed && Got.Attempts == 1);
			Check(TEXT("leak"), TEXT("4xx_msg_clean"), !Got.Message.IsEmpty() && !SupabaseLeaks(Got.Message));
			const FString Masked = C.Client->Redact(FString(TEXT("a")) + SupabaseTestKey + TEXT("b"));
			Check(TEXT("leak"), TEXT("redact_key_masked"), !Masked.Contains(SupabaseTestKey) && Masked.Contains(TEXT("sb_secret_****(")));
			const FString MaskedUrl = C.Client->Redact(FString(SupabaseTestUrl) + TEXT("/x"));
			Check(TEXT("leak"), TEXT("redact_url_masked"), MaskedUrl.Contains(TEXT("[supabase-url]")) && !SupabaseLeaks(MaskedUrl));
			const FString Desc = C.Client->DescribeMasked();
			Check(TEXT("leak"), TEXT("describe_masked"), !SupabaseLeaks(Desc) && Desc.Contains(TEXT("sb_secret_****(")));
		}

		// ---------------- bad json / data ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::RawBody, 0.05, 0, TEXT("{not json")));
			const TSharedRef<FSupabaseRes> Bad = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("badjson"), TEXT("badjson_unavailable"), Bad->O == SupabaseOut::Unavailable);
			Check(TEXT("badjson"), TEXT("badjson_no_retry"), C.Tr->Requests.Num() == 1);
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::RawBody, 0.05, 0, TEXT("{\"x\":1}")));
			Check(TEXT("badjson"), TEXT("nooutcome_unavailable"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);

			FSupabaseCtx G;
			G.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			G.Tr->Server.bGarbageData = true;
			Check(TEXT("badjson"), TEXT("baddata_invalid"), SupabaseLoad(G, TEXT("MMOSave_Ali"))->O == SupabaseOut::Invalid);
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("badjson"), TEXT("baddata_not_leased"), !G.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));

			FSupabaseCtx R1;
			R1.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R1.Tr->Server.RevisionOverride = MakeShared<FJsonValueString>(TEXT("12x"));
			Check(TEXT("badjson"), TEXT("badrev_text"), SupabaseLoad(R1, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			FSupabaseCtx R2;
			R2.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R2.Tr->Server.RevisionOverride = MakeShared<FJsonValueNumber>(-1.0);
			Check(TEXT("badjson"), TEXT("negrev"), SupabaseLoad(R2, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			FSupabaseCtx R3;
			R3.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R3.Tr->Server.RevisionOverride = MakeShared<FJsonValueNumber>(1.5);
			Check(TEXT("badjson"), TEXT("fracrev"), SupabaseLoad(R3, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
		}

		// ---------------- int64 metin ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 9007199254740993LL, 9007199254740994LL);
			const TSharedRef<FSupabaseRes> L = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			int64 Epoch = 0, Rev = 0;
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("int64"), TEXT("load_ok"), L->O == SupabaseOut::Ok);
			Check(TEXT("int64"), TEXT("epoch_exact"), Epoch == 9007199254740995LL);
			Check(TEXT("int64"), TEXT("rev_exact"), Rev == 9007199254740993LL);
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("int64"), TEXT("save_ok"), S->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("save_character"));
			FString EpochText, RevText;
			if (B.IsValid())
			{
				B->TryGetStringField(TEXT("p_epoch"), EpochText);
				B->TryGetStringField(TEXT("p_expected_revision"), RevText);
			}
			Check(TEXT("int64"), TEXT("body_epoch_text"), EpochText == TEXT("9007199254740995"));
			Check(TEXT("int64"), TEXT("body_rev_text"), RevText == TEXT("9007199254740993"));
			Check(TEXT("int64"), TEXT("server_rev"), C.Tr->Server.Records[TEXT("ali")].Revision == 9007199254740994LL);
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("int64"), TEXT("client_rev"), Rev == 9007199254740994LL);

			FSupabaseCtx N;
			N.Tr->Server.bEmitNumbers = true;
			N.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 7, 2);
			const TSharedRef<FSupabaseRes> NL = SupabaseLoad(N, TEXT("MMOSave_Ali"));
			int64 NE = 0, NR = 0;
			N.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), NE, NR);
			Check(TEXT("int64"), TEXT("numbers_ok"), NL->O == SupabaseOut::Ok);
			Check(TEXT("int64"), TEXT("numbers_values"), NE == 3 && NR == 7);
		}

		// ---------------- names ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali_veli"), SupabaseFixture(1), 1, 0);
			const TSharedRef<FSupabaseRes> Cn = SupabaseLoad(C, TEXT("MMOSave_Ali Veli"));
			Check(TEXT("names"), TEXT("canonical_ok"), Cn->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("acquire_lease"));
			Check(TEXT("names"), TEXT("canonical_body"), B.IsValid() && B->GetStringField(TEXT("p_key")) == TEXT("ali_veli"));
			const int32 ReqBefore = C.Tr->Requests.Num();
			Check(TEXT("names"), TEXT("invalid_nonascii"), SupabaseLoad(C, *SupabaseNonAsciiSlot())->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_punct_not_trimmed"), SupabaseLoad(C, TEXT("MMOSave_Ali!"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_reserved_player"), SupabaseLoad(C, TEXT("MMOSave_Player123"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_reserved_default"), SupabaseLoad(C, TEXT("MMOSave_Default"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_too_long"), SupabaseLoad(C, *(FString(TEXT("MMOSave_")) + FString::ChrN(33, TEXT('a'))))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_empty"), SupabaseLoad(C, TEXT("MMOSave_"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_slash"), SupabaseLoad(C, TEXT("MMOSave_a/b"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_no_requests"), C.Tr->Requests.Num() == ReqBefore);
			Check(TEXT("names"), TEXT("save_invalid"), SupabaseSave(C, *SupabaseNonAsciiSlot(), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("create_invalid"), SupabaseCreate(C, *SupabaseNonAsciiSlot(), SupabaseFixture(1))->O == SupabaseOut::Invalid);
		}

		// ---------------- thread / iptal ----------------
		{
			FSupabaseCtx C;
			C.Tr->bDeliverOnWorker = true;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			const TSharedRef<FSupabaseRes> L = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("thread"), TEXT("worker_load_ok"), L->O == SupabaseOut::Ok && L->Calls == 1);
			Check(TEXT("thread"), TEXT("worker_callback_game_thread"), L->bGame);

			FSupabaseCtx K;
			bool bCalled = false;
			const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("p_key"), TEXT("ali"));
			const uint64 Id = K.Client->Call(TEXT("character_exists"), P, TEXT("op-cancel"), [&](FMMORpcResult&&) { bCalled = true; });
			K.Client->Cancel(Id);
			K.Tr->Pump();
			Check(TEXT("thread"), TEXT("cancel_no_callback"), !bCalled);
			Check(TEXT("thread"), TEXT("cancel_cleaned"), K.Client->NumInFlight() == 0);
		}

		// ---------------- sync kapali-basarisiz ----------------
		{
			FSupabaseCtx C;
			FMMOCharacterSaveData D;
			Check(TEXT("sync"), TEXT("load_false"), !C.Store->Load(TEXT("MMOSave_Ali"), D));
			Check(TEXT("sync"), TEXT("save_false"), !C.Store->Save(TEXT("MMOSave_Ali"), SupabaseFixture(1)));
			Check(TEXT("sync"), TEXT("exists_false"), !C.Store->Exists(TEXT("MMOSave_Ali")));
			TArray<FString> Names;
			Names.Add(TEXT("x"));
			C.Store->ListSlotNames(Names);
			Check(TEXT("sync"), TEXT("list_empty"), Names.IsEmpty());
			bool bSuccess = true;
			C.Store->LoadAsync(TEXT("MMOSave_Ali"), [&](bool bOK, FMMOCharacterSaveData&&) { bSuccess = bOK; });
			Check(TEXT("sync"), TEXT("loadasync_false"), !bSuccess);
			Check(TEXT("sync"), TEXT("no_requests"), C.Tr->Requests.Num() == 0);
		}

		// ---------------- yazim birlestirme ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(0), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			const TSharedRef<FSupabaseRes> A = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(1), false);
			const TSharedRef<FSupabaseRes> B = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2), false);
			const TSharedRef<FSupabaseRes> D = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(3), false);
			C.Tr->Pump();
			Check(TEXT("coalesce"), TEXT("all_ok"), A->O == SupabaseOut::Ok && B->O == SupabaseOut::Ok && D->O == SupabaseOut::Ok
				&& A->Calls == 1 && B->Calls == 1 && D->Calls == 1);
			Check(TEXT("coalesce"), TEXT("two_saves"), C.Tr->CountCalls(TEXT("save_character")) == 2);
			FMMOCharacterSaveData Stored;
			FMMOCharacterSaveJson::FromJson(C.Tr->Server.Records[TEXT("ali")].DataJson, Stored);
			Check(TEXT("coalesce"), TEXT("server_gold_last"), Stored.Gold == 3);
			Check(TEXT("coalesce"), TEXT("commits_two"), C.Tr->Server.Commits == 2);
			Check(TEXT("coalesce"), TEXT("rev_five"), C.Tr->Server.Records[TEXT("ali")].Revision == 5);
		}

		// ---------------- log (tum senaryolar boyunca) ----------------
		{
			FString Captured;
			{
				FScopeLock Guard(&Cap.Lock);
				Captured = Cap.All;
			}
			Check(TEXT("log"), TEXT("no_key"), !Captured.Contains(SupabaseTestKey, ESearchCase::CaseSensitive));
			Check(TEXT("log"), TEXT("no_host"), !Captured.Contains(SupabaseTestHost, ESearchCase::IgnoreCase));
			Check(TEXT("log"), TEXT("has_key_mask"), Captured.Contains(TEXT("sb_secret_****(")));
			Check(TEXT("log"), TEXT("has_url_mask"), Captured.Contains(TEXT("[supabase-url]")));
			Check(TEXT("log"), TEXT("has_rpc_lines"), Captured.Contains(TEXT("rpc POST")));
		}

		if (OutChecks != SupabaseTestExpectedChecks)
		{
			++Failures;
			OutError += TEXT(" check_count_mismatch");
		}
		if (Failures)
		{
			OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		}
		return Failures == 0;
	}

	struct FMMOSupabaseStoreTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSupabaseStore"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = SupabaseRunSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_SUPABASE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSupabaseStoreTestExec GMMOSupabaseStoreTestExec;
}
#endif

// Kapsam notu: sahte ulasim ve sahte sunucu tek senkron karede (Pump) calisir; gercek Supabase, TLS, FHttpModule
// (FMMOUnrealHttpTransport), gercek PostgREST davranisi, zamanlayici (FTSTicker) ve gercek kira SURESI DEGILDIR.
// Sahte sunucu mmo_api sozlesmesinin (DESIGN_SUPABASE 3.3) bu dilimin kullandigi kismini modeller; gercek SQL ile
// uyum Dilim 1 vektorleri + Dilim 6 entegrasyon betigine kalir.
