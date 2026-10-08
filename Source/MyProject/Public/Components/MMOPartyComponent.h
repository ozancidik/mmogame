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

	// Parti lideri (partiyi kuran; o ayrilirsa kalanlardan birine gecer). Partisizken null.
	// Partideki HER uyenin bileseninde ayni karakteri gosterir.
	UPROPERTY(ReplicatedUsing = OnRep_Leader, BlueprintReadOnly, Category = "MMO|Party")
	TObjectPtr<AMMOCharacter> Leader;

	UFUNCTION(BlueprintPure, Category = "MMO|Party")
	bool IsInParty() const { return Members.Num() > 0; }

	// Owner-only server identities: session shared by the group; membership changes on rejoin/transfer.
	// They are request context, not an atomic snapshot of the displayed roster.
	UPROPERTY(Replicated)
	FGuid PartySessionId;
	UPROPERTY(Replicated)
	FGuid PartyMembershipId;

	// Sahibi bu partinin lideri mi? (Sadece lider uye cikarabilir.)
	UFUNCTION(BlueprintPure, Category = "MMO|Party")
	bool IsPartyLeader() const;

	// Menzildeki (Origin'e Range icinde) parti uyelerini + sahibi dondurur (XP paylasimi icin).
	void GetPartyInRange(const FVector& Origin, float Range, TArray<AMMOCharacter*>& Out) const;

	// --- Sahip istemciden cagrilir (sunucuya yonlenir) ---
	void RequestInviteNearest();
	void RequestAccept();
	void RequestAcceptSeen(uint64 SeenInviteId);
	void RequestLeave();
	void RequestLeaveSeen(const FGuid& SeenMembershipId);
	// Gelen parti davetini reddeder (davetciye bildirim gider).
	void RequestDecline();
	void RequestDeclineSeen(uint64 SeenInviteId);
	// Bir uyeyi partiden cikarir (sunucu sadece lider cagirdiysa uygular).
	void RequestKick(AMMOCharacter* Target);

	// Bekleyen daveti (sadece sunucu) ve daveti gonderen.
	TWeakObjectPtr<AMMOCharacter> PendingInviteFrom;
	// Recipient generation survives consumption. Only this pawn's owner receives it.
	UPROPERTY(Replicated)
	uint64 PendingInviteId = 0;

	// Davetin gonderildigi zaman (sunucu World time) — kabul aninda susre/mesafe
	// dogrulamasi icin (davet edenin cok uzaklasmis veya cok eski bir davet olmasi).
	float PendingInviteTime = -1.f;

#if !UE_BUILD_SHIPPING
	// Controllersiz component + native controller chat fixtures; no transport/Slate/durable save proof.
	static bool RunMergeConsentSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

protected:
	UFUNCTION()
	void OnRep_Members();

	UFUNCTION()
	void OnRep_Leader();

	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	AMMOCharacter* GetOwnerCharacter() const;
	AMMOCharacter* FindNearestOtherPlayer(float MaxRange) const;
	bool SetPendingInviteServer(AMMOCharacter* Inviter);

	// Davet spam korumasi (sunucu-otoriter): art arda MMOInvite cagrisinin hedefin
	// ekranini bildirimle doldurmasini engeller.
	float LastInviteTime = -100.f;
	static constexpr float InviteCooldown = 3.f;

	// Bekleyen bir davetin gecerlilik suresi (kabul aninda kontrol edilir).
	// 30 sn: kotu niyetli birinin hedefin davet kutusunu uzun sure isgal etmesini sinirlar (ticaretle ayni).
	static constexpr float InviteExpirySeconds = 30.f;
	// Kabul aninda davet edenle aralarindaki izin verilen azami mesafe (davet menzilinden
	// (1600) biraz genis tutulur ki dogal hareket reddi tetiklemesin).
	static constexpr float InviteAcceptMaxDistance = 3000.f;

	// Sunucu tarafi parti islemleri.
	void FormOrJoin(AMMOCharacter* Inviter, AMMOCharacter* Invitee);
	// Mevcut iki grubu birlestirme, her grubun liderinin onayini gerektirir.
	static bool CanCombineParties(AMMOCharacter* Inviter, AMMOCharacter* Invitee);
	// Bir karakteri partiden cikarir. Normal ayrilma da, lider tarafindan cikarilma (kick)
	// da bunu kullanir — davranis ikisinde de ayni (kalanlar yeniden duzenlenir, tek kisi
	// kalirsa o da partisiz olur).
	static void LeavePartyServer(AMMOCharacter* Leaver);

	// Verilen tam uye kumesini her uyenin Members listesine (kendisi haric) yazar ve
	// hepsine ayni lideri isler.
	static void ApplyPartySet(const TArray<AMMOCharacter*>& FullSet, AMMOCharacter* NewLeader, const FGuid& SessionId);

	// Daveti sunucu cozer (istemci diger oyuncularin controller'larini goremez).
	UFUNCTION(Server, Reliable)
	void ServerInviteNearest();

	UFUNCTION(Server, Reliable)
	void ServerAccept(uint64 SeenInviteId);

	UFUNCTION(Server, Reliable)
	void ServerDecline(uint64 SeenInviteId);

	UFUNCTION(Server, Reliable)
	void ServerLeave(const FGuid& SeenMembershipId);

	// Uye cikarma; sunucu Leader == cagiran mi diye dogrular.
	UFUNCTION(Server, Reliable)
	void ServerKick(AMMOCharacter* Target);

	// Davet edilene ekranda bildirim gonder.
	UFUNCTION(Client, Reliable)
	void ClientNotifyInvited(AMMOCharacter* From);

	// Partiden cikarilana ekranda bildirim gonder.
	UFUNCTION(Client, Reliable)
	void ClientNotifyKicked();
};
