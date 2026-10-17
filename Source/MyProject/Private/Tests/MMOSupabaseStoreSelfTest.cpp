// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSupabaseCharacterStore.h"

#if !UE_BUILD_SHIPPING
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Core/MMONameRules.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Http.h"
#include "Internationalization/Regex.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Supabase karakter deposu native testi (docs/DESIGN_SUPABASE.md 4, Dilim 2, sozlesme 2).
// TEK DOGRULUK KAYNAGI: <ProjectDir>/supabase/tests/vectors.json. Sahte sunucu o dosyadan TUREtilir:
//  - transport/rpc_params: listede olmayan, eksik zorunlu ya da yanlis tipli parametre = HTTP 404 PGRST202;
//  - responses: her yanit sablonun anahtar kumesiyle birebir uretilir (bigint = JSON SAYI, lease_epoch, found);
//  - scenarios: 19 senaryo adim adim RPC istemcisinden gecer (istek -> beklenen yanit);
//  - canonical_key / holder: UE SlotToKey / IsValidHolder dogrulamasi vektorlere gore.
// Dosya yoksa test FAIL olur (sessizce atlanmaz). Gercek ag, gercek Supabase, diske yazma YOK.
namespace
{
	using SupabaseOut = EMMOSupabaseStoreOutcome;
	using FSupabaseCheck = TFunctionRef<void(const TCHAR*, const TCHAR*, bool)>;

	// vectors.json degisince (senaryo/vektor sayisi) test BILEREK kirilir; yeni sayi gozden gecirilip burada ve
	// Scripts/TestSupabaseStore.ps1'de birlikte guncellenir.
	constexpr int32 SupabaseTestExpectedChecks = 449;
	const TCHAR* SupabaseTestKey = TEXT("sb_secret_TESTKEYDONOTLEAK0123456789abcdef");
	const TCHAR* SupabaseTestHost = TEXT("zzunittestref.supabase.co");
	const TCHAR* SupabaseTestUrl = TEXT("https://zzunittestref.supabase.co");
	const TCHAR* SupabaseEnvUrlName = TEXT("MMO_SUPABASE_URL");
	const TCHAR* SupabaseEnvKeyName = TEXT("MMO_SUPABASE_SECRET_KEY");

	// ---- JSON yardimcilari ----
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

	/** Tek bir degerin siki metni (benzersiz parmak izi icin). */
	FString SupabaseValueText(const TSharedPtr<FJsonValue>& V)
	{
		const TSharedRef<FJsonObject> W = MakeShared<FJsonObject>();
		W->SetField(TEXT("v"), V.IsValid() ? V : MakeShared<FJsonValueNull>());
		return SupabaseText(W);
	}

	TSharedPtr<FJsonValue> SupabaseCloneValue(const TSharedPtr<FJsonValue>& V)
	{
		const TSharedPtr<FJsonObject> O = SupabaseParse(SupabaseValueText(V));
		return O.IsValid() ? O->Values.FindRef(TEXT("v")) : TSharedPtr<FJsonValue>();
	}

	TSharedRef<FJsonObject> SupabaseCloneObject(const TSharedRef<FJsonObject>& Obj)
	{
		const TSharedPtr<FJsonObject> Copy = SupabaseParse(SupabaseText(Obj));
		return Copy.IsValid() ? Copy.ToSharedRef() : MakeShared<FJsonObject>();
	}

