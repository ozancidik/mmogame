// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSupabaseRestClient.h"
#include "Async/Async.h"
#include "HAL/PlatformMisc.h"
#include "Http.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogMMOSupabase);

namespace
{
	const TCHAR* SupabaseEnvUrl = TEXT("MMO_SUPABASE_URL");
	const TCHAR* SupabaseEnvKey = TEXT("MMO_SUPABASE_SECRET_KEY");
	constexpr int32 SupabaseMaxResponseChars = 4 * 1024 * 1024;

	bool SupabaseHasBadChar(const FString& S)
	{
		for (const TCHAR Ch : S)
		{
			if (Ch <= 32 || Ch >= 127)
			{
				return true;
			}
		}
		return false;
	}

	bool SupabaseIsFunctionName(const FString& Fn)
	{
		if (Fn.IsEmpty() || Fn.Len() > 48)
		{
			return false;
		}
		for (const TCHAR Ch : Fn)
		{
			if (!((Ch >= TEXT('a') && Ch <= TEXT('z')) || Ch == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}
}

bool FMMOSupabaseConfig::Validate(const FMMOSupabaseConfig& Config, FString& OutError)
{
	OutError.Reset();
	if (Config.BaseUrl.IsEmpty())
	{
		OutError = TEXT("Supabase URL yok");
		return false;
	}
	if (!Config.BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase) || Config.BaseUrl.Len() <= 9)
	{
		OutError = TEXT("Supabase URL https olmali");
		return false;
	}
	// Kullanici bilgisi/sorgu/parca anahtar sizdirabilir ya da URL'yi bozar.
	if (SupabaseHasBadChar(Config.BaseUrl) || Config.BaseUrl.Contains(TEXT("@")) || Config.BaseUrl.Contains(TEXT("?"))
		|| Config.BaseUrl.Contains(TEXT("#")))
	{
		OutError = TEXT("Supabase URL gecersiz");
		return false;
	}
	if (Config.SecretKey.IsEmpty())
	{
		OutError = TEXT("Supabase anahtari yok");
		return false;
	}
	if (Config.SecretKey.Len() < 8 || SupabaseHasBadChar(Config.SecretKey))
	{
		OutError = TEXT("Supabase anahtari gecersiz");
		return false;
	}
	if (Config.MaxAttempts < 1 || Config.RequestTimeoutSeconds <= 0.f || Config.BudgetSeconds <= 0.f)
	{
		OutError = TEXT("Supabase zaman/deneme ayari gecersiz");
		return false;
	}
	return true;
}

bool FMMOSupabaseConfig::FromEnvironment(FMMOSupabaseConfig& OutConfig, FString& OutError)
{
	FMMOSupabaseConfig Loaded;
	Loaded.BaseUrl = FPlatformMisc::GetEnvironmentVariable(SupabaseEnvUrl).TrimStartAndEnd();
	Loaded.SecretKey = FPlatformMisc::GetEnvironmentVariable(SupabaseEnvKey).TrimStartAndEnd();
	if (!Validate(Loaded, OutError))
	{
		return false;
	}
	OutConfig = Loaded;
	return true;
}

FMMOSupabaseRestClient::FMMOSupabaseRestClient(const TSharedRef<IMMOHttpTransport>& InTransport, const FMMOSupabaseConfig& InConfig)
	: Transport(InTransport)
	, Config(InConfig)
	, BaseUrl(InConfig.BaseUrl)
{
	while (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}
}

TSharedPtr<FMMOSupabaseRestClient> FMMOSupabaseRestClient::Create(const TSharedRef<IMMOHttpTransport>& Transport,
	const FMMOSupabaseConfig& Config, FString& OutError)
{
	if (!FMMOSupabaseConfig::Validate(Config, OutError))
	{
		return nullptr;
	}
	EnforceHttpLogLevel();
	if (CommandLineRequestsVerboseHttp(FCommandLine::Get()))
	{
		UE_LOG(LogMMOSupabase, Warning, TEXT("Komut satirinda LogHttp Verbose istendi; Supabase istemcisi LogHttp'yi Warning'e sabitler (istek basliklarinda anahtar var)"));
	}
	TSharedRef<FMMOSupabaseRestClient> Client = MakeShareable(new FMMOSupabaseRestClient(Transport, Config));
	UE_LOG(LogMMOSupabase, Log, TEXT("Supabase istemcisi hazir: %s"), *Client->DescribeMasked());
	return Client;
}

void FMMOSupabaseRestClient::EnforceHttpLogLevel()
{
	// NO_LOGGING (Shipping, bUseLoggingInShipping yok): LogHttp seviyesi okunamaz/ayarlanamaz ve zaten hicbir satir yazilmaz.
#if !NO_LOGGING
	const ELogVerbosity::Type Current = LogHttp.GetVerbosity();
	if (Current <= ELogVerbosity::Warning)
	{
		return;
	}
	LogHttp.SetVerbosity(ELogVerbosity::Warning);
	if (Current >= ELogVerbosity::Verbose)
	{
		UE_LOG(LogMMOSupabase, Warning, TEXT("LogHttp %s idi; istek basliklari (apikey) loga dusmesin diye Warning'e sabitlendi"),
			ToString(Current));
	}
#endif
}

bool FMMOSupabaseRestClient::UsesBearerAuth(const FString& SecretKey)
{
	// Eski service_role anahtari JWT'dir (header.payload.signature, "eyJ" ile baslar). Yeni sb_secret_... anahtari JWT DEGILDIR:
	// Authorization: Bearer ile gonderilirse gateway 401 verir; yalniz apikey basligi kullanilir.
	if (!SecretKey.StartsWith(TEXT("eyJ"), ESearchCase::CaseSensitive))
	{
		return false;
	}
	int32 Dots = 0;
	for (const TCHAR Ch : SecretKey)
	{
		Dots += Ch == TEXT('.') ? 1 : 0;
	}
	return Dots == 2;
}

bool FMMOSupabaseRestClient::CommandLineRequestsVerboseHttp(const FString& CommandLine)
{
	const FString Lower = CommandLine.ToLower();
	int32 From = 0;
	while (true)
	{
		const int32 At = Lower.Find(TEXT("loghttp"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		if (At == INDEX_NONE)
		{
			return false;
		}
		// Ayni LogCmds/ExecCmds girdisi icinde (virgul/tirnaga kadar) "verbose" (veryverbose dahil) geciyor mu.
		FString Tail = Lower.Mid(At + 7, 24);
		int32 Cut = INDEX_NONE;
		if (Tail.FindChar(TEXT(','), Cut) || Tail.FindChar(TEXT('"'), Cut))
		{
			Tail.LeftInline(Cut);
		}
		if (Tail.Contains(TEXT("verbose")))
		{
			return true;
		}
		From = At + 7;
	}
}

FString FMMOSupabaseRestClient::NewOpId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

FString FMMOSupabaseRestClient::Redact(const FString& Text) const
{
	FString Out = Text;
	if (!Config.SecretKey.IsEmpty())
	{
		Out.ReplaceInline(*Config.SecretKey, TEXT("[supabase-key]"), ESearchCase::CaseSensitive); // uzunluk bilgisi de verilmez
	}
	if (!Config.BaseUrl.IsEmpty())
	{
		Out.ReplaceInline(*Config.BaseUrl, TEXT("[supabase-url]"), ESearchCase::IgnoreCase);
		Out.ReplaceInline(*BaseUrl, TEXT("[supabase-url]"), ESearchCase::IgnoreCase);
		FString Host = BaseUrl;
		Host.RightChopInline(8); // "https://"
		if (!Host.IsEmpty())
		{
			Out.ReplaceInline(*Host, TEXT("[supabase-host]"), ESearchCase::IgnoreCase);
		}
	}
	return Out;
}

FString FMMOSupabaseRestClient::DescribeMasked() const
{
	return FString::Printf(TEXT("url=[supabase-url] key=[supabase-key] timeout=%.1fs budget=%.1fs attempts=%d"),
		Config.RequestTimeoutSeconds, Config.BudgetSeconds, Config.MaxAttempts);
}

uint64 FMMOSupabaseRestClient::Call(const FString& Function, const TSharedRef<FJsonObject>& Params, const FString& OpId, FCallComplete&& Done)
{
	const TSharedRef<FCall> Call = MakeShared<FCall>();
	Call->Id = ++LastCallId;
	Call->Function = Function;
	Call->OpId = OpId;
	Call->Done = MoveTemp(Done);
	Call->StartTime = Transport->NowSeconds();
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Call->Body);
	FJsonSerializer::Serialize(Params, Writer);
	Writer->Close();
	Calls.Add(Call->Id, Call);

	if (!SupabaseIsFunctionName(Function))
	{
		// Asenkron reddet (cagiran yeniden girisi varsaymasin).
		FMMORpcResult Bad;
		Bad.Status = EMMORpcStatus::Failed;
		Bad.Error = EMMORpcError::BadFunction;
		Bad.OpId = OpId;
		Bad.Message = TEXT("gecersiz rpc adi");
		const TWeakPtr<FMMOSupabaseRestClient> Weak = AsShared();
		const uint64 Id = Call->Id;
		Transport->ScheduleAfter(0.f, [Weak, Id, Bad]()
		{
			if (const TSharedPtr<FMMOSupabaseRestClient> Self = Weak.Pin())
			{
				if (const TSharedRef<FCall>* Found = Self->Calls.Find(Id))
				{
					const TSharedRef<FCall> Keep = *Found;
					FMMORpcResult Copy = Bad;
					Self->Finish(Keep, MoveTemp(Copy));
				}
			}
		});
		return Call->Id;
	}
	StartAttempt(Call);
	return Call->Id;
}

void FMMOSupabaseRestClient::Cancel(uint64 CallId)
{
	if (const TSharedRef<FCall>* Found = Calls.Find(CallId))
	{
		(*Found)->bCancelled = true;
		(*Found)->Done = nullptr;
	}
}

void FMMOSupabaseRestClient::StartAttempt(const TSharedRef<FCall>& Call)
{
	++Call->Attempts;
	EnforceHttpLogLevel(); // baslik loglamasi konsoldan yeniden acilmis olabilir
	FMMOHttpRequest Request;
	Request.Verb = TEXT("POST");
	Request.Url = BaseUrl + TEXT("/rest/v1/rpc/") + Call->Function;
	Request.Body = Call->Body;
	Request.TimeoutSeconds = Config.RequestTimeoutSeconds;
	Request.Headers.Add({TEXT("apikey"), Config.SecretKey});
	if (UsesBearerAuth(Config.SecretKey))
	{
		Request.Headers.Add({TEXT("Authorization"), TEXT("Bearer ") + Config.SecretKey});
	}
	Request.Headers.Add({TEXT("Content-Type"), TEXT("application/json")});
	Request.Headers.Add({TEXT("Accept"), TEXT("application/json")});
	// mmo_api ozel sema: PostgREST profil basliklarini ister.
	Request.Headers.Add({TEXT("Content-Profile"), TEXT("mmo_api")});
	Request.Headers.Add({TEXT("Accept-Profile"), TEXT("mmo_api")});
	UE_LOG(LogMMOSupabase, Verbose, TEXT("rpc POST %s op=%s attempt=%d"), *Redact(Request.Url), *Call->OpId, Call->Attempts);

	const TWeakPtr<FMMOSupabaseRestClient> Weak = AsShared();
	const uint64 Id = Call->Id;
	Transport->Send(Request, [Weak, Id](FMMOHttpResponse&& Response)
	{
		const auto Handle = [Weak, Id](FMMOHttpResponse&& Resp)
		{
			if (const TSharedPtr<FMMOSupabaseRestClient> Self = Weak.Pin())
			{
				Self->OnResponse(Id, MoveTemp(Resp));
			}
		};
		if (IsInGameThread())
		{
			Handle(MoveTemp(Response));
		}
		else
		{
			// Sozlesme: geri cagri oyun is parcaciginda.
			AsyncTask(ENamedThreads::GameThread, [Handle, Moved = MoveTemp(Response)]() mutable
			{
				Handle(MoveTemp(Moved));
			});
		}
	});
}

void FMMOSupabaseRestClient::OnResponse(uint64 CallId, FMMOHttpResponse&& Response)
{
	const TSharedRef<FCall>* Found = Calls.Find(CallId);
	if (!Found)
	{
		return;
	}
	const TSharedRef<FCall> Call = *Found;
	if (Call->bCancelled)
	{
		Calls.Remove(CallId);
		return;
	}

	FMMORpcResult Result;
	Result.OpId = Call->OpId;
	Result.Attempts = Call->Attempts;
	Result.HttpCode = Response.HttpCode;

	bool bRetryable = false;
	bool bAmbiguousIfExhausted = true;
	if (Response.Result == EMMOHttpResult::Timeout)
	{
		bRetryable = true;
		Result.Error = EMMORpcError::Timeout;
		Result.Message = TEXT("zaman asimi");
	}
	else if (Response.Result == EMMOHttpResult::NetworkError)
	{
		bRetryable = true;
		Result.Error = EMMORpcError::Network;
		Result.Message = TEXT("ag hatasi");
	}
	else if (Response.HttpCode == 200)
	{
		TSharedPtr<FJsonObject> Root;
		if (Response.Body.Len() <= SupabaseMaxResponseChars)
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.Body);
			if (!FJsonSerializer::Deserialize(Reader, Root))
			{
				Root.Reset();
			}
		}
		FString Outcome;
		if (Root.IsValid() && Root->TryGetStringField(TEXT("outcome"), Outcome) && !Outcome.IsEmpty())
		{
			Result.Status = EMMORpcStatus::Ok;
			Result.Outcome = Outcome;
			Result.Json = Root;
			Finish(Call, MoveTemp(Result));
			return;
		}
		// 200 ama govde bozuk: islem uygulanmis olabilir -> belirsiz, RED.
		Result.Status = EMMORpcStatus::Unknown;
		Result.Error = EMMORpcError::BadResponse;
		Result.Message = TEXT("bozuk yanit");
		Finish(Call, MoveTemp(Result));
		return;
	}
	else if (Response.HttpCode == 408 || Response.HttpCode >= 500)
	{
		bRetryable = true;
		Result.Error = EMMORpcError::Http5xx;
		Result.Message = FString::Printf(TEXT("http %d"), Response.HttpCode);
	}
	else if (Response.HttpCode == 429)
	{
		// Hiz siniri istek islenmeden doner: denenir, bitince kesin basarisiz (uygulanmadi).
		bRetryable = true;
		bAmbiguousIfExhausted = false;
		Result.Error = EMMORpcError::Http429;
		Result.Message = TEXT("http 429");
	}
	else if (Response.HttpCode >= 400 && Response.HttpCode < 500)
	{
		Result.Status = EMMORpcStatus::Failed;
		Result.Error = EMMORpcError::Http4xx;
		Result.Message = FString::Printf(TEXT("http %d"), Response.HttpCode);
		UE_LOG(LogMMOSupabase, Warning, TEXT("rpc fn=%s op=%s reddedildi: %s"), *Call->Function, *Call->OpId, *Result.Message);
		Finish(Call, MoveTemp(Result));
		return;
	}
	else
	{
		Result.Status = EMMORpcStatus::Unknown;
		Result.Error = EMMORpcError::BadResponse;
		Result.Message = FString::Printf(TEXT("beklenmeyen http %d"), Response.HttpCode);
		Finish(Call, MoveTemp(Result));
		return;
	}

	if (bRetryable && Call->Attempts < Config.MaxAttempts)
	{
		const float Delay = 0.5f * static_cast<float>(1 << (Call->Attempts - 1)) + FMath::FRandRange(0.f, 0.25f);
		if ((Transport->NowSeconds() - Call->StartTime) + Delay < Config.BudgetSeconds)
		{
			UE_LOG(LogMMOSupabase, Verbose, TEXT("rpc fn=%s op=%s yeniden deneme (%s) %.2fs sonra"), *Call->Function, *Call->OpId, *Result.Message, Delay);
			const TWeakPtr<FMMOSupabaseRestClient> Weak = AsShared();
			Transport->ScheduleAfter(Delay, [Weak, CallId]()
			{
				if (const TSharedPtr<FMMOSupabaseRestClient> Self = Weak.Pin())
				{
					Self->Resume(CallId);
				}
			});
			return;
		}
	}

	Result.Status = bAmbiguousIfExhausted ? EMMORpcStatus::Unknown : EMMORpcStatus::Failed;
	UE_LOG(LogMMOSupabase, Warning, TEXT("rpc fn=%s op=%s basarisiz: %s (deneme=%d, durum=%s)"), *Call->Function, *Call->OpId,
		*Result.Message, Call->Attempts, bAmbiguousIfExhausted ? TEXT("belirsiz") : TEXT("kesin"));
	Finish(Call, MoveTemp(Result));
}

void FMMOSupabaseRestClient::Resume(uint64 CallId)
{
	const TSharedRef<FCall>* Found = Calls.Find(CallId);
	if (!Found)
	{
		return;
	}
	const TSharedRef<FCall> Call = *Found;
	if (Call->bCancelled)
	{
		Calls.Remove(CallId);
		return;
	}
	StartAttempt(Call);
}

void FMMOSupabaseRestClient::Finish(const TSharedRef<FCall>& Call, FMMORpcResult&& Result)
{
	Calls.Remove(Call->Id);
	Result.Attempts = Call->Attempts;
	Result.OpId = Call->OpId;
	if (Call->bCancelled)
	{
		return;
	}
	FCallComplete Done = MoveTemp(Call->Done);
	if (Done)
	{
		Done(MoveTemp(Result));
	}
}
