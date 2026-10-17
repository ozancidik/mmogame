// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Save/MMOHttpTransport.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMOSupabase, Log, All);

/**
 * Supabase baglanti ayari (docs/DESIGN_SUPABASE.md 4.4). Anahtar YALNIZ ortam degiskeninden okunur
 * (MMO_SUPABASE_SECRET_KEY), URL MMO_SUPABASE_URL; komut satirina/ini'ye YAZILMAZ. Anahtar ve URL hicbir
 * hata metnine/loga ham girmez (Redact).
 */
struct MYPROJECT_API FMMOSupabaseConfig
{
	FString BaseUrl;   // https://<ref>.supabase.co
	FString SecretKey; // sb_secret_... (ya da eski service_role JWT)
	float RequestTimeoutSeconds = 8.f; // istek basina
	float BudgetSeconds = 20.f;        // mantiksal islem (tum denemeler)
	int32 MaxAttempts = 3;

	/**
	 * Her istekte giden ACIK User-Agent. Supabase gecidi tarayici gibi gorunen UA ile gelen secret API key'i reddeder
	 * (HTTP 4xx, "Forbidden use of secret API key in browser"); UE'nin varsayilan UA'si (Proje/Surum Platform/Surum) ve
	 * Windows PowerShell varsayilani buna takilabilir. Bu deger UE HTTP'nin kendi UA'sini ezer.
	 * Yasakli alt dizgeler (Mozilla, Chrome, Safari, Edge, Gecko, AppleWebKit; buyuk/kucuk harf fark etmez) Validate'te reddedilir.
	 */
	static constexpr const TCHAR* DefaultUserAgent = TEXT("MMOGameServer/0.1");
	FString UserAgent = DefaultUserAgent;

	/** UserAgent: bos degil, <= 64 karakter, yalniz gorunur ASCII (bosluk yok), tarayici gibi gorunen alt dizge yok. */
	static bool IsAllowedUserAgent(const FString& UserAgent);

	/** https, kullanici bilgisi/sorgu yok; anahtar bos degil, bosluk/kontrol karakteri yok. OutError sabit metindir. */
	static bool Validate(const FMMOSupabaseConfig& Config, FString& OutError);
	/** Ortam degiskenlerinden okur ve dogrular. Eksik/bos = false (sunucu baslamaz, dosyaya DUSULMEZ). */
	static bool FromEnvironment(FMMOSupabaseConfig& OutConfig, FString& OutError);
};

enum class EMMORpcStatus : uint8
{
	Ok,      // HTTP 200 ve gecerli JSON (Outcome dolu); is sonucu Outcome'dadir
	Unknown, // BELIRSIZ: zaman asimi/ag/5xx/bozuk yanit; islem uygulanmis OLABILIR
	Failed,  // kesin basarisiz: istek uygulanmadi (4xx, yapilandirma)
};

enum class EMMORpcError : uint8
{
	None,
	NotConfigured,
	BadFunction,
	Http4xx,
	Http429,
	Http5xx,
	Timeout,
	Network,
	BadResponse,
};

struct FMMORpcResult
{
	EMMORpcStatus Status = EMMORpcStatus::Failed;
	EMMORpcError Error = EMMORpcError::None;
	int32 HttpCode = 0;
	int32 Attempts = 0;
	FString OpId;
	FString Outcome;                 // Status == Ok iken: yanittaki "outcome"
	TSharedPtr<FJsonObject> Json;    // Status == Ok iken: yanit govdesi
	FString Message;                 // sabit metin + kod; govde/URL/anahtar ICERMEZ
};

/**
 * Supabase PostgREST RPC istemcisi: POST {URL}/rest/v1/rpc/<fn>, JSON govde. Yeniden deneme AYNI govde
 * (ayni op_id) ile; yalniz ag hatasi/zaman asimi/408/429/5xx; 4xx denenmez. Geri cagri oyun is parcaciginda.
 * Iptal yalniz yerel geri cagriyi keser; sunucudaki commit'i iptal ETMEZ (NETWORK_AUDIT O6 siniri).
 */
class MYPROJECT_API FMMOSupabaseRestClient : public TSharedFromThis<FMMOSupabaseRestClient>
{
public:
	using FCallComplete = TFunction<void(FMMORpcResult&&)>;

	/** Yapilandirma gecersizse nullptr + OutError. */
	static TSharedPtr<FMMOSupabaseRestClient> Create(const TSharedRef<IMMOHttpTransport>& Transport,
		const FMMOSupabaseConfig& Config, FString& OutError);

	/** Sozlesme surumu (supabase/tests/vectors.json contract_version, SQL healthcheck.contract_version). */
	static constexpr int32 ContractVersion = 2;

	/** Yeni op_id (UUID, kucuk harf, tireli). Mantiksal bir yazim icin BIR KEZ uretilir. */
	static FString NewOpId();

	/**
	 * LogHttp'yi Warning'e sabitler: UE HTTP modulu Verbose/VeryVerbose'ta istek basliklarini (apikey/Authorization)
	 * loga yazabilir. Surec geneli yan etki: LogHttp'nin Log seviyesindeki ozet satirlari baska HTTP kullanicilari icin de
	 * kapanir. NO_LOGGING derlemesinde (Shipping, bUseLoggingInShipping yok) hicbir sey yapmaz. Create'te ve her istek denemesinde cagrilir (konsoldan yeniden acilsa bile bir sonraki istekte kapanir).
	 */
	static void EnforceHttpLogLevel();
	/**
	 * Authorization: Bearer yalniz JWT bicimli (eski service_role) anahtarla gonderilir; sb_secret_... anahtarinda YALNIZ apikey
	 * basligi vardir (Bearer ile 401 riski). Her ikisinde de anahtar loga/hataya ham girmez.
	 */
	static bool UsesBearerAuth(const FString& SecretKey);
	/** Komut satirinda LogHttp icin Verbose/VeryVerbose istenmis mi (-LogCmds="LogHttp Verbose" vb.). */
	static bool CommandLineRequestsVerboseHttp(const FString& CommandLine);

	/** Params govde olur; op_id'yi cagiran Params'a koyar ve burada log icin verir. Done her zaman ASENKRON doner. */
	uint64 Call(const FString& Function, const TSharedRef<FJsonObject>& Params, const FString& OpId, FCallComplete&& Done);
	void Cancel(uint64 CallId);
	int32 NumInFlight() const { return Calls.Num(); }

	/** Anahtar ve URL maskelenir: [supabase-key] (uzunluk bilgisi YOK), [supabase-url], [supabase-host]. */
	FString Redact(const FString& Text) const;
	/** Log icin maskeli ozet. */
	FString DescribeMasked() const;

private:
	struct FCall
	{
		uint64 Id = 0;
		FString Function;
		FString OpId;
		FString Body;
		int32 Attempts = 0;
		double StartTime = 0.0;
		bool bCancelled = false;
		FCallComplete Done;
	};

	FMMOSupabaseRestClient(const TSharedRef<IMMOHttpTransport>& InTransport, const FMMOSupabaseConfig& InConfig);

	void StartAttempt(const TSharedRef<FCall>& Call);
	void OnResponse(uint64 CallId, FMMOHttpResponse&& Response);
	void Resume(uint64 CallId);
	void Finish(const TSharedRef<FCall>& Call, FMMORpcResult&& Result);

	TSharedRef<IMMOHttpTransport> Transport;
	FMMOSupabaseConfig Config;
	FString BaseUrl; // sondaki '/' atilmis
	uint64 LastCallId = 0;
	TMap<uint64, TSharedRef<FCall>> Calls;
};
