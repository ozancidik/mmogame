// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOFriendsComponent.generated.h"

class AMMOCharacter;

/**
 * Sunucu-otoriteli, karsilikli onayli arkadas listesi bileseni.
 * Karakter basina kayitli arkadas adlari tutulur (FMMOCharacterSaveData::Friends).
 * Komutlar sohbetten gelir (AMMOPlayerController::ServerSendChat sunucuda calisir),
 * bu yuzden ayri RPC yok: Cmd* fonksiyonlari yalniz otoritede calisir.
 * Liste yalniz sahibine replike olur; HUD yok (yalniz sohbet satirlari).
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOFriendsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOFriendsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Kayitli arkadas adlari. Yalniz sahibine replike.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MMO|Friends")
	TArray<FString> Friends;

	static constexpr int32 MaxFriends = 30;
	static constexpr int32 MaxNameLength = 24;

	// Ad listede mi (buyuk/kucuk harf duyarsiz).
	bool HasFriend(const FString& Name) const;

	// --- Sohbet komutlari (yalniz sunucu/otorite; ServerSendChat icinden cagrilir) ---
	void CmdAdd(const FString& Name);
	void CmdAccept();
	void CmdRemove(const FString& Name);
	void CmdList();

	// Sahibi cevrimici oldu/cikti: listesinde bu karakter olan diger cevrimici oyunculara sistem satiri.
	// Yalniz otorite. (Giris: AMMOCharacter::AutoLoadDeferred, cikis: AMMOCharacter::EndPlay.)
	// NameOverride bos degilse o ad icin duyurur (ad degisiminde eski adi cevrimdisi yapmak icin).
	// Ayni durum tekrar duyurulmaz (bolge gecisinde sahte "cevrimici oldu" yok; QA ORTA-1).
	void AnnouncePresence(bool bOnline, const FString& NameOverride = FString()) const;

	// --- Kalicilik (yalniz otorite) ---
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

protected:
	bool HasAuthorityInternal() const;
	AMMOCharacter* GetOwnerCharacter() const;

	// Sahibin ekranina sistem sohbet satiri gonderir.
	void NotifyOwner(const FString& Text) const;

	// Ekle/sil sonrasi tam kayit en fazla 5 sn'de bir; arada otomatik kayit (60 sn) tutar (QA ORTA-4).
	float LastFriendSaveTime = -100.f;
	void SaveThrottled(AMMOCharacter* Me);
	static constexpr float FriendSaveCooldown = 5.f;

	// Yalniz sunucuda tutulan, kisa omurlu arkadaslik istegi. Kayda/replikasyona girmez;
	// kabul edilince iki oyuncunun listesi birlikte guncellenir.
	TWeakObjectPtr<AMMOCharacter> PendingFriendInviter;
	float PendingFriendInviteExpiry = -1.f;
	static constexpr float FriendInviteDuration = 20.f;
};
