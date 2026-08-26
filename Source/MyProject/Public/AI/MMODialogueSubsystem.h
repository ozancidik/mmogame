// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MMODialogueSubsystem.generated.h"

/** LLM cevabi hazir oldugunda cagrilir. bSuccess=false ise Text hata/yedek metindir. */
DECLARE_DELEGATE_TwoParams(FMMOOnDialogueReady, bool /*bSuccess*/, const FString& /*Text*/);

/**
 * Oyun ici NPC diyalogu icin Anthropic Messages API istemcisi.
 *
 * Tasarim:
 *  - SADECE SUNUCUDA calisir. API anahtari istemciye asla gitmez; uretilen metin
 *    Client RPC ile sadece istegi yapan oyuncuya replike edilir.
 *  - Anahtar ANTHROPIC_API_KEY ortam degiskeninden okunur (koda gomulmez, kaydedilmez).
 *  - C++ icin resmi Anthropic SDK'si yok; Unreal'in HTTP modulu ile ham istek atilir.
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

	/** ANTHROPIC_API_KEY tanimli mi? (tanimli degilse istek atmayiz) */
	bool HasApiKey() const;

	// --- Ayarlar ---

	// Kullanilacak model. Oyun ici diyalogda gecikme onemliyse daha kucuk bir modele
	// gecmek OYUNCUNUN karari; varsayilan en yetenekli model.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	FString Model = TEXT("claude-opus-5");

	// Cikti uzunlugu tavani. DIKKAT: dusunme (thinking) + metin BIRLIKTE bu tavana sayilir,
	// bu yuzden kisa replik icin bile bol birakiyoruz; yoksa cevap yarida kesilir.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	int32 MaxTokens = 1024;

	// Dusuk gecikme icin effort=low. (Dusunmeyi tamamen KAPATMIYORUZ: kapaliyken
	// model ara sira <thinking> etiketlerini gorunur metne sizdirabiliyor ki bu
	// oyuncunun ekraninda hata gibi gorunur. Dusuk effort zaten hizli ve ucuz.)
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	FString Effort = TEXT("low");

	// Istek zaman asimi (saniye). Oyun icinde uzun beklememek icin kisa tutuluyor.
	UPROPERTY(EditAnywhere, Category = "MMO|AI")
	float TimeoutSeconds = 12.f;

protected:
	/** Istegi kurar ve gonderir. */
	void SendRequest(const FString& SystemPrompt, const FString& UserPrompt, FMMOOnDialogueReady OnReady);

	/** Yanit govdesinden gorunur metni cikarir. Basarisizsa OutError doldurulur. */
	static bool ParseResponse(const FString& Body, FString& OutText, FString& OutError);
};
