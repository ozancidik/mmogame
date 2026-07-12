// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOPartyComponent.generated.h"

class AMMOCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnPartyChanged);

/**
 * Oyuncu parti (grup) bileseni. Sunucu-otoriteli.
 * Her uyenin bileseni, KENDISI HARIC diger parti uyelerini tutar (Members bos -> partisiz).
 * Davet -> kabul ile parti kurulur; mob oldurunce menzildeki parti uyeleri XP/gorev kredisi alir.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOPartyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOPartyComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Parti degisince HUD'u tazelemek icin.
	UPROPERTY(BlueprintAssignable, Category = "MMO|Party")
	FMMOOnPartyChanged OnPartyChanged;

	// Diger parti uyeleri (kendisi haric). Bos -> partisiz.
	UPROPERTY(ReplicatedUsing = OnRep_Members, BlueprintReadOnly, Category = "MMO|Party")
	TArray<TObjectPtr<AMMOCharacter>> Members;

	UFUNCTION(BlueprintPure, Category = "MMO|Party")
	bool IsInParty() const { return Members.Num() > 0; }

	// Menzildeki (Origin'e Range icinde) parti uyelerini + sahibi dondurur (XP paylasimi icin).
	void GetPartyInRange(const FVector& Origin, float Range, TArray<AMMOCharacter*>& Out) const;

	// --- Sahip istemciden cagrilir (sunucuya yonlenir) ---
	void RequestInviteNearest();
	void RequestAccept();
	void RequestLeave();

	// Bekleyen daveti (sadece sunucu) ve daveti gonderen.
	TWeakObjectPtr<AMMOCharacter> PendingInviteFrom;

protected:
	UFUNCTION()
	void OnRep_Members();

	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	AMMOCharacter* GetOwnerCharacter() const;
	AMMOCharacter* FindNearestOtherPlayer(float MaxRange) const;

	// Sunucu tarafi parti islemleri.
	void FormOrJoin(AMMOCharacter* Inviter, AMMOCharacter* Invitee);
	void LeavePartyServer(AMMOCharacter* Leaver);

	// Verilen tam uye kumesini her uyenin Members listesine (kendisi haric) yazar.
	static void ApplyPartySet(const TArray<AMMOCharacter*>& FullSet);

	// Daveti sunucu cozer (istemci diger oyuncularin controller'larini goremez).
	UFUNCTION(Server, Reliable)
	void ServerInviteNearest();

	UFUNCTION(Server, Reliable)
	void ServerAccept();

	UFUNCTION(Server, Reliable)
	void ServerLeave();

	// Davet edilene ekranda bildirim gonder.
	UFUNCTION(Client, Reliable)
	void ClientNotifyInvited(AMMOCharacter* From);
};