	bool SupabaseJsonEq(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid() == B.IsValid();
		}
		if (A->Type != B->Type)
		{
			return false;
		}
		switch (A->Type)
		{
		case EJson::Null: return true;
		case EJson::String: return A->AsString() == B->AsString();
		case EJson::Number: return A->AsNumber() == B->AsNumber();
		case EJson::Boolean: return A->AsBool() == B->AsBool();
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& X = A->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& Y = B->AsArray();
			if (X.Num() != Y.Num())
			{
				return false;
			}
			for (int32 I = 0; I < X.Num(); ++I)
			{
				if (!SupabaseJsonEq(X[I], Y[I])) return false;
			}
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> X = A->AsObject();
			const TSharedPtr<FJsonObject> Y = B->AsObject();
			if (!X.IsValid() || !Y.IsValid() || X->Values.Num() != Y->Values.Num())
			{
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : X->Values)
			{
				const TSharedPtr<FJsonValue>* O = Y->Values.Find(P.Key);
				if (!O || !SupabaseJsonEq(P.Value, *O)) return false;
			}
			return true;
		}
		default: return false;
		}
	}

	/** jsonb @> (kapsama): nesneler anahtar anahtar, diziler "her beklenen eleman bir gercek elemanda". */
	bool SupabaseJsonContains(const TSharedPtr<FJsonValue>& Actual, const TSharedPtr<FJsonValue>& Expect)
	{
		if (!Actual.IsValid() || !Expect.IsValid())
		{
			return false;
		}
		if (Expect->Type == EJson::Object)
		{
			if (Actual->Type != EJson::Object) return false;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Expect->AsObject()->Values)
			{
				const TSharedPtr<FJsonValue>* A = Actual->AsObject()->Values.Find(P.Key);
				if (!A || !SupabaseJsonContains(*A, P.Value)) return false;
			}
			return true;
		}
		if (Expect->Type == EJson::Array)
		{
			if (Actual->Type != EJson::Array) return false;
			for (const TSharedPtr<FJsonValue>& E : Expect->AsArray())
			{
				bool bAny = false;
				for (const TSharedPtr<FJsonValue>& A : Actual->AsArray())
				{
					if (SupabaseJsonContains(A, E)) { bAny = true; break; }
				}
				if (!bAny) return false;
			}
			return true;
		}
		return SupabaseJsonEq(Actual, Expect);
	}

	// ---- vectors.json ----
	struct FSupabaseVectors
	{
		TSharedPtr<FJsonObject> Root;
		FString Error;

		bool Load()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("supabase/tests/vectors.json")));
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				Error = TEXT("vectors.json okunamadi (<ProjectDir>/supabase/tests/vectors.json)");
				return false;
			}
			Root = SupabaseParse(Text);
			if (!Root.IsValid())
			{
				Error = TEXT("vectors.json gecerli JSON degil");
				return false;
			}
			return true;
		}

		TSharedPtr<FJsonObject> Obj(const TCHAR* Field) const
		{
			const TSharedPtr<FJsonObject>* P = nullptr;
			return (Root.IsValid() && Root->TryGetObjectField(Field, P) && P) ? *P : TSharedPtr<FJsonObject>();
		}

		TArray<TSharedPtr<FJsonValue>> Arr(const TCHAR* Field) const
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			return (Root.IsValid() && Root->TryGetArrayField(Field, A) && A) ? *A : TArray<TSharedPtr<FJsonValue>>();
		}

		FString Limit(const TCHAR* Name) const
		{
			FString S;
			if (const TSharedPtr<FJsonObject> L = Obj(TEXT("limits"))) L->TryGetStringField(Name, S);
			return S;
		}

		int64 LimitInt(const TCHAR* Name) const
		{
			double D = 0;
			if (const TSharedPtr<FJsonObject> L = Obj(TEXT("limits"))) L->TryGetNumberField(Name, D);
			return static_cast<int64>(D);
		}

		FString Transport(const TCHAR* Name) const
		{
			FString S;
			if (const TSharedPtr<FJsonObject> T = Obj(TEXT("transport"))) T->TryGetStringField(Name, S);
			return S;
		}

		int32 TransportInt(const TCHAR* Name) const
		{
			double D = 0;
			if (const TSharedPtr<FJsonObject> T = Obj(TEXT("transport"))) T->TryGetNumberField(Name, D);
			return static_cast<int32>(D);
		}
	};

	const FSupabaseVectors* SupabaseGVectors = nullptr;

	/** Sablondaki '@isim' degerlerini fixtures[isim] ile degistirir (derin kopya). */
	TSharedPtr<FJsonValue> SupabaseResolveFixtures(const TSharedPtr<FJsonValue>& V, const TSharedPtr<FJsonObject>& Fixtures)
	{
		if (!V.IsValid())
		{
			return V;
		}
		if (V->Type == EJson::String)
		{
			const FString S = V->AsString();
			if (S.StartsWith(TEXT("@")) && Fixtures.IsValid())
			{
				const TSharedPtr<FJsonValue>* F = Fixtures->Values.Find(S.RightChop(1));
				return F ? SupabaseCloneValue(*F) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());
			}
			return V;
		}
		if (V->Type == EJson::Object)
		{
			const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : V->AsObject()->Values)
			{
				Out->SetField(P.Key, SupabaseResolveFixtures(P.Value, Fixtures));
			}
			return MakeShared<FJsonValueObject>(Out);
		}
		if (V->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			for (const TSharedPtr<FJsonValue>& E : V->AsArray())
			{
				Items.Add(SupabaseResolveFixtures(E, Fixtures));
			}
			return MakeShared<FJsonValueArray>(Items);
		}
		return V;
	}

	class FSupabaseRegex
	{
	public:
		explicit FSupabaseRegex(const FString& Pattern) : Compiled(Anchor(Pattern)) {}
		bool Matches(const FString& Text) const
		{
			FRegexMatcher Matcher(Compiled, Text);
			return Matcher.FindNext();
		}

	private:
		// ICU '$' sondaki satir sonundan once de eslesir; PostgreSQL ~ eslesmez: '\z' (girdi sonu) kullanilir.
		static FString Anchor(FString Pattern)
		{
			if (Pattern.EndsWith(TEXT("$")))
			{
				Pattern.LeftChopInline(1);
				Pattern += TEXT("\\z");
			}
			return Pattern;
		}
		FRegexPattern Compiled;
	};

	// ---- sahte sunucu (vectors.json'dan turetilmis) ----
	struct FSupabaseRecord
	{
		FString Key;
		FString DisplayName;
		FString Race;
		FString CharacterId;
		int32 SchemaVersion = 2;
		int64 Revision = 0;
		int64 Epoch = 0;
		TSharedPtr<FJsonObject> Data;
		TMap<FString, TSharedPtr<FJsonValue>> Side;
		FString Holder; // bos = birakilmis
		bool bLive = false;
		FString ImportSha;

		FString DataText() const { return Data.IsValid() ? SupabaseText(Data.ToSharedRef()) : FString(); }
	};

	struct FSupabaseOpRecord
	{
		FString Kind;
		FString Key;
		FString Digest;
		TSharedPtr<FJsonObject> Result;
	};

	class FSupabaseFakeServer
	{
	public:
		TMap<FString, FSupabaseRecord> Records;
		TMap<FString, FSupabaseOpRecord> Ops;
		int32 Commits = 0;
		int32 Requests = 0;
		int32 ParamViolations = 0;    // sozlesme disi istek (rpc_params ile uyumsuz)
		int32 TemplateViolations = 0; // sunucunun kendi yaniti sablonla uyumsuz (sahte sunucu hatasi)
		FString LastErrorCode;

		// Sozlesme DISI yanit uretmek icin test sapmalari (sablon denetiminden SONRA uygulanir).
		bool bGarbageData = false;
		bool bLegacyEpochOnly = false; // yalniz eski `epoch` (lease_epoch yok)
		TSharedPtr<FJsonValue> RevisionOverride;
		FString GetOpKindOverride;

		FSupabaseFakeServer()
			: KeyRe(SupabaseGVectors ? SupabaseGVectors->Limit(TEXT("key_regex")) : FString())
			, NameRe(SupabaseGVectors ? SupabaseGVectors->Limit(TEXT("name_regex")) : FString())
			, HolderRe(SupabaseGVectors ? SupabaseGVectors->Limit(TEXT("holder_regex")) : FString())
			, PrefixRe(TEXT("^[a-z0-9_-]{0,32}$"))
			, ShaRe(TEXT("^[0-9a-f]{64}$"))
			, RaceRe(TEXT("^[A-Za-z0-9_-]{1,32}$"))
		{
		}

		void Seed(const FString& Key, const FMMOCharacterSaveData& Data, int64 Revision, int64 Epoch)
		{
			FSupabaseRecord R;
			R.Key = Key;
			R.DisplayName = Key;
			R.CharacterId = NextCharacterId();
			FString Json;
			FMMOCharacterSaveJson::ToJson(Data, Json);
			R.Data = SupabaseParse(Json);
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
				R->bLive = true;
			}
		}

		bool ExpireLease(const FString& Key)
		{
			if (FSupabaseRecord* R = Records.Find(Key))
			{
				R->bLive = false;
				return true;
			}
			return false;
		}

		/** rpc_params: bilinen fonksiyon, bilinen parametreler, zorunlular var, JSON tipleri uyumlu. */
		bool CheckParams(const FString& Fn, const FJsonObject& B) const
		{
			const TSharedPtr<FJsonObject> Params = SupabaseGVectors ? SupabaseGVectors->Obj(TEXT("rpc_params")) : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!Params.IsValid() || !Params->TryGetArrayField(Fn, List) || !List)
			{
				return false;
			}
			TSet<FString> Known;
			for (const TSharedPtr<FJsonValue>& Entry : *List)
			{
				const TSharedPtr<FJsonObject> Spec = Entry->AsObject();
				FString Name, JsonType, PgType;
				bool bRequired = false;
				Spec->TryGetStringField(TEXT("name"), Name);
				Spec->TryGetStringField(TEXT("json_type"), JsonType);
				Spec->TryGetStringField(TEXT("pg_type"), PgType);
				Spec->TryGetBoolField(TEXT("required"), bRequired);
				Known.Add(Name);
				const TSharedPtr<FJsonValue>* V = B.Values.Find(Name);
				if (!V && bRequired)
				{
					return false;
				}
				if (V && !TypeOk(*V, JsonType, PgType))
				{
					return false;
				}
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : B.Values)
			{
				if (!Known.Contains(P.Key))
				{
					return false;
				}
			}
			return true;
		}

		FString Handle(const FString& Fn, const TSharedPtr<FJsonObject>& B, int32& OutHttp)
		{
			++Requests;
			OutHttp = 200;
			if (!B.IsValid())
			{
				OutHttp = 400;
				return TEXT("{}");
			}
			if (!CheckParams(Fn, *B))
			{
				++ParamViolations;
				OutHttp = SupabaseGVectors->TransportInt(TEXT("param_mismatch_status"));
				LastErrorCode = SupabaseGVectors->Transport(TEXT("param_mismatch_code"));
				const TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), LastErrorCode);
				Err->SetStringField(TEXT("message"), TEXT("Could not find the function with the supplied parameters"));
				return SupabaseText(Err);
			}
			return SupabaseText(Dispatch(Fn, *B));
		}

	private:
		FSupabaseRegex KeyRe;
		FSupabaseRegex NameRe;
		FSupabaseRegex HolderRe;
		FSupabaseRegex PrefixRe;
		FSupabaseRegex ShaRe;
		FSupabaseRegex RaceRe;
		int32 CharCounter = 0;

		static bool IsUuid(const FString& S)
		{
			if (S.Len() != 36) return false;
			for (int32 I = 0; I < 36; ++I)
			{
				const TCHAR C = S[I];
				if (I == 8 || I == 13 || I == 18 || I == 23)
				{
					if (C != TEXT('-')) return false;
				}
				else if (!FChar::IsHexDigit(C))
				{
					return false;
				}
			}
			return true;
		}

		static bool TypeOk(const TSharedPtr<FJsonValue>& V, const FString& JsonType, const FString& PgType)
		{
			if (!V.IsValid() || V->Type == EJson::Null)
			{
				return true; // SQL: NULL gecer; is kurali (null_argument vb.) isleyicide
			}
			if (JsonType == TEXT("string"))
			{
				return V->Type == EJson::String && (PgType != TEXT("uuid") || IsUuid(V->AsString()));
			}
			if (JsonType == TEXT("number"))
			{
				if (V->Type != EJson::Number) return false;
				const double N = V->AsNumber();
				if (N != FMath::FloorToDouble(N)) return false;
				return PgType != TEXT("integer") || (N >= -2147483648.0 && N <= 2147483647.0);
			}
			if (JsonType == TEXT("object"))
			{
				return V->Type == EJson::Object;
			}
			return false;
		}

		FString NextCharacterId()
		{
			return FString::Printf(TEXT("00000000-0000-4000-8000-%012d"), ++CharCounter);
		}

		static bool Present(const FJsonObject& B, const TCHAR* N) { return B.Values.Contains(N); }
		static bool IsNull(const FJsonObject& B, const TCHAR* N)
		{
			const TSharedPtr<FJsonValue>* V = B.Values.Find(N);
			return !V || !V->IsValid() || (*V)->Type == EJson::Null;
		}
		static FString Str(const FJsonObject& B, const TCHAR* N)
		{
			FString S;
			B.TryGetStringField(N, S);
			return S;
		}
		static int64 I64(const FJsonObject& B, const TCHAR* N)
		{
			double D = 0;
			B.TryGetNumberField(N, D);
			return static_cast<int64>(D);
		}
		static void Num(FJsonObject& O, const TCHAR* K, int64 V) { O.SetNumberField(K, static_cast<double>(V)); }
		static FString ValText(const FJsonObject& B, const TCHAR* N)
		{
			const TSharedPtr<FJsonValue>* V = B.Values.Find(N);
			return SupabaseValueText(V ? *V : TSharedPtr<FJsonValue>());
		}

		/** Yaniti sablondan uretir; anahtar kumesi sablonla birebir olmali (aksi sahte sunucu hatasi). */
		TSharedRef<FJsonObject> Build(const TCHAR* Fn, const TCHAR* Key, const TFunctionRef<void(FJsonObject&)>& Fill)
		{
			const TSharedPtr<FJsonObject> Responses = SupabaseGVectors ? SupabaseGVectors->Obj(TEXT("responses")) : nullptr;
			const TSharedPtr<FJsonObject>* PerFn = nullptr;
			const TSharedPtr<FJsonObject>* Tmpl = nullptr;
			if (!Responses.IsValid() || !Responses->TryGetObjectField(Fn, PerFn) || !PerFn || !(*PerFn)->TryGetObjectField(Key, Tmpl) || !Tmpl)
			{
				++TemplateViolations;
				const TSharedRef<FJsonObject> Fallback = MakeShared<FJsonObject>();
				Fallback->SetStringField(TEXT("outcome"), Key);
				return Fallback;
			}
			const TSharedRef<FJsonObject> Out = SupabaseCloneObject((*Tmpl).ToSharedRef());
			Fill(*Out);
			bool bSame = Out->Values.Num() == (*Tmpl)->Values.Num();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Tmpl)->Values)
			{
				bSame = bSame && Out->Values.Contains(P.Key);
			}
			if (!bSame)
			{
				++TemplateViolations;
			}
			return Out;
		}

		TSharedRef<FJsonObject> Plain(const TCHAR* Fn, const TCHAR* Key)
		{
			return Build(Fn, Key, [](FJsonObject&) {});
		}

		TSharedRef<FJsonObject> Invalid(const TCHAR* Fn, const TCHAR* Reason)
		{
			return Build(Fn, TEXT("invalid"), [Reason](FJsonObject& O) { O.SetStringField(TEXT("reason"), Reason); });
		}

		TSharedRef<FJsonObject> Lost(const TCHAR* Fn, const FSupabaseRecord& R)
		{
			return Build(Fn, TEXT("ownership_lost"), [&R](FJsonObject& O)
			{
				Num(O, TEXT("lease_epoch"), R.Epoch);
				Num(O, TEXT("epoch"), R.Epoch);
			});
		}

		bool DbCanonical(const FString& Name, FString& Out) const
		{
			if (!NameRe.Matches(Name))
			{
				return false;
			}
			Out = Name.Replace(TEXT(" "), TEXT("_")).ToLower();
			return true;
		}

		static bool IsReservedKey(const FString& Key)
		{
			if (Key == TEXT("default")) return true;
			if (!Key.StartsWith(TEXT("player")) || Key.Len() <= 6) return false;
			for (int32 I = 6; I < Key.Len(); ++I)
			{
				if (Key[I] < TEXT('0') || Key[I] > TEXT('9')) return false;
			}
			return true;
		}

		const TCHAR* NameProblem(const FJsonObject& B) const
		{
			if (IsNull(B, TEXT("p_key")) || IsNull(B, TEXT("p_display_name"))) return TEXT("bad_name");
			FString Canon;
			if (!DbCanonical(Str(B, TEXT("p_display_name")), Canon)) return TEXT("bad_name");
			if (Canon != Str(B, TEXT("p_key"))) return TEXT("key_mismatch");
			if (IsReservedKey(Str(B, TEXT("p_key")))) return TEXT("reserved_name");
			return nullptr;
		}

		static bool IntInRange(const TSharedPtr<FJsonValue>& V, double Lo, double Hi)
		{
			if (!V.IsValid() || V->Type != EJson::Number) return false;
			const double N = V->AsNumber();
			return N >= Lo && N <= Hi && N == FMath::FloorToDouble(N);
		}

		const TCHAR* EnvelopeProblem(const FJsonObject& B, int64 SchemaVersion) const
		{
			const TSharedPtr<FJsonValue>* DV = B.Values.Find(TEXT("p_data"));
			if (!DV || !DV->IsValid() || (*DV)->Type != EJson::Object) return TEXT("data_not_object");
			const TSharedPtr<FJsonObject> D = (*DV)->AsObject();
			if (SupabaseText(D.ToSharedRef()).Len() > SupabaseGVectors->LimitInt(TEXT("data_max_octets"))) return TEXT("data_too_large");
			const TSharedPtr<FJsonValue>* Sv = D->Values.Find(TEXT("SchemaVersion"));
			if (!Sv || !IntInRange(*Sv, static_cast<double>(SchemaVersion), static_cast<double>(SchemaVersion))) return TEXT("schema_version_mismatch");
			const TSharedPtr<FJsonValue>* Inner = D->Values.Find(TEXT("Data"));
			if (!Inner || !Inner->IsValid() || (*Inner)->Type != EJson::Object) return TEXT("data_missing");
			const TSharedPtr<FJsonObject> In = (*Inner)->AsObject();
			if (!IntInRange(In->Values.FindRef(TEXT("Level")), 1, 999)) return TEXT("bad_level");
			if (!IntInRange(In->Values.FindRef(TEXT("Experience")), 0, 2147483647.0)) return TEXT("bad_experience");
			if (!IntInRange(In->Values.FindRef(TEXT("Gold")), 0, 2147483647.0)) return TEXT("bad_gold");
			if (!IntInRange(In->Values.FindRef(TEXT("NextInstanceId")), 1, 2147483647.0)) return TEXT("bad_next_instance_id");
			return nullptr;
		}

		const TCHAR* SideProblem(const FJsonObject& B) const
		{
			if (IsNull(B, TEXT("p_side"))) return nullptr;
			const TSharedPtr<FJsonObject> S = B.Values.FindRef(TEXT("p_side"))->AsObject();
			if (!S.IsValid()) return TEXT("side_not_object");
			TArray<FString> Kinds;
			for (const TSharedPtr<FJsonValue>& K : SupabaseGVectors->Obj(TEXT("limits"))->GetArrayField(TEXT("side_kinds")))
			{
				Kinds.Add(K->AsString());
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : S->Values)
			{
				if (!Kinds.Contains(P.Key)) return TEXT("side_unknown_kind");
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : S->Values)
			{
				if (!P.Value.IsValid() || P.Value->Type != EJson::Object
					|| SupabaseValueText(P.Value).Len() > SupabaseGVectors->LimitInt(TEXT("side_value_max_octets"))) return TEXT("side_bad_value");
			}
			return nullptr;
		}

		void FillRecord(FJsonObject& O, const FSupabaseRecord& R) const
		{
			O.SetStringField(TEXT("canonical_key"), R.Key);
			O.SetStringField(TEXT("character_id"), R.CharacterId);
			O.SetStringField(TEXT("display_name"), R.DisplayName);
			if (R.Race.IsEmpty()) O.SetField(TEXT("race"), MakeShared<FJsonValueNull>()); else O.SetStringField(TEXT("race"), R.Race);
			Num(O, TEXT("schema_version"), R.SchemaVersion);
			Num(O, TEXT("revision"), R.Revision);
			O.SetObjectField(TEXT("data"), R.Data.IsValid() ? SupabaseCloneObject(R.Data.ToSharedRef()) : MakeShared<FJsonObject>());
			const TSharedRef<FJsonObject> Side = MakeShared<FJsonObject>();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : R.Side)
			{
				Side->SetField(P.Key, SupabaseCloneValue(P.Value));
			}
			O.SetObjectField(TEXT("side"), Side);
			Num(O, TEXT("lease_epoch"), R.Epoch);
			O.SetStringField(TEXT("lease_until"), TEXT("2026-10-17T12:02:00.000000+00:00"));
		}

		TSharedRef<FJsonObject> Dispatch(const FString& Fn, const FJsonObject& B)
		{
			if (Fn == TEXT("healthcheck")) return Plain(TEXT("healthcheck"), TEXT("ok"));
			if (Fn == TEXT("load_character")) return LoadCharacter(B);
			if (Fn == TEXT("character_exists")) return CharacterExists(B);
			if (Fn == TEXT("list_keys")) return ListKeys(B);
			if (Fn == TEXT("create_character")) return CreateCharacter(B);
			if (Fn == TEXT("acquire_lease")) return AcquireLease(B);
			if (Fn == TEXT("renew_lease")) return RenewLease(B);
			if (Fn == TEXT("release_lease")) return ReleaseLease(B);
			if (Fn == TEXT("save_character")) return SaveCharacter(B);
			if (Fn == TEXT("get_op")) return GetOp(B);
			if (Fn == TEXT("import_character")) return ImportCharacter(B);
			if (Fn == TEXT("purge_write_ops")) return PurgeWriteOps(B);
			++TemplateViolations;
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> LoadCharacter(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("load_character");
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const FSupabaseRecord* R = Records.Find(Str(B, TEXT("p_key")));
			if (!R) return Plain(Fn, TEXT("not_found"));
			return Build(Fn, TEXT("ok"), [this, R](FJsonObject& O)
			{
				FillRecord(O, *R);
				if (R->Holder.IsEmpty()) O.SetField(TEXT("active_server"), MakeShared<FJsonValueNull>()); else O.SetStringField(TEXT("active_server"), R->Holder);
			});
		}

		TSharedRef<FJsonObject> CharacterExists(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("character_exists");
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const bool bExists = Records.Contains(Str(B, TEXT("p_key")));
			return Build(Fn, TEXT("ok"), [bExists](FJsonObject& O) { O.SetBoolField(TEXT("exists"), bExists); });
		}

		TSharedRef<FJsonObject> ListKeys(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("list_keys");
			const FString Prefix = IsNull(B, TEXT("p_prefix")) ? FString() : Str(B, TEXT("p_prefix"));
			if (Present(B, TEXT("p_limit")) && IsNull(B, TEXT("p_limit"))) return Invalid(Fn, TEXT("bad_limit"));
			const int64 Limit = Present(B, TEXT("p_limit")) ? I64(B, TEXT("p_limit")) : SupabaseGVectors->LimitInt(TEXT("list_keys_default"));
			if (Limit < 1 || Limit > SupabaseGVectors->LimitInt(TEXT("list_keys_max"))) return Invalid(Fn, TEXT("bad_limit"));
			if (!PrefixRe.Matches(Prefix)) return Invalid(Fn, TEXT("bad_prefix"));
			TArray<FString> Keys;
			for (const TPair<FString, FSupabaseRecord>& P : Records)
			{
				if (P.Key.StartsWith(Prefix, ESearchCase::CaseSensitive)) Keys.Add(P.Key);
			}
			Keys.Sort();
			const bool bTruncated = Keys.Num() > Limit;
			if (bTruncated) Keys.SetNum(static_cast<int32>(Limit));
			return Build(Fn, TEXT("ok"), [&Keys, bTruncated](FJsonObject& O)
			{
				TArray<TSharedPtr<FJsonValue>> Items;
				for (const FString& K : Keys) Items.Add(MakeShared<FJsonValueString>(K));
				O.SetArrayField(TEXT("keys"), Items);
				O.SetBoolField(TEXT("truncated"), bTruncated);
			});
		}

		TSharedRef<FJsonObject> CreateCharacter(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("create_character");
			if (IsNull(B, TEXT("p_op_id")) || IsNull(B, TEXT("p_holder")) || IsNull(B, TEXT("p_schema_version"))) return Invalid(Fn, TEXT("null_argument"));
			if (!HolderRe.Matches(Str(B, TEXT("p_holder")))) return Invalid(Fn, TEXT("bad_holder"));
			if (!IsNull(B, TEXT("p_race")) && !RaceRe.Matches(Str(B, TEXT("p_race")))) return Invalid(Fn, TEXT("bad_race"));
			if (const TCHAR* Problem = NameProblem(B)) return Invalid(Fn, Problem);
			if (const TCHAR* Problem = EnvelopeProblem(B, I64(B, TEXT("p_schema_version")))) return Invalid(Fn, Problem);

			const FString Key = Str(B, TEXT("p_key"));
			const FString OpId = Str(B, TEXT("p_op_id"));
			const FString Digest = TEXT("create|") + ValText(B, TEXT("p_key")) + ValText(B, TEXT("p_display_name")) + ValText(B, TEXT("p_holder"))
				+ ValText(B, TEXT("p_schema_version")) + ValText(B, TEXT("p_race")) + ValText(B, TEXT("p_data"));
			if (const FSupabaseOpRecord* Prev = Ops.Find(OpId))
			{
				if (Prev->Kind == TEXT("create") && Prev->Key == Key && Prev->Digest == Digest)
				{
					const TSharedRef<FJsonObject> Replay = SupabaseCloneObject(Prev->Result.ToSharedRef());
					Replay->SetBoolField(TEXT("replayed"), true);
					return Replay;
				}
				return Plain(Fn, TEXT("idempotency_conflict"));
			}
			if (Records.Contains(Key)) return Plain(Fn, TEXT("already_exists"));

			FSupabaseRecord R;
			R.Key = Key;
			R.DisplayName = Str(B, TEXT("p_display_name"));
			R.Race = IsNull(B, TEXT("p_race")) ? FString() : Str(B, TEXT("p_race"));
			R.CharacterId = NextCharacterId();
			R.SchemaVersion = static_cast<int32>(I64(B, TEXT("p_schema_version")));
			R.Data = SupabaseCloneObject(B.Values.FindRef(TEXT("p_data"))->AsObject().ToSharedRef());
			R.Revision = 0;
			R.Epoch = 1;
			R.Holder = Str(B, TEXT("p_holder"));
			R.bLive = true;
			Records.Add(Key, R);
			++Commits;
			const TSharedRef<FJsonObject> Result = Build(Fn, TEXT("committed"), [&Key](FJsonObject& O)
			{
				O.SetStringField(TEXT("canonical_key"), Key);
				Num(O, TEXT("revision"), 0);
				Num(O, TEXT("lease_epoch"), 1);
				Num(O, TEXT("epoch"), 1);
			});
			Ops.Add(OpId, {TEXT("create"), Key, Digest, SupabaseCloneObject(Result)});
			return Result;
		}

		TSharedRef<FJsonObject> AcquireLease(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("acquire_lease");
			if (IsNull(B, TEXT("p_holder")) || (Present(B, TEXT("p_ttl_s")) && IsNull(B, TEXT("p_ttl_s")))) return Invalid(Fn, TEXT("null_argument"));
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const FString Holder = Str(B, TEXT("p_holder"));
			if (!HolderRe.Matches(Holder)) return Invalid(Fn, TEXT("bad_holder"));
			const int64 Ttl = Present(B, TEXT("p_ttl_s")) ? I64(B, TEXT("p_ttl_s")) : SupabaseGVectors->LimitInt(TEXT("lease_ttl_default_s"));
			if (Ttl < SupabaseGVectors->LimitInt(TEXT("lease_ttl_min_s")) || Ttl > SupabaseGVectors->LimitInt(TEXT("lease_ttl_max_s"))) return Invalid(Fn, TEXT("bad_ttl"));
			FSupabaseRecord* R = Records.Find(Str(B, TEXT("p_key")));
			if (!R) return Plain(Fn, TEXT("not_found"));
			if (!R->Holder.IsEmpty() && R->bLive)
			{
				if (R->Holder != Holder)
				{
					return Build(Fn, TEXT("ownership_lost"), [](FJsonObject& O) { Num(O, TEXT("retry_after_s"), 87); });
				}
			}
			else
			{
				++R->Epoch;
			}
			R->Holder = Holder;
			R->bLive = true;
			const TSharedRef<FJsonObject> Out = Build(Fn, TEXT("ok"), [this, R](FJsonObject& O)
			{
				FillRecord(O, *R);
				Num(O, TEXT("epoch"), R->Epoch);
			});
			if (bGarbageData)
			{
				const TSharedRef<FJsonObject> Junk = MakeShared<FJsonObject>();
				Junk->SetStringField(TEXT("junk"), TEXT("x"));
				Out->SetObjectField(TEXT("data"), Junk);
			}
			if (RevisionOverride.IsValid()) Out->SetField(TEXT("revision"), RevisionOverride);
			if (bLegacyEpochOnly) Out->RemoveField(TEXT("lease_epoch"));
			return Out;
		}

		TSharedRef<FJsonObject> RenewLease(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("renew_lease");
			if (IsNull(B, TEXT("p_holder")) || IsNull(B, TEXT("p_epoch"))) return Invalid(Fn, TEXT("null_argument"));
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const FString Holder = Str(B, TEXT("p_holder"));
			if (!HolderRe.Matches(Holder)) return Invalid(Fn, TEXT("bad_holder"));
			if (I64(B, TEXT("p_epoch")) < 1) return Invalid(Fn, TEXT("bad_epoch"));
			if (!IsNull(B, TEXT("p_ttl_s")))
			{
				const int64 Ttl = I64(B, TEXT("p_ttl_s"));
				if (Ttl < SupabaseGVectors->LimitInt(TEXT("lease_ttl_min_s")) || Ttl > SupabaseGVectors->LimitInt(TEXT("lease_ttl_max_s"))) return Invalid(Fn, TEXT("bad_ttl"));
			}
			const FSupabaseRecord* R = Records.Find(Str(B, TEXT("p_key")));
			if (!R) return Plain(Fn, TEXT("not_found"));
			if (R->Holder != Holder || R->Epoch != I64(B, TEXT("p_epoch")) || !R->bLive) return Lost(Fn, *R);
			return Build(Fn, TEXT("ok"), [R](FJsonObject& O)
			{
				Num(O, TEXT("lease_epoch"), R->Epoch);
				Num(O, TEXT("epoch"), R->Epoch);
			});
		}

		TSharedRef<FJsonObject> ReleaseLease(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("release_lease");
			if (IsNull(B, TEXT("p_holder")) || IsNull(B, TEXT("p_epoch"))) return Invalid(Fn, TEXT("null_argument"));
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const FString Holder = Str(B, TEXT("p_holder"));
			if (!HolderRe.Matches(Holder)) return Invalid(Fn, TEXT("bad_holder"));
			const int64 Epoch = I64(B, TEXT("p_epoch"));
			if (Epoch < 1) return Invalid(Fn, TEXT("bad_epoch"));
			FSupabaseRecord* R = Records.Find(Str(B, TEXT("p_key")));
			if (!R) return Plain(Fn, TEXT("not_found"));
			const bool bAlready = R->Holder.IsEmpty() && R->Epoch == Epoch;
			if (!bAlready && (R->Holder != Holder || R->Epoch != Epoch)) return Lost(Fn, *R);
			R->Holder.Reset();
			R->bLive = false;
			return Build(Fn, TEXT("ok"), [R, bAlready](FJsonObject& O)
			{
				O.SetBoolField(TEXT("already_released"), bAlready);
				Num(O, TEXT("lease_epoch"), R->Epoch);
				Num(O, TEXT("epoch"), R->Epoch);
			});
		}

		TSharedRef<FJsonObject> SaveCharacter(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("save_character");
			if (IsNull(B, TEXT("p_op_id")) || IsNull(B, TEXT("p_holder")) || IsNull(B, TEXT("p_epoch")) || IsNull(B, TEXT("p_expected_revision"))
				|| IsNull(B, TEXT("p_schema_version"))) return Invalid(Fn, TEXT("null_argument"));
			if (!KeyRe.Matches(Str(B, TEXT("p_key")))) return Invalid(Fn, TEXT("bad_key"));
			const FString Holder = Str(B, TEXT("p_holder"));
			if (!HolderRe.Matches(Holder)) return Invalid(Fn, TEXT("bad_holder"));
			const int64 Epoch = I64(B, TEXT("p_epoch"));
			const int64 Expected = I64(B, TEXT("p_expected_revision"));
			if (Epoch < 1 || Expected < 0) return Invalid(Fn, TEXT("bad_epoch_or_revision"));
			if (const TCHAR* Problem = EnvelopeProblem(B, I64(B, TEXT("p_schema_version")))) return Invalid(Fn, Problem);
			if (const TCHAR* Problem = SideProblem(B)) return Invalid(Fn, Problem);

			const FString Key = Str(B, TEXT("p_key"));
			const FString OpId = Str(B, TEXT("p_op_id"));
			const FString Digest = TEXT("save|") + ValText(B, TEXT("p_key")) + ValText(B, TEXT("p_holder")) + ValText(B, TEXT("p_epoch"))
				+ ValText(B, TEXT("p_expected_revision")) + ValText(B, TEXT("p_schema_version")) + ValText(B, TEXT("p_data")) + ValText(B, TEXT("p_side"));
			if (const FSupabaseOpRecord* Prev = Ops.Find(OpId))
			{
				// Kayitli islem: kira/revision'a BAKMADAN kayitli sonuc.
				if (Prev->Kind == TEXT("save") && Prev->Key == Key && Prev->Digest == Digest)
				{
					const TSharedRef<FJsonObject> Replay = SupabaseCloneObject(Prev->Result.ToSharedRef());
					Replay->SetBoolField(TEXT("replayed"), true);
					return Replay;
				}
				return Plain(Fn, TEXT("idempotency_conflict"));
			}
			FSupabaseRecord* R = Records.Find(Key);
			if (!R) return Plain(Fn, TEXT("not_found"));
			if (R->Holder != Holder || R->Epoch != Epoch || !R->bLive) return Lost(Fn, *R);
			if (R->Revision != Expected)
			{
				return Build(Fn, TEXT("revision_conflict"), [R](FJsonObject& O) { Num(O, TEXT("revision"), R->Revision); });
			}
			R->Data = SupabaseCloneObject(B.Values.FindRef(TEXT("p_data"))->AsObject().ToSharedRef());
			R->SchemaVersion = static_cast<int32>(I64(B, TEXT("p_schema_version")));
			++R->Revision;
			if (!IsNull(B, TEXT("p_side")))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : B.Values.FindRef(TEXT("p_side"))->AsObject()->Values)
				{
					R->Side.Add(P.Key, SupabaseCloneValue(P.Value));
				}
			}
			++Commits;
			const TSharedRef<FJsonObject> Result = Build(Fn, TEXT("committed"), [R](FJsonObject& O)
			{
				Num(O, TEXT("revision"), R->Revision);
				Num(O, TEXT("lease_epoch"), R->Epoch);
				Num(O, TEXT("epoch"), R->Epoch);
			});
			Ops.Add(OpId, {TEXT("save"), Key, Digest, SupabaseCloneObject(Result)});
			return Result;
		}

		TSharedRef<FJsonObject> GetOp(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("get_op");
			if (IsNull(B, TEXT("p_op_id"))) return Invalid(Fn, TEXT("null_argument"));
			const FSupabaseOpRecord* Op = Ops.Find(Str(B, TEXT("p_op_id")));
			if (!Op) return Plain(Fn, TEXT("not_found"));
			const TSharedRef<FJsonObject> Out = Build(Fn, TEXT("ok"), [Op](FJsonObject& O)
			{
				O.SetStringField(TEXT("op_kind"), Op->Kind);
				O.SetStringField(TEXT("canonical_key"), Op->Key);
				O.SetObjectField(TEXT("result"), SupabaseCloneObject(Op->Result.ToSharedRef()));
				O.SetStringField(TEXT("created_at"), TEXT("2026-10-17T12:00:00.000000+00:00"));
			});
			if (!GetOpKindOverride.IsEmpty()) Out->SetStringField(TEXT("op_kind"), GetOpKindOverride);
			return Out;
		}

		TSharedRef<FJsonObject> ImportCharacter(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("import_character");
			if (IsNull(B, TEXT("p_source_sha256")) || IsNull(B, TEXT("p_schema_version"))) return Invalid(Fn, TEXT("null_argument"));
			const FString Sha = Str(B, TEXT("p_source_sha256"));
			if (!ShaRe.Matches(Sha)) return Invalid(Fn, TEXT("bad_sha"));
			if (!IsNull(B, TEXT("p_race")) && !RaceRe.Matches(Str(B, TEXT("p_race")))) return Invalid(Fn, TEXT("bad_race"));
			if (const TCHAR* Problem = NameProblem(B)) return Invalid(Fn, Problem);
			if (const TCHAR* Problem = EnvelopeProblem(B, I64(B, TEXT("p_schema_version")))) return Invalid(Fn, Problem);

			const FString Key = Str(B, TEXT("p_key"));
			const TSharedRef<FJsonObject> NewData = SupabaseCloneObject(B.Values.FindRef(TEXT("p_data"))->AsObject().ToSharedRef());
			FSupabaseRecord* R = Records.Find(Key);
			if (!R)
			{
				FSupabaseRecord N;
				N.Key = Key;
				N.DisplayName = Str(B, TEXT("p_display_name"));
				N.Race = IsNull(B, TEXT("p_race")) ? FString() : Str(B, TEXT("p_race"));
				N.CharacterId = NextCharacterId();
				N.SchemaVersion = static_cast<int32>(I64(B, TEXT("p_schema_version")));
				N.Data = NewData;
				N.ImportSha = Sha;
				Records.Add(Key, N);
				return Build(Fn, TEXT("imported"), [](FJsonObject& O)
				{
					Num(O, TEXT("revision"), 0);
					O.SetBoolField(TEXT("replaced_import"), false);
				});
			}
			if (R->ImportSha == Sha)
			{
				return Build(Fn, TEXT("already_imported"), [R](FJsonObject& O) { Num(O, TEXT("revision"), R->Revision); });
			}
			if (!R->ImportSha.IsEmpty() && R->Revision == 0 && (R->Holder.IsEmpty() || !R->bLive))
			{
				R->DisplayName = Str(B, TEXT("p_display_name"));
				R->Race = IsNull(B, TEXT("p_race")) ? FString() : Str(B, TEXT("p_race"));
				R->SchemaVersion = static_cast<int32>(I64(B, TEXT("p_schema_version")));
				R->Data = NewData;
				R->ImportSha = Sha;
				return Build(Fn, TEXT("imported"), [](FJsonObject& O)
				{
					Num(O, TEXT("revision"), 0);
					O.SetBoolField(TEXT("replaced_import"), true);
				});
			}
			return Build(Fn, TEXT("conflict"), [R](FJsonObject& O) { Num(O, TEXT("revision"), R->Revision); });
		}

		TSharedRef<FJsonObject> PurgeWriteOps(const FJsonObject& B)
		{
			const TCHAR* Fn = TEXT("purge_write_ops");
			if (Present(B, TEXT("p_older_than_days")) && IsNull(B, TEXT("p_older_than_days"))) return Invalid(Fn, TEXT("bad_days"));
			const int64 Days = Present(B, TEXT("p_older_than_days")) ? I64(B, TEXT("p_older_than_days")) : SupabaseGVectors->LimitInt(TEXT("purge_default_days"));
			if (Days < SupabaseGVectors->LimitInt(TEXT("purge_min_days")) || Days > SupabaseGVectors->LimitInt(TEXT("purge_max_days"))) return Invalid(Fn, TEXT("bad_days"));
			return Build(Fn, TEXT("ok"), [](FJsonObject& O)
			{
				Num(O, TEXT("deleted"), 0); // sahte sunucuda saat ilerlemez: hicbir kayit "eski" degil
				O.SetBoolField(TEXT("more"), false);
			});
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
		FString Snapshot()
		{
			GLog->FlushThreadedLogs(); // satirlar gunluk is parcaciginda gecikmeli gelebilir; kilit DISINDA bosalt
			FScopeLock Guard(&Lock);
			return All;
		}
	};

	struct FSupabaseLogScope
	{
		FSupabaseLogCapture& Cap;
		ELogVerbosity::Type PrevMMO;
		ELogVerbosity::Type PrevHttp;
		explicit FSupabaseLogScope(FSupabaseLogCapture& InCap)
			: Cap(InCap), PrevMMO(LogMMOSupabase.GetVerbosity()), PrevHttp(LogHttp.GetVerbosity())
		{
			LogMMOSupabase.SetVerbosity(ELogVerbosity::VeryVerbose);
			GLog->AddOutputDevice(&Cap);
		}
		~FSupabaseLogScope()
		{
			GLog->RemoveOutputDevice(&Cap);
			LogMMOSupabase.SetVerbosity(PrevMMO);
			LogHttp.SetVerbosity(PrevHttp);
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

	// Depo testlerinde sunucunun gordugu sozlesme ihlalleri toplanir (hepsi 0 olmali).
	int32 SupabaseGViolations = 0;
	int32 SupabaseGRequests = 0;

	struct FSupabaseCtx
	{
		TSharedRef<FSupabaseFakeTransport> Tr = MakeShared<FSupabaseFakeTransport>();
		TSharedPtr<FMMOSupabaseRestClient> Client;
		TUniquePtr<FMMOSupabaseCharacterStore> Store;
		bool bCountViolations = true;

		explicit FSupabaseCtx(const TCHAR* Holder = TEXT("srv-a"))
		{
			FString Error;
			Client = FMMOSupabaseRestClient::Create(Tr, SupabaseGoodConfig(), Error);
			if (Client.IsValid())
			{
				Store = MakeUnique<FMMOSupabaseCharacterStore>(Client.ToSharedRef(), Holder);
			}
		}

		~FSupabaseCtx()
		{
			SupabaseGRequests += Tr->Server.Requests;
			if (bCountViolations)
			{
				SupabaseGViolations += Tr->Server.ParamViolations + Tr->Server.TemplateViolations;
			}
		}

		TUniquePtr<FMMOSupabaseCharacterStore> NewStore(const TCHAR* Holder) const
		{
			return MakeUnique<FMMOSupabaseCharacterStore>(Client.ToSharedRef(), Holder);
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

	TSharedRef<FSupabaseRes> SupabaseLoad(FSupabaseCtx& C, const TCHAR* Slot, FMMOSupabaseCharacterStore* Other = nullptr)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		(Other ? *Other : *C.Store).LoadEx(Slot, [R](SupabaseOut O, FMMOCharacterSaveData&& D)
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

	TSharedRef<FSupabaseRes> SupabaseRenew(FSupabaseCtx& C, const TCHAR* Slot)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->RenewLeaseEx(Slot, [R](SupabaseOut O) { ++R->Calls; R->O = O; });
		C.Tr->Pump();
		return R;
	}

	TSharedRef<FSupabaseRes> SupabaseRelease(FSupabaseCtx& C, const TCHAR* Slot, bool bPump = true)
	{
		const TSharedRef<FSupabaseRes> R = MakeShared<FSupabaseRes>();
		C.Store->ReleaseLeaseEx(Slot, [R](SupabaseOut O) { ++R->Calls; R->O = O; });
		if (bPump)
		{
			C.Tr->Pump();
		}
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

	bool SupabaseHasField(const TSharedPtr<FJsonObject>& O, const TCHAR* Name)
	{
		return O.IsValid() && O->HasField(Name);
	}

	double SupabaseNumberOf(const TSharedPtr<FJsonObject>& O, const TCHAR* Name)
	{
		double D = -1.0;
		if (O.IsValid())
		{
			O->TryGetNumberField(Name, D);
		}
		return D;
	}

	// ---- vektor tabanli denetimler ----

	/** rpc_params: sahte sunucunun parametre denetimi (eksik/fazla/yanlis tip) ve p_op_id kumesi. */
	void SupabaseCheckRpcParams(const FSupabaseVectors& V, const FSupabaseCheck& Check)
	{
		const TSharedPtr<FJsonObject> Params = V.Obj(TEXT("rpc_params"));
		TSet<FString> WithOpId;
		FSupabaseFakeServer Server;
		const auto Sample = [](const FString& JsonType, const FString& PgType) -> TSharedPtr<FJsonValue>
		{
			if (JsonType == TEXT("string")) return MakeShared<FJsonValueString>(PgType == TEXT("uuid") ? TEXT("00000000-0000-4000-8000-000000000099") : TEXT("x"));
			if (JsonType == TEXT("number")) return MakeShared<FJsonValueNumber>(1.0);
			return MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
		};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Fn : Params->Values)
		{
			const TArray<TSharedPtr<FJsonValue>> List = Fn.Value->AsArray();
			TArray<FString> Names, JsonTypes, PgTypes;
			TArray<bool> Required;
			for (const TSharedPtr<FJsonValue>& E : List)
			{
				const TSharedPtr<FJsonObject> Spec = E->AsObject();
				FString N, J, P;
				bool bReq = false;
				Spec->TryGetStringField(TEXT("name"), N);
				Spec->TryGetStringField(TEXT("json_type"), J);
				Spec->TryGetStringField(TEXT("pg_type"), P);
				Spec->TryGetBoolField(TEXT("required"), bReq);
				Names.Add(N);
				JsonTypes.Add(J);
				PgTypes.Add(P);
				Required.Add(bReq);
				if (N == TEXT("p_op_id")) WithOpId.Add(Fn.Key);
			}
			bool bOk = true;
			const TSharedRef<FJsonObject> Full = MakeShared<FJsonObject>();
			const TSharedRef<FJsonObject> ReqOnly = MakeShared<FJsonObject>();
			for (int32 I = 0; I < Names.Num(); ++I)
			{
				Full->SetField(Names[I], Sample(JsonTypes[I], PgTypes[I]));
				if (Required[I]) ReqOnly->SetField(Names[I], Sample(JsonTypes[I], PgTypes[I]));
			}
			bOk = bOk && Server.CheckParams(Fn.Key, *Full) && Server.CheckParams(Fn.Key, *ReqOnly);
			for (int32 I = 0; I < Names.Num(); ++I)
			{
				if (Required[I])
				{
					const TSharedRef<FJsonObject> Missing = SupabaseCloneObject(Full);
					Missing->RemoveField(Names[I]);
					bOk = bOk && !Server.CheckParams(Fn.Key, *Missing);
				}
				const TSharedRef<FJsonObject> Wrong = SupabaseCloneObject(Full);
				if (JsonTypes[I] == TEXT("string") && PgTypes[I] == TEXT("uuid")) Wrong->SetStringField(Names[I], TEXT("not-a-uuid"));
				else if (JsonTypes[I] == TEXT("string")) Wrong->SetNumberField(Names[I], 1.0);
				else Wrong->SetStringField(Names[I], TEXT("1"));
				bOk = bOk && !Server.CheckParams(Fn.Key, *Wrong);
			}
			const TSharedRef<FJsonObject> Extra = SupabaseCloneObject(Full);
			Extra->SetStringField(TEXT("p_unknown_extra"), TEXT("x"));
			bOk = bOk && !Server.CheckParams(Fn.Key, *Extra);
			Check(TEXT("rpcparams"), *Fn.Key, bOk);
		}
		Check(TEXT("rpcparams"), TEXT("function_count"), Params->Values.Num() == 12);
		Check(TEXT("rpcparams"), TEXT("op_id_only_create_save_get_op"),
			WithOpId.Num() == 3 && WithOpId.Contains(TEXT("create_character")) && WithOpId.Contains(TEXT("save_character")) && WithOpId.Contains(TEXT("get_op")));
		Check(TEXT("rpcparams"), TEXT("unknown_function_rejected"), !Server.CheckParams(TEXT("drop_everything"), FJsonObject()));
	}

	/** canonical_key[] ve holder[] vektorleri: UE SlotToKey / IsValidHolder / MMONameRules ve sunucu regex'i. */
	void SupabaseCheckNameVectors(const FSupabaseVectors& V, const FSupabaseCheck& Check)
	{
		int32 Index = 0;
		for (const TSharedPtr<FJsonValue>& Entry : V.Arr(TEXT("canonical_key")))
		{
			const TSharedPtr<FJsonObject> E = Entry->AsObject();
			FString Name, UeKey, DbKey;
			bool bUeReserved = false, bDbReserved = false;
			E->TryGetStringField(TEXT("name"), Name);
			E->TryGetStringField(TEXT("ue_canonical_key"), UeKey);
			E->TryGetBoolField(TEXT("ue_is_reserved"), bUeReserved);
			const bool bDbNull = !E->HasTypedField<EJson::String>(TEXT("db_key"));
			E->TryGetStringField(TEXT("db_key"), DbKey);
			E->TryGetBoolField(TEXT("db_reserved"), bDbReserved);

			bool bOk = MMONameRules::CanonicalKey(Name) == UeKey && MMONameRules::IsReservedName(Name) == bUeReserved;
			FString Key1, Disp1, Key2, Disp2;
			const bool bSlot = FMMOSupabaseCharacterStore::SlotToKey(Name, Key1, Disp1);
			const bool bSlotPrefixed = FMMOSupabaseCharacterStore::SlotToKey(FString(TEXT("MMOSave_")) + Name, Key2, Disp2);
			if (bDbNull)
			{
				bOk = bOk && !bSlot && !bSlotPrefixed; // DB reddeder: UE de gondermez (kirpip baska ada cevirmez)
			}
			else
			{
				// DB gecerli: kanonik anahtar ayni; ayrilmis adlari UE yine de depo yuvasi olarak reddeder.
				bOk = bOk && UeKey == DbKey && bSlot == !bDbReserved && bSlotPrefixed == bSlot;
				if (bSlot) bOk = bOk && Key1 == DbKey && Key2 == DbKey && Disp1 == Name;
			}
			Check(TEXT("canonical"), *FString::Printf(TEXT("i%02d"), Index++), bOk);
		}
		Index = 0;
		for (const TSharedPtr<FJsonValue>& Entry : V.Arr(TEXT("holder")))
		{
			const TSharedPtr<FJsonObject> E = Entry->AsObject();
			FString Value;
			bool bValid = false;
			E->TryGetStringField(TEXT("value"), Value);
			E->TryGetBoolField(TEXT("valid"), bValid);
			Check(TEXT("holder"), *FString::Printf(TEXT("i%02d"), Index++), FMMOSupabaseCharacterStore::IsValidHolder(Value) == bValid);
		}
	}

	/** scenarios[]: her adim RPC istemcisinden gecer; sahte sunucu yaniti vektorun beklentisini saglamali. */
	void SupabaseRunScenarios(const FSupabaseVectors& V, const FSupabaseCheck& Check)
	{
		FSupabaseCtx C;
		C.bCountViolations = false; // param_contract senaryosu BILEREK ihlal gonderir
		const TSharedPtr<FJsonObject> Fixtures = V.Obj(TEXT("fixtures"));
		const TArray<TSharedPtr<FJsonValue>> Scenarios = V.Arr(TEXT("scenarios"));
		int32 ExpectedErrors = 0;
		for (const TSharedPtr<FJsonValue>& ScenarioValue : Scenarios)
		{
			const TSharedPtr<FJsonObject> Scenario = ScenarioValue->AsObject();
			FString Id;
			Scenario->TryGetStringField(TEXT("id"), Id);
			const FString Case = TEXT("scn_") + Id;
			const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
			Scenario->TryGetArrayField(TEXT("steps"), Steps);
			int32 StepIndex = 0;
			for (const TSharedPtr<FJsonValue>& StepValue : Steps ? *Steps : TArray<TSharedPtr<FJsonValue>>())
			{
				const TSharedPtr<FJsonObject> Step = StepValue->AsObject();
				FString Admin;
				if (Step->TryGetStringField(TEXT("admin"), Admin))
				{
					FString Key;
					Step->TryGetStringField(TEXT("key"), Key);
					const bool bDone = Admin == TEXT("expire_lease") && C.Tr->Server.ExpireLease(Key);
					Check(*Case, *FString::Printf(TEXT("s%02d_admin"), StepIndex++), bDone);
					continue;
				}
				FString Rpc;
				Step->TryGetStringField(TEXT("rpc"), Rpc);
				const TSharedPtr<FJsonValue> ArgsValue = SupabaseResolveFixtures(Step->Values.FindRef(TEXT("args")), Fixtures);
				const TSharedRef<FJsonObject> Args = ArgsValue.IsValid() && ArgsValue->Type == EJson::Object ? ArgsValue->AsObject().ToSharedRef() : MakeShared<FJsonObject>();
				FMMORpcResult Got;
				bool bDone = false;
				C.Client->Call(Rpc, Args, TEXT("scenario"), [&](FMMORpcResult&& R) { Got = MoveTemp(R); bDone = true; });
				C.Tr->Pump();

				bool bOk = bDone;
				if (const TSharedPtr<FJsonObject>* ErrorSpec = nullptr; Step->TryGetObjectField(TEXT("expect_error"), ErrorSpec) && ErrorSpec)
				{
					++ExpectedErrors;
					double Http = 0;
					FString Code;
					(*ErrorSpec)->TryGetNumberField(TEXT("http_status"), Http);
					(*ErrorSpec)->TryGetStringField(TEXT("code"), Code);
					bOk = bOk && Got.Status == EMMORpcStatus::Failed && Got.HttpCode == static_cast<int32>(Http) && C.Tr->Server.LastErrorCode == Code;
				}
				else
				{
					bool bExact = false;
					Step->TryGetBoolField(TEXT("exact"), bExact);
					const TSharedPtr<FJsonValue> Expect = SupabaseResolveFixtures(Step->Values.FindRef(TEXT("expect")), Fixtures);
					const TSharedPtr<FJsonValue> Actual = Got.Json.IsValid() ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueObject>(Got.Json)) : nullptr;
					bOk = bOk && Got.Status == EMMORpcStatus::Ok
						&& (bExact ? SupabaseJsonEq(Actual, Expect) : SupabaseJsonContains(Actual, Expect));
				}
				Check(*Case, *FString::Printf(TEXT("s%02d_%s"), StepIndex++, *Rpc), bOk);
			}
		}
		Check(TEXT("scenarios"), TEXT("count_19"), Scenarios.Num() == 19);
		Check(TEXT("scenarios"), TEXT("no_template_violations"), C.Tr->Server.TemplateViolations == 0);
		Check(TEXT("scenarios"), TEXT("param_violations_match_expected_errors"), ExpectedErrors > 0 && C.Tr->Server.ParamViolations == ExpectedErrors);
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

		// ---------------- contract (vectors.json) ----------------
		FSupabaseVectors Vectors;
		const bool bLoaded = Vectors.Load();
		Check(TEXT("contract"), TEXT("vectors_loaded"), bLoaded);
		if (!bLoaded)
		{
			OutError = Vectors.Error;
			return false; // dosya yoksa devam etmek anlamsiz: sessizce atlama yok, FAIL
		}
		SupabaseGVectors = &Vectors;
		struct FClearVectors { ~FClearVectors() { SupabaseGVectors = nullptr; } } ClearVectors;
		SupabaseGViolations = 0;
		SupabaseGRequests = 0;

		double Version = -1.0;
		Vectors.Root->TryGetNumberField(TEXT("contract_version"), Version);
		Check(TEXT("contract"), TEXT("version_matches_code"), static_cast<int32>(Version) == FMMOSupabaseRestClient::ContractVersion);
		Check(TEXT("contract"), TEXT("health_template_version"), [&]()
		{
			const TSharedPtr<FJsonObject> Resp = Vectors.Obj(TEXT("responses"));
			const TSharedPtr<FJsonObject>* H = nullptr;
			const TSharedPtr<FJsonObject>* Ok = nullptr;
			double Cv = -1.0;
			return Resp.IsValid() && Resp->TryGetObjectField(TEXT("healthcheck"), H) && H && (*H)->TryGetObjectField(TEXT("ok"), Ok) && Ok
				&& (*Ok)->TryGetNumberField(TEXT("contract_version"), Cv) && static_cast<int32>(Cv) == FMMOSupabaseRestClient::ContractVersion;
		}());
		SupabaseCheckRpcParams(Vectors, Check);
		SupabaseCheckNameVectors(Vectors, Check);
		SupabaseRunScenarios(Vectors, Check);

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
			Check(TEXT("basic"), TEXT("req_method_from_vectors"), Req.Verb == Vectors.Transport(TEXT("method")));
			const auto HeaderOf = [&](const TCHAR* Name)
			{
				for (const FMMOHttpHeader& H : Req.Headers)
				{
					if (H.Name == Name) return H.Value;
				}
				return FString();
			};
			// vectors.transport.headers: "Ad: deger" (anahtar yer tutucusu <secret>).
			bool bHeadersOk = true;
			for (const TSharedPtr<FJsonValue>& HV : Vectors.Obj(TEXT("transport"))->GetArrayField(TEXT("headers")))
			{
				FString Name, Value;
				if (!HV->AsString().Split(TEXT(": "), &Name, &Value)) { bHeadersOk = false; continue; }
				const FString Actual = HeaderOf(*Name);
				if (Value == TEXT("<secret>")) bHeadersOk = bHeadersOk && Actual == SupabaseTestKey;
				else if (Value == TEXT("Bearer <secret>")) bHeadersOk = bHeadersOk && Actual == FString(TEXT("Bearer ")) + SupabaseTestKey;
				else bHeadersOk = bHeadersOk && Actual == Value;
			}
			Check(TEXT("basic"), TEXT("hdr_from_vectors"), bHeadersOk);
			const TSharedPtr<FJsonObject> Body = SupabaseParse(Req.Body);
			Check(TEXT("basic"), TEXT("body_key"), Body.IsValid() && Body->GetStringField(TEXT("p_key")) == TEXT("ali"));
			Check(TEXT("basic"), TEXT("body_holder"), Body.IsValid() && Body->GetStringField(TEXT("p_holder")) == TEXT("srv-a"));
			Check(TEXT("basic"), TEXT("body_no_opid_on_acquire"), Body.IsValid() && !Body->HasField(TEXT("p_op_id")));
			Check(TEXT("basic"), TEXT("body_ttl_number"), SupabaseNumberOf(Body, TEXT("p_ttl_s")) == 120.0);
			int64 Epoch = 0, Rev = 0;
			const bool bInfo = C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("basic"), TEXT("info_epoch"), bInfo && Epoch == 1);
			Check(TEXT("basic"), TEXT("info_rev"), bInfo && Rev == 3);

			const TSharedRef<FSupabaseRes> S1 = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(700));
			Check(TEXT("basic"), TEXT("save1_ok"), S1->O == SupabaseOut::Ok && S1->Calls == 1 && S1->bGame);
			Check(TEXT("basic"), TEXT("save1_server_rev"), C.Tr->Server.Records[TEXT("ali")].Revision == 4);
			const TSharedPtr<FJsonObject> B1 = C.Tr->LastBody(TEXT("save_character"));
			Check(TEXT("basic"), TEXT("save1_expected_rev_number"), B1.IsValid() && B1->HasTypedField<EJson::Number>(TEXT("p_expected_revision")) && SupabaseNumberOf(B1, TEXT("p_expected_revision")) == 3.0);
			Check(TEXT("basic"), TEXT("save1_epoch_number"), B1.IsValid() && B1->HasTypedField<EJson::Number>(TEXT("p_epoch")) && SupabaseNumberOf(B1, TEXT("p_epoch")) == 1.0);
			Check(TEXT("basic"), TEXT("save1_schema_number"), SupabaseNumberOf(B1, TEXT("p_schema_version")) == 2.0);
			Check(TEXT("basic"), TEXT("save1_has_opid"), B1.IsValid() && B1->GetStringField(TEXT("p_op_id")).Len() >= 32);
			const TSharedRef<FSupabaseRes> S2 = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(900));
			Check(TEXT("basic"), TEXT("save2_ok"), S2->O == SupabaseOut::Ok);
			Check(TEXT("basic"), TEXT("save2_expected_rev_number"), SupabaseNumberOf(C.Tr->LastBody(TEXT("save_character")), TEXT("p_expected_revision")) == 4.0);
			FMMOCharacterSaveData Stored;
			FMMOCharacterSaveJson::FromJson(C.Tr->Server.Records[TEXT("ali")].DataText(), Stored);
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
			const TSharedPtr<FJsonObject> CreateBody = C.Tr->LastBody(TEXT("create_character"));
			Check(TEXT("notfound"), TEXT("create_body_has_opid_and_null_race"), SupabaseHasField(CreateBody, TEXT("p_op_id")) && CreateBody->HasTypedField<EJson::Null>(TEXT("p_race")));
			const TSharedPtr<FJsonObject> ExistsBody = C.Tr->LastBody(TEXT("character_exists"));
			Check(TEXT("notfound"), TEXT("exists_body_only_key"), ExistsBody.IsValid() && ExistsBody->Values.Num() == 1 && ExistsBody->HasField(TEXT("p_key")));
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
			const TSharedPtr<FJsonObject> G = C.Tr->LastBody(TEXT("get_op"));
			Check(TEXT("autoreconcile"), TEXT("getop_body_only_opid"), G.IsValid() && G->Values.Num() == 1 && G->HasField(TEXT("p_op_id")));
		}

		// ---------------- create belirsizligi (get_op ile uzlasma) ----------------
		{
			// 1) ilk deneme commit oldu, tum yanitlar kayboldu: get_op found/create => Ok.
			FSupabaseCtx C;
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::CommitThenTimeout, 8.0));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			const TSharedRef<FSupabaseRes> Cr = SupabaseCreate(C, TEXT("MMOSave_Lostcreate"), SupabaseFixture(10));
			Check(TEXT("createamb"), TEXT("found_is_ok"), Cr->O == SupabaseOut::Ok && Cr->Calls == 1);
			Check(TEXT("createamb"), TEXT("found_not_frozen"), !C.Store->IsSlotFrozen(TEXT("MMOSave_Lostcreate")));
			Check(TEXT("createamb"), TEXT("found_single_effect"), C.Tr->Server.Commits == 1 && C.Tr->CountCalls(TEXT("get_op")) == 1);
			const TSharedPtr<FJsonObject> G = C.Tr->LastBody(TEXT("get_op"));
			const TSharedPtr<FJsonObject> CB = C.Tr->LastBody(TEXT("create_character"));
			Check(TEXT("createamb"), TEXT("getop_uses_create_opid"), G.IsValid() && CB.IsValid() && G->GetStringField(TEXT("p_op_id")) == CB->GetStringField(TEXT("p_op_id")));
		}
		{
			// 2) create hic ulasmadi: get_op not_found => Unknown + DONUK; ReconcileEx hala Unknown; gec commit sonrasi Ok.
			FSupabaseCtx C;
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			const TSharedRef<FSupabaseRes> Cr = SupabaseCreate(C, TEXT("MMOSave_Latecreate"), SupabaseFixture(10));
			Check(TEXT("createamb"), TEXT("notfound_unknown"), Cr->O == SupabaseOut::Unknown);
			Check(TEXT("createamb"), TEXT("notfound_frozen"), C.Store->IsSlotFrozen(TEXT("MMOSave_Latecreate")));
			Check(TEXT("createamb"), TEXT("notfound_not_notfound_outcome"), Cr->O != SupabaseOut::NotFound);
			Check(TEXT("createamb"), TEXT("reconcile_still_unknown"), SupabaseReconcile(C, TEXT("MMOSave_Latecreate"))->O == SupabaseOut::Unknown);
			Check(TEXT("createamb"), TEXT("reconcile_still_frozen"), C.Store->IsSlotFrozen(TEXT("MMOSave_Latecreate")));
			int32 Http = 0;
			C.Tr->Server.Handle(TEXT("create_character"), C.Tr->LastBody(TEXT("create_character")), Http); // gec gelen istek commit olur
			Check(TEXT("createamb"), TEXT("late_commit_applied"), C.Tr->Server.Commits == 1);
			Check(TEXT("createamb"), TEXT("reconcile_after_late_commit_ok"), SupabaseReconcile(C, TEXT("MMOSave_Latecreate"))->O == SupabaseOut::Ok);
			Check(TEXT("createamb"), TEXT("reconcile_unfrozen"), !C.Store->IsSlotFrozen(TEXT("MMOSave_Latecreate")));
		}
		{
			// 3) get_op baska turde (save) bir kayit dondururse create uzlasti SAYILMAZ.
			FSupabaseCtx C;
			C.Tr->Server.GetOpKindOverride = TEXT("save");
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::CommitThenTimeout, 8.0));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			const TSharedRef<FSupabaseRes> Cr = SupabaseCreate(C, TEXT("MMOSave_Wrongkind"), SupabaseFixture(10));
			Check(TEXT("createamb"), TEXT("wrong_kind_unknown"), Cr->O == SupabaseOut::Unknown && C.Store->IsSlotFrozen(TEXT("MMOSave_Wrongkind")));
		}
		{
			// 4) get_op'a da ulasilamaz: Unknown + donuk.
			FSupabaseCtx C;
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::TimeoutNoCommit, 8.0));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::NetworkError));
			const TSharedRef<FSupabaseRes> Cr = SupabaseCreate(C, TEXT("MMOSave_Blindcreate"), SupabaseFixture(10));
			Check(TEXT("createamb"), TEXT("getop_down_unknown_frozen"), Cr->O == SupabaseOut::Unknown && C.Store->IsSlotFrozen(TEXT("MMOSave_Blindcreate")));
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

		// ---------------- lease_epoch zorunlu (eski `epoch` takma adi okunmaz) ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.bLegacyEpochOnly = true;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("leaseepoch"), TEXT("legacy_epoch_only_unavailable"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			Check(TEXT("leaseepoch"), TEXT("legacy_epoch_only_not_leased"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			FSupabaseCtx G;
			G.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			Check(TEXT("leaseepoch"), TEXT("normal_has_both_fields"), SupabaseLoad(G, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
		}

		// ---------------- renew ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			const TSharedRef<FSupabaseRes> R1 = SupabaseRenew(C, TEXT("MMOSave_Ali"));
			Check(TEXT("renew"), TEXT("ok"), R1->O == SupabaseOut::Ok && R1->Calls == 1);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("renew_lease"));
			Check(TEXT("renew"), TEXT("body_exact_keys"), B.IsValid() && B->Values.Num() == 4 && B->HasField(TEXT("p_key")) && B->HasField(TEXT("p_holder"))
				&& B->HasField(TEXT("p_epoch")) && B->HasField(TEXT("p_ttl_s")) && !B->HasField(TEXT("p_op_id")));
			Check(TEXT("renew"), TEXT("body_epoch_number"), B.IsValid() && B->HasTypedField<EJson::Number>(TEXT("p_epoch")) && SupabaseNumberOf(B, TEXT("p_epoch")) == 1.0);
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("renew"), TEXT("still_leased"), C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev) && Epoch == 1);
			Check(TEXT("renew"), TEXT("save_after_renew_ok"), SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2))->O == SupabaseOut::Ok);

			C.Tr->Server.Steal(TEXT("ali"), TEXT("srv-b"));
			Check(TEXT("renew"), TEXT("stolen_ownership_lost"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::OwnershipLost);
			Check(TEXT("renew"), TEXT("stolen_lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			const int32 ReqBefore = C.Tr->Requests.Num();
			Check(TEXT("renew"), TEXT("stolen_then_save_notleased"), SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(3))->O == SupabaseOut::NotLeased);
			Check(TEXT("renew"), TEXT("stolen_then_renew_notleased"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::NotLeased);
			Check(TEXT("renew"), TEXT("notleased_no_request"), C.Tr->Requests.Num() == ReqBefore);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.ExpireLease(TEXT("ali"));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("renew"), TEXT("expired_ownership_lost"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::OwnershipLost);
			Check(TEXT("renew"), TEXT("expired_lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			Check(TEXT("renew"), TEXT("expired_reacquire_epoch_plus_one"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok
				&& C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev) && Epoch == 2);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::NetworkError));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("renew"), TEXT("outage_unavailable"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			Check(TEXT("renew"), TEXT("outage_lease_kept"), C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev) && Epoch == 1);
			Check(TEXT("renew"), TEXT("after_outage_ok"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.Records.Remove(TEXT("ali"));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("renew"), TEXT("vanished_notfound"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::NotFound);
			Check(TEXT("renew"), TEXT("vanished_lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			Check(TEXT("renew"), TEXT("invalid_name"), SupabaseRenew(C, TEXT("MMOSave_Ali!"))->O == SupabaseOut::Invalid);
		}

		// ---------------- release ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			Check(TEXT("release"), TEXT("ok"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("release_lease"));
			Check(TEXT("release"), TEXT("body_exact_keys"), B.IsValid() && B->Values.Num() == 3 && B->HasField(TEXT("p_key")) && B->HasField(TEXT("p_holder"))
				&& B->HasTypedField<EJson::Number>(TEXT("p_epoch")) && !B->HasField(TEXT("p_op_id")) && !B->HasField(TEXT("p_ttl_s")));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("release"), TEXT("lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			Check(TEXT("release"), TEXT("server_released"), C.Tr->Server.Records[TEXT("ali")].Holder.IsEmpty());
			const int32 ReqBefore = C.Tr->Requests.Num();
			Check(TEXT("release"), TEXT("save_after_release_notleased"), SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2))->O == SupabaseOut::NotLeased);
			Check(TEXT("release"), TEXT("second_release_notleased"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::NotLeased);
			Check(TEXT("release"), TEXT("notleased_no_request"), C.Tr->Requests.Num() == ReqBefore);
			const TUniquePtr<FMMOSupabaseCharacterStore> Other = C.NewStore(TEXT("srv-b"));
			Check(TEXT("release"), TEXT("other_server_acquires"), SupabaseLoad(C, TEXT("MMOSave_Ali"), Other.Get())->O == SupabaseOut::Ok);
			Check(TEXT("release"), TEXT("other_server_epoch_plus_one"), Other->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev) && Epoch == 2 && Rev == 3);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.Steal(TEXT("ali"), TEXT("srv-b"));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("release"), TEXT("stolen_ownership_lost"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::OwnershipLost);
			Check(TEXT("release"), TEXT("stolen_lease_dropped"), !C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev));
			Check(TEXT("release"), TEXT("stolen_other_holder_untouched"), C.Tr->Server.Records[TEXT("ali")].Holder == TEXT("srv-b"));
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			for (int32 I = 0; I < 3; ++I) C.Tr->Script.Add(SupabaseFaultOf(FSupabaseFault::EKind::NetworkError));
			int64 Epoch = 0, Rev = 0;
			Check(TEXT("release"), TEXT("outage_unavailable"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			Check(TEXT("release"), TEXT("outage_lease_kept"), C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev) && Epoch == 1);
			Check(TEXT("release"), TEXT("retry_after_outage_ok"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			const TSharedRef<FSupabaseRes> Pending = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2), false);
			const int32 ReqBefore = C.Tr->Requests.Num();
			Check(TEXT("release"), TEXT("inflight_save_blocks_release"), SupabaseRelease(C, TEXT("MMOSave_Ali"), false)->O == SupabaseOut::Unavailable);
			Check(TEXT("release"), TEXT("inflight_no_release_request"), C.Tr->Requests.Num() == ReqBefore);
			C.Tr->Pump();
			Check(TEXT("release"), TEXT("inflight_save_completed"), Pending->O == SupabaseOut::Ok);
			Check(TEXT("release"), TEXT("release_after_save_ok"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
		}
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(C, TEXT("MMOSave_Ali"));
			C.Tr->Server.ExpireLease(TEXT("ali"));
			Check(TEXT("release"), TEXT("expired_release_ok"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok);
		}

		// ---------------- holder ----------------
		{
			FSupabaseCtx C(TEXT("srv A"));
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			Check(TEXT("holder"), TEXT("load_invalid"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Invalid);
			Check(TEXT("holder"), TEXT("create_invalid"), SupabaseCreate(C, TEXT("MMOSave_Bob"), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("holder"), TEXT("save_invalid"), SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("holder"), TEXT("renew_invalid"), SupabaseRenew(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Invalid);
			Check(TEXT("holder"), TEXT("release_invalid"), SupabaseRelease(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Invalid);
			Check(TEXT("holder"), TEXT("no_request_sent"), C.Tr->Requests.Num() == 0);
		}
		{
			// Vektordeki her holder gercek depo uzerinden: gecerli = istek gider ve kira alinir, gecersiz = Invalid ve istek YOK.
			int32 Index = 0;
			for (const TSharedPtr<FJsonValue>& Entry : Vectors.Arr(TEXT("holder")))
			{
				FString Value;
				bool bValid = false;
				Entry->AsObject()->TryGetStringField(TEXT("value"), Value);
				Entry->AsObject()->TryGetBoolField(TEXT("valid"), bValid);
				FSupabaseCtx C(*Value);
				C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
				const TSharedRef<FSupabaseRes> L = SupabaseLoad(C, TEXT("MMOSave_Ali"));
				const bool bOk = bValid ? (L->O == SupabaseOut::Ok && C.Tr->Requests.Num() == 1) : (L->O == SupabaseOut::Invalid && C.Tr->Requests.Num() == 0);
				Check(TEXT("holderstore"), *FString::Printf(TEXT("i%02d"), Index++), bOk);
			}
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

		// ---------------- LogHttp (istek basliklari / anahtar loga dusmez) ----------------
		{
			LogHttp.SetVerbosity(ELogVerbosity::VeryVerbose);
			FSupabaseCtx C; // Create: LogHttp'yi Warning'e ceker
			Check(TEXT("loghttp"), TEXT("create_pins_warning"), LogHttp.GetVerbosity() <= ELogVerbosity::Warning);
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			LogHttp.SetVerbosity(ELogVerbosity::VeryVerbose); // konsoldan yeniden acildi
			Check(TEXT("loghttp"), TEXT("request_pins_warning"), SupabaseLoad(C, TEXT("MMOSave_Ali"))->O == SupabaseOut::Ok && LogHttp.GetVerbosity() <= ELogVerbosity::Warning);
			// UE HTTP modulu basliklari Verbose'ta yazar: seviye Warning iken bu satir loga GIRMEZ.
			UE_LOG(LogHttp, Verbose, TEXT("Request Headers: apikey: %s"), SupabaseTestKey);
			Check(TEXT("loghttp"), TEXT("verbose_header_line_suppressed"), !Cap.Snapshot().Contains(SupabaseTestKey, ESearchCase::CaseSensitive));
			Check(TEXT("loghttp"), TEXT("raise_is_warned"), Cap.Snapshot().Contains(TEXT("Warning'e sabitlendi")));
		}
		{
			using FRC = FMMOSupabaseRestClient;
			Check(TEXT("cmdline"), TEXT("logcmds_verbose"), FRC::CommandLineRequestsVerboseHttp(TEXT("-game -LogCmds=\"LogHttp Verbose\"")));
			Check(TEXT("cmdline"), TEXT("logcmds_veryverbose_list"), FRC::CommandLineRequestsVerboseHttp(TEXT("-LogCmds=\"LogTemp Log, LogHttp VeryVerbose\"")));
			Check(TEXT("cmdline"), TEXT("execcmds_lowercase"), FRC::CommandLineRequestsVerboseHttp(TEXT("-ExecCmds=\"log loghttp verbose\"")));
			Check(TEXT("cmdline"), TEXT("warning_not_flagged"), !FRC::CommandLineRequestsVerboseHttp(TEXT("-LogCmds=\"LogHttp Warning, LogTemp Verbose\"")));
			Check(TEXT("cmdline"), TEXT("empty_not_flagged"), !FRC::CommandLineRequestsVerboseHttp(FString()));
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
			R1.Tr->Server.RevisionOverride = MakeShared<FJsonValueString>(TEXT("7"));
			Check(TEXT("badjson"), TEXT("badrev_text_rejected"), SupabaseLoad(R1, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			FSupabaseCtx R2;
			R2.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R2.Tr->Server.RevisionOverride = MakeShared<FJsonValueNumber>(-1.0);
			Check(TEXT("badjson"), TEXT("negrev"), SupabaseLoad(R2, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			FSupabaseCtx R3;
			R3.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R3.Tr->Server.RevisionOverride = MakeShared<FJsonValueNumber>(1.5);
			Check(TEXT("badjson"), TEXT("fracrev"), SupabaseLoad(R3, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
			FSupabaseCtx R4;
			R4.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			R4.Tr->Server.RevisionOverride = MakeShared<FJsonValueNumber>(9007199254740994.0);
			Check(TEXT("badjson"), TEXT("revision_above_2pow53_rejected"), SupabaseLoad(R4, TEXT("MMOSave_Ali"))->O == SupabaseOut::Unavailable);
		}

		// ---------------- bigint = JSON sayi ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 4503599627370497LL, 4503599627370496LL);
			const TSharedRef<FSupabaseRes> L = SupabaseLoad(C, TEXT("MMOSave_Ali"));
			int64 Epoch = 0, Rev = 0;
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("bigint"), TEXT("load_ok"), L->O == SupabaseOut::Ok);
			Check(TEXT("bigint"), TEXT("epoch_exact"), Epoch == 4503599627370497LL);
			Check(TEXT("bigint"), TEXT("rev_exact"), Rev == 4503599627370497LL);
			const TSharedRef<FSupabaseRes> S = SupabaseSave(C, TEXT("MMOSave_Ali"), SupabaseFixture(2));
			Check(TEXT("bigint"), TEXT("save_ok"), S->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("save_character"));
			Check(TEXT("bigint"), TEXT("body_epoch_number_exact"), SupabaseNumberOf(B, TEXT("p_epoch")) == 4503599627370497.0);
			Check(TEXT("bigint"), TEXT("body_rev_number_exact"), SupabaseNumberOf(B, TEXT("p_expected_revision")) == 4503599627370497.0);
			Check(TEXT("bigint"), TEXT("server_rev"), C.Tr->Server.Records[TEXT("ali")].Revision == 4503599627370498LL);
			C.Store->TryGetSlotInfo(TEXT("MMOSave_Ali"), Epoch, Rev);
			Check(TEXT("bigint"), TEXT("client_rev"), Rev == 4503599627370498LL);
		}

		// ---------------- names ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali_veli"), SupabaseFixture(1), 1, 0);
			C.Tr->Server.Seed(TEXT("mmosave_x"), SupabaseFixture(1), 1, 0);
			const TSharedRef<FSupabaseRes> Cn = SupabaseLoad(C, TEXT("MMOSave_Ali Veli"));
			Check(TEXT("names"), TEXT("canonical_ok"), Cn->O == SupabaseOut::Ok);
			const TSharedPtr<FJsonObject> B = C.Tr->LastBody(TEXT("acquire_lease"));
			Check(TEXT("names"), TEXT("canonical_body"), B.IsValid() && B->GetStringField(TEXT("p_key")) == TEXT("ali_veli"));
			Check(TEXT("names"), TEXT("prefix_stripped_once"), SupabaseLoad(C, TEXT("MMOSave_MMOSave_x"))->O == SupabaseOut::Ok
				&& C.Tr->LastBody(TEXT("acquire_lease"))->GetStringField(TEXT("p_key")) == TEXT("mmosave_x"));
			const int32 ReqBefore = C.Tr->Requests.Num();
			Check(TEXT("names"), TEXT("invalid_nonascii"), SupabaseLoad(C, *SupabaseNonAsciiSlot())->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_punct_not_trimmed"), SupabaseLoad(C, TEXT("MMOSave_Ali!"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_reserved_player"), SupabaseLoad(C, TEXT("MMOSave_Player123"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_reserved_default"), SupabaseLoad(C, TEXT("MMOSave_Default"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_too_long"), SupabaseLoad(C, *(FString(TEXT("MMOSave_")) + FString::ChrN(33, TEXT('a'))))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_empty"), SupabaseLoad(C, TEXT("MMOSave_"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_slash"), SupabaseLoad(C, TEXT("MMOSave_a/b"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_leading_space"), SupabaseLoad(C, TEXT("MMOSave_ Ali"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_trailing_space"), SupabaseLoad(C, TEXT("MMOSave_Ali "))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_only_spaces"), SupabaseLoad(C, TEXT("MMOSave_   "))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_leading_space_no_prefix"), SupabaseLoad(C, TEXT(" Ali"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_space_after_prefix_strip"), SupabaseLoad(C, TEXT("MMOSave_ MMOSave_x"))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("invalid_no_requests"), C.Tr->Requests.Num() == ReqBefore);
			Check(TEXT("names"), TEXT("save_invalid"), SupabaseSave(C, *SupabaseNonAsciiSlot(), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("create_invalid"), SupabaseCreate(C, *SupabaseNonAsciiSlot(), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("create_trailing_space_invalid"), SupabaseCreate(C, TEXT("MMOSave_Bob "), SupabaseFixture(1))->O == SupabaseOut::Invalid);
			Check(TEXT("names"), TEXT("exists_invalid"), SupabaseExists(C, TEXT("MMOSave_Ali "))->O == SupabaseOut::Invalid);
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

		// ---------------- sahipsiz geri cagri (depo silindi) ----------------
		{
			FSupabaseCtx C;
			C.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			bool bCalled = false;
			C.Store->LoadEx(TEXT("MMOSave_Ali"), [&](SupabaseOut, FMMOCharacterSaveData&&) { bCalled = true; });
			C.Store.Reset(); // yanit gelmeden depo silinir
			C.Tr->Pump();
			Check(TEXT("orphan"), TEXT("load_callback_not_called"), !bCalled);
			Check(TEXT("orphan"), TEXT("load_orphan_logged"), Cap.Snapshot().Contains(TEXT("tamamlanma geri cagrisi atlandi (depo silindi) fn=acquire_lease")));
			Check(TEXT("orphan"), TEXT("client_cleaned"), C.Client->NumInFlight() == 0);

			FSupabaseCtx S;
			S.Tr->Server.Seed(TEXT("ali"), SupabaseFixture(1), 3, 0);
			SupabaseLoad(S, TEXT("MMOSave_Ali"));
			bool bSaveCalled = false;
			S.Store->SaveEx(TEXT("MMOSave_Ali"), SupabaseFixture(2), [&](SupabaseOut) { bSaveCalled = true; });
			S.Store.Reset();
			S.Tr->Pump();
			Check(TEXT("orphan"), TEXT("save_callback_not_called"), !bSaveCalled);
			Check(TEXT("orphan"), TEXT("save_orphan_logged"), Cap.Snapshot().Contains(TEXT("tamamlanma geri cagrisi atlandi (depo silindi) fn=save_character")));
			Check(TEXT("orphan"), TEXT("save_server_still_committed"), S.Tr->Server.Commits == 1);
			Check(TEXT("orphan"), TEXT("orphan_log_clean"), !SupabaseLeaks(Cap.Snapshot()));
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
			FMMOCharacterSaveJson::FromJson(C.Tr->Server.Records[TEXT("ali")].DataText(), Stored);
			Check(TEXT("coalesce"), TEXT("server_gold_last"), Stored.Gold == 3);
			Check(TEXT("coalesce"), TEXT("commits_two"), C.Tr->Server.Commits == 2);
			Check(TEXT("coalesce"), TEXT("rev_five"), C.Tr->Server.Records[TEXT("ali")].Revision == 5);
		}

		// ---------------- sozlesme: depo testlerinde sunucu hic ihlal gormedi ----------------
		Check(TEXT("contract"), TEXT("store_flows_no_param_or_template_violations"), SupabaseGViolations == 0);
		Check(TEXT("contract"), TEXT("store_flows_exercised_server"), SupabaseGRequests > 200);

		// ---------------- log (tum senaryolar boyunca) ----------------
		{
			const FString Captured = Cap.Snapshot();
			Check(TEXT("log"), TEXT("no_key"), !Captured.Contains(SupabaseTestKey, ESearchCase::CaseSensitive));
			Check(TEXT("log"), TEXT("no_host"), !Captured.Contains(SupabaseTestHost, ESearchCase::IgnoreCase));
			Check(TEXT("log"), TEXT("has_key_mask"), Captured.Contains(TEXT("sb_secret_****(")));
			Check(TEXT("log"), TEXT("has_url_mask"), Captured.Contains(TEXT("[supabase-url]")));
			Check(TEXT("log"), TEXT("has_rpc_lines"), Captured.Contains(TEXT("rpc POST")));
		}

		if (OutChecks != SupabaseTestExpectedChecks)
		{
			++Failures;
			OutError += FString::Printf(TEXT(" check_count_mismatch(got=%d expected=%d)"), OutChecks, SupabaseTestExpectedChecks);
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
// (FMMOUnrealHttpTransport), gercek PostgREST/PostgreSQL davranisi, zamanlayici (FTSTicker) ve gercek kira SURESI DEGILDIR.
// Sahte sunucu davranisi SQL fonksiyonlarini (supabase/migrations) elle modeller; modelin dogrulugu vectors.json senaryolarinin
// ayni RPC istemcisinden gecmesiyle sinanir. Gercek SQL ile uyum supabase/tests (psql) + Dilim 6 entegrasyonuna kalir.
// Bilincli gevsek noktalar: yanlis tipli parametre gercek PG'de bazen sessizce donusturulur (burada 404); sqlstate 42883 sahtede
// yalniz HTTP 404 + PGRST202 olarak modellenir.
