// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MMODialogueSubsystem.generated.h"

/** LLM cevabi hazir oldugunda cagrilir. bSuccess=false ise Text hata/yedek metindir. */
DECLARE_DELEGATE_TwoParams(FMMOOnDialogueReady, bool /*bSuccess*/, const FString& /*Text*/);

/** Hangi LLM saglayicisi kullanilacak. */
UENUM(BlueprintType)
enum class EMMOLLMProvider : uint8
{
	// NVIDIA NIM (build.nvidia.com) - OpenAI uyumlu, ucretsiz katman. Anahtar: NVIDIA_API_KEY.
	NVIDIA UMETA(DisplayName = "NVIDIA NIM (build.nvidia.com)"),
	// Anthropic Messages API. Anahtar: ANTHROPIC_API_KEY.
	Anthropic UMETA(DisplayName = "Anthropic")
};

/**
 * Oyun ici NPC diyalogu icin LLM istemcisi (NVIDIA NIM veya Anthropic).
 *
 * Tasarim:
 *  - SADECE SUNUCUDA calisir. API anahtari istemciye asla gitmez; uretilen metin
 *    Client RPC ile sadece istegi yapan oyuncuya replike edilir.
 *  - Anahtar ortam degiskeninden okunur (koda gomulmez, kaydedilmez): saglayiciya gore
 *    NVIDIA_API_KEY veya ANTHROPIC_API_KEY.
 *  - Resmi C++ SDK yok; Unreal'in HTTP modulu ile ham istek atilir. NVIDIA endpoint'i
 *    OpenAI /chat/completions formatiyla uyumludur.
 */
UCLASS()
class MYPROJECT_API UMMODialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * NPC icin tek seferlik bir replik uretir.
	 * @param NpcName    NPC'nin adi (or. "Satici Merek")
	 * @param NpcRole    NPC'nin rolu/kisiligi (or. "kasabanin silah saticisi, aksi ama durust")
	 * @param PlayerInfo Oyuncu baglami (seviye, sinif, aktif gorev) — replige renk katar
	 * @param OnReady    Cevap hazir olunca cagrilir (oyun is parcaciginda)
	 * @param PlayerKey  Istegi yapan oyuncunun anahtari (oyuncu adi). Oyuncu basi gunluk kota icin; bos ise
	 *                   ortak "?" kovasi sayilir. LLM'e GITMEZ (yalniz sayac anahtari).
	 */
	void RequestNpcLine(const FString& NpcName, const FString& NpcRole,
		const FString& PlayerInfo, FMMOOnDialogueReady OnReady, const FString& PlayerKey = FString());

	/** Serbest bir soru sorar (test/konsol icin). */
	void RequestRaw(const FString& SystemPrompt, const FString& UserPrompt, FMMOOnDialogueReady OnReady,
		const FString& PlayerKey = FString());

	/** Secili saglayicinin API anahtari tanimli mi? (tanimli degilse istek atmayiz) */
	bool HasApiKey() const;

	// --- Ayarlar ---

	// Hangi LLM saglayicisi. Varsayilan NVIDIA NIM (ucretsiz katman). Anthropic'e
	// gecmek icin NVIDIA yerine Anthropic sec ve ANTHROPIC_API_KEY tanimla.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	EMMOLLMProvider Provider = EMMOLLMProvider::NVIDIA;

	// Anthropic modeli (Provider=Anthropic iken). Oyun ici gecikme onemliyse daha
	// kucuk bir modele gecmek OYUNCUNUN karari; varsayilan en yetenekli model.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	FString Model = TEXT("claude-opus-5");

	// NVIDIA NIM modeli (Provider=NVIDIA iken). OpenAI-uyumlu id. Ucretsiz katman ornekleri:
	// meta/llama-3.1-8b-instruct (hizli), meta/llama-3.3-70b-instruct (daha kaliteli),
	// nvidia/llama-3.1-nemotron-70b-instruct. Gecikme oyun ici onemli oldugu icin 8B varsayilan.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	FString NvidiaModel = TEXT("meta/llama-3.1-8b-instruct");

	// NVIDIA (OpenAI) icin ornekleme sicakligi. Replikleri cesitli ama tutarli tutar.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	float Temperature = 0.7f;

	// Cikti uzunlugu tavani (replik <= 2 cumle, oyuna 240 karakter girer; SECURITY_AUDIT Y-4: eskiden 2048,
	// istek basina maliyet/gecikme tavani gereksiz genisti). NVIDIA yolunda dusunme yok, 400 bol bol yeter.
	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "64"))
	int32 MaxTokens = 400;

	// Anthropic yolunda DIKKAT: Opus 5'te dusunme (thinking) varsayilan ACIK ve dusunme + metin BIRLIKTE
	// max_tokens'a sayilir; 400 dusunmeyi yetersiz birakirsa cevap kesilir (stop_reason=max_tokens) ve
	// sessizce yedege duser. Bu yuzden Anthropic icin taban ayri tutulur (olculmedi; Anthropic'e gecilirse
	// yedege dusme sikligina bak).
	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "64"))
	int32 AnthropicMaxTokens = 1024;

	// Dusuk gecikme icin effort=low. (Dusunmeyi tamamen KAPATMIYORUZ: kapaliyken
	// model ara sira <thinking> etiketlerini gorunur metne sizdirabiliyor ki bu
	// oyuncunun ekraninda hata gibi gorunur. Dusuk effort zaten hizli ve ucuz.)
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	FString Effort = TEXT("low");

	// Istek zaman asimi (saniye). Oyun icinde uzun beklememek icin kisa tutuluyor.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	float TimeoutSeconds = 12.f;

	// Sunucu geneli hiz siniri (SECURITY_AUDIT Y-4): oyuncu basina 2 sn cooldown tek basina yetmez
	// (cok sayida baglanti = cok istek = fatura/kota). Sinir asilinca istek atilmaz, NPC yedek replige duser.
	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "1"))
	int32 MaxRequestsPerMinute = 30;

	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "1"))
	int32 MaxConcurrentRequests = 4;

	// Oturum (bu GameInstance / host sureci omru) basina toplam LLM cagrisi butcesi. Dakikalik sinir kotu
	// niyetli ama yavas bir sizintiyi (gun boyu 30/dk) tutamaz; butce dolunca NPC'ler yedek replige duser,
	// oyun etkilenmez. 0 = sinirsiz. Host yeniden baslayinca sifirlanir.
	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "0"))
	int32 MaxSessionRequests = 400;

	// Bu oturumda atilan toplam LLM istegi sayisi (yalniz sunucuda artar).
	int32 SessionRequestCount = 0;

	// Sunucu basina GUNLUK toplam LLM cagrisi tavani (oturum-otesi). Sayac UTC gun degisince sifirlanir ve
	// Saved/MMO/LLMDailyBudget.txt'te saklanir: host gun icinde yeniden baslasa da toplam korunur
	// (MaxSessionRequests yalniz tek sureci sinirlar). Tavan dolunca istek atilmaz, NPC yedek replige duser.
	// SAYAC KURALI (maliyet): yalnizca saglayicinin yanit URETMIS olabilecegi istekler sayilir. HTTP 200
	// (ayristirma hatasi dahil) ve ZAMAN ASIMI sayilir (istek saglayiciya ulasmis olabilir, faturalanmis
	// olabilir); baglanti hatasi, iptal, HTTP != 200 (401/403/429/5xx...) ve baslatilamayan istek IADE edilir.
	// Dakikalik sinir iade edilmedigi icin hatali istek donguleri yine de sinirlidir.
	// Ayar onceligi: komut satiri -MMOLlmDaily=N > DefaultGame.ini [MMO] LlmDailyRequests=N > varsayilan 1500
	// (maliyet/denge karari sizin); 0 = sinirsiz. Dosya sayaci tek surece aittir: ayni Saved/ dizinini
	// paylasan iki host sureci birbirini ezer (son yazan kazanir).
	UPROPERTY(EditAnywhere, Category = "MMO|AI", meta = (ClampMin = "0"))
	int32 MaxDailyRequests = 1500;

	// Bugun (UTC) atilan toplam LLM istegi sayisi (yalniz sunucuda artar).
	int32 DailyRequestCount = 0;

	// Oyuncu basi gunluk kota: tek oyuncu herkesin NPC'sini yedek replige dusurmesin. Tavanin 1/4'u, en az 50
	// (MaxDailyRequests = 0 ise kota da yok). Oyuncu = PlayerState adi (kimlik zaafi bilinen bosluk).
	int32 GetPerPlayerDailyQuota() const
	{
		return MaxDailyRequests > 0 ? FMath::Max(50, MaxDailyRequests / 4) : 0;
	}

