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
	/**
	 * NPC icin tek seferlik bir replik uretir.
	 * @param NpcName    NPC'nin adi (or. "Satici Merek")
	 * @param NpcRole    NPC'nin rolu/kisiligi (or. "kasabanin silah saticisi, aksi ama durust")
	 * @param PlayerInfo Oyuncu baglami (seviye, sinif, aktif gorev) — replige renk katar
	 * @param OnReady    Cevap hazir olunca cagrilir (oyun is parcaciginda)
	 */
	void RequestNpcLine(const FString& NpcName, const FString& NpcRole,
		const FString& PlayerInfo, FMMOOnDialogueReady OnReady);

	/** Serbest bir soru sorar (test/konsol icin). */
	void RequestRaw(const FString& SystemPrompt, const FString& UserPrompt, FMMOOnDialogueReady OnReady);

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

protected:
	// Hiz siniri durumu (yalniz bu subsystem'de, replike degil).
	TArray<double> RecentRequestTimes;
	int32 InFlightRequests = 0;

	/** Istegi kurar ve gonderir. */
	void SendRequest(const FString& SystemPrompt, const FString& UserPrompt, FMMOOnDialogueReady OnReady);

	/** Anthropic Messages yanitindan gorunur metni cikarir. Basarisizsa OutError doldurulur. */
	static bool ParseAnthropicResponse(const FString& Body, FString& OutText, FString& OutError);

	/** NVIDIA/OpenAI /chat/completions yanitindan gorunur metni cikarir. */
	static bool ParseOpenAIResponse(const FString& Body, FString& OutText, FString& OutError);
};