protected:
	// Gunluk butce durumu: ilk kullanimda dosyadan yuklenir, gun degisince sifirlanir.
	int64 DailyBudgetDay = -1;
	bool bDailyBudgetLoaded = false;

	// Oyuncu anahtari (kucuk harf) -> bugunku istek sayisi. Boyut tavani asilirsa yeni anahtar izlenmez
	// (kota uygulanmaz, genel tavan korur) — sahte adlarla bellek sisirilemesin.
	TMap<FString, int32> PlayerDailyCounts;
	static constexpr int32 MaxTrackedPlayers = 4096;

	// Sayac dosyasi yazim sikligi: degisiklik biriktikce en fazla her 10 cagrida / 30 sn'de bir diske iner,
	// ayrica Deinitialize'da (kapanista) zorla yazilir. Yazim gecici dosya + tasima ile yapilir.
	bool bBudgetDirty = false;
	int32 UnsavedBudgetChanges = 0;
	double LastBudgetSaveTime = 0.0;

	// Ini / komut satirindan MaxDailyRequests'i okur (Initialize'da).
	void LoadDailyLimitConfig();

	// Gunluk sayaci yukler/sifirlar (gun degistiyse). SendRequest'in basinda cagrilir.
	void RollDailyBudget();
	// Degisikligi isaretler; sik yazimi sinirlar (bForce = hemen yaz).
	void NoteBudgetChanged(bool bForce = false);
	void WriteDailyBudgetFile();
	// Sayac anahtarini normallestirir (kucuk harf, sekme/satir sonu yok, <= 64 karakter, bos -> "?").
	static FString NormalizePlayerKey(const FString& PlayerKey);
	// Atilmis ama saglayicinin yanit uretmedigi bir istegi sayaclardan dusurur (Session + Daily + oyuncu).
	// RequestDay != bugunse gun sayaclari dokunulmaz (gun donmus, yeni gunun sayaci yanlis dusmesin).
	void RefundRequest(const FString& PlayerKey, int64 RequestDay);

	// Hiz siniri durumu (yalniz bu subsystem'de, replike degil).
	TArray<double> RecentRequestTimes;
	int32 InFlightRequests = 0;

	/** Istegi kurar ve gonderir. */
	void SendRequest(const FString& SystemPrompt, const FString& UserPrompt, FMMOOnDialogueReady OnReady,
		const FString& PlayerKey);

	/** Anthropic Messages yanitindan gorunur metni cikarir. Basarisizsa OutError doldurulur. */
	static bool ParseAnthropicResponse(const FString& Body, FString& OutText, FString& OutError);

	/** NVIDIA/OpenAI /chat/completions yanitindan gorunur metni cikarir. */
	static bool ParseOpenAIResponse(const FString& Body, FString& OutText, FString& OutError);
};
