// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Types/SlateEnums.h"
#include "MMOPlayerController.generated.h"

class AMMOCharacter;
class AMMOTalkerNPC;
class AMMOHealerNPC;

/**
 * Tikla-git / tikla-saldir kontrol semasi (klasik MMO tarzi).
 * - Sol tik: zemine tiklaninca oraya yuru; dusmana tiklaninca ona gidip menzilde saldir.
 * - Sag tik basili: kamerayi dondur (basili degilken fare sadece imlec).
 * OS imleci gizli; HUD kendi imlecini cizer (normal ucgen / saldiri modunda kilic).
 */
UCLASS()
class MYPROJECT_API AMMOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMMOPlayerController();

	// Yalniz listen server hostu: butun oturumu Summer/Winter arasinda tasir.
	UFUNCTION(Exec)
	void MMOTravel();

	// HUD imleci icin: su an bir dusmani hedefliyor muyuz (kilic imleci)?
	UFUNCTION(BlueprintPure, Category = "MMO")
	bool IsInAttackMode() const;

	// Z ile secilen hedef (HUD hedef bari icin). Olu/gecersizse nullptr.
	UFUNCTION(BlueprintPure, Category = "MMO")
	AMMOCharacter* GetSelectedTarget() const;

	// Sunucu (ya da yerel kapi) hedefe saldiriyi reddetti (PvP kapali / parti-klan / hub / dogum korumasi):
	// otomatik saldiriyi ve kovalamayi durdurur. SelectedTarget kalir (secili ama saldirilmaz);
	// ayni hedefe kisa sure tekrar tikla-kovala kurulmaz (basili tutulan sol tus dongusu).
	void ReleaseAttackTarget(const AMMOCharacter* Rejected);

	// --- Sohbet (KO tarzi genel + parti) ---
	// Enter: sol altta yazi kutusu acar; Enter gonderir, Esc/odak kaybi kapatir.
	// "/p mesaj" yalniz parti uyelerine gider. Konsol yedegi: "Say merhaba".
	UFUNCTION(Exec)
	void Say(const FString& Message);

	// Sunucu -> istemci: sohbet satiri. Channel 0 = genel, 1 = parti, 2 = sistem, 3 = gelen fisilti, 4 = giden fisilti, 5 = klan.
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FString& Sender, const FString& Message, uint8 Channel);

	bool IsChatInputOpen() const { return ChatBox.IsValid(); }

	// Server telemetry only, after successful quest mutation. No client RPC or saved progress.
	bool RecordFirstQuestMetric(FName Action, FName QuestId);
	// After a real pickup grant; Amount is delivered count/gold, not requested amount.
	bool RecordFirstLootMetric(FName Kind, FName ItemId, int32 Amount);
	// Ilk sunucu kill kredisi; fatal/contribution/party/fallback ayrimi, XP oncesi seviye.
	bool RecordFirstKillMetric(FName MobId, int32 MobLevel, int32 XP, FName Credit, bool bElite);
	// After a successful equip request; starter setup and save restore are excluded.
	bool RecordFirstEquipMetric(FName ItemId, int32 InstanceId, int32 Slot);
	// Accepted authoritative cast after mana payment, before effects; not first damage.
	bool RecordFirstSkillMetric(FName SkillId);
	// Accepted authoritative basic attack before damage/XP; not a damage or input counter.
	bool RecordFirstBasicAttackMetric(FName TargetKind, FName TargetId, int32 TargetLevel);
	// Observed server death after XP penalty; None means unknown/non-mob source, not PvP attribution.
	bool RecordFirstDeathMetric(FName KillerMobId, int32 XPLoss);
#if !UE_BUILD_SHIPPING
	static bool RunFirstQuestMetricsSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	// Native controller chat bodies/owning pawn context; no network transport or Slate input.
	static bool RunTradeChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunPartyChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunClanChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

protected:
	bool RecordFirstActionMetric(FName Action, const FString& Payload);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SeamlessTravelTo(APlayerController* NewPC) override;

#if !UE_BUILD_SHIPPING
	FGuid FirstActionMetricsSession;
	double FirstActionMetricsStart = -1.0;
	TSet<FName> RecordedFirstActionMetrics;
#endif

	UFUNCTION(Server, Reliable)
	void ServerSendChat(const FString& Message, AMMOCharacter* SeenCommandPawn, uint64 SeenTradeInviteId, uint64 SeenActiveTradeSessionId, uint64 SeenPartyInviteId, uint64 SeenClanInviteId, const FGuid& SeenPartyMembershipId);
	// Capture party/trade/clan command context before the owning client sends chat.
	void SendChatWithContext(const FString& Message);

	// Sunucu tarafi sohbet spam korumasi.
	float LastChatServerTime = -100.f;

	void OnChatKey();
	void OpenChatInput();
	void CloseChatInput();
	void OnChatCommitted(const FText& Text, ETextCommit::Type CommitType);

	TSharedPtr<class SWidget> ChatContainer;
	TSharedPtr<class SEditableTextBox> ChatBox;
	// Enter'in ayni tusa basisi yazi kutusuna gitmesin, kapatma da kendi callback'i
	// icinde widget'i yok etmesin diye ac/kapa bir sonraki PlayerTick'te yapilir.
	bool bChatOpenRequested = false;
	bool bChatCloseRequested = false;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// --- Konusan NPC diyalogu (LLM) ---
	// Istemci NPC'ye tiklar -> sunucuya sorar. Sunucu mesafeyi dogrular, LLM'den (veya yedek
	// replikten) metin uretir, sonra sadece istekte bulunan istemciye ClientReceive... ile gonderir.
	UFUNCTION(Server, Reliable)
	void ServerRequestNpcDialogue(AMMOTalkerNPC* Npc);

	UFUNCTION(Client, Reliable)
	void ClientReceiveNpcDialogue(const FString& SpeakerName, const FString& Text);

	// --- Sifaci NPC (Sifaci Lyra) ---
	// Istemci sifaciya tiklar -> sunucuya iyilestirme ister. Sunucu mesafeyi + altini dogrular,
	// yeterliyse altini duser ve tam iyilestirir; sonucu ClientReceiveNpcDialogue balonuyla bildirir.
	UFUNCTION(Server, Reliable)
	void ServerRequestHeal(AMMOHealerNPC* Npc);

	// Istemci-tarafi spam korumasi (yalnizca UX; ust uste tiklamada istek atmaz).
	float LastNpcTalkRequestTime = -100.f;

	// SUNUCU-tarafi throttle (guvenlik): hileli istemci ServerRequestNpcDialogue'u
	// bombardiman edip her cagrida gercek bir LLM istegi (para) tetikleyemesin diye.
	// Host'ta (listen server) istemci+sunucu ayni obje oldugundan istemci damgasindan AYRI tutulur.
	float LastNpcTalkServerTime = -100.f;

	// Oyuncu basina en fazla dakikada 12 LLM istegi (SECURITY_AUDIT Y-4); sunucu geneli tavan ve
	// oturum butcesi UMMODialogueSubsystem'de.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|AI")
	float NpcTalkServerCooldown = 5.f;

	// Sifaci icin ayni cift-katman throttle (talker'daki desenin ayni si). Bug-hunt bulgusu:
	// sol tus basili tutulunca DoLeftClickAction HER KAREDE calisiyor; sifacida hic throttle
	// yoktu -> DoT ile tam-can arasinda gidip gelen oyuncu her karede 100 altin dusurebiliyordu
	// (+ reliable RPC'yi kare basina bombardimana tutuyordu).
	float LastHealRequestTime = -100.f;
	float LastHealServerTime = -100.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|AI")
	float HealServerCooldown = 1.f;

	void OnLeftClickPressed();
	void OnLeftClickReleased();

	// Sol-tik "tek dokunus" (surukleme yapilmadan birakildi) eylemi: tikla-git / NPC ac / saldiri hedefi.
	void DoLeftClickAction();

	// Karakterin hareket yonune donmesini ac/kapa.
	// WASD'de KAPALI (bakis yonu sabit, yana kayma), tikla-git'te ACIK (yola donerek yurur).
	void SetOrientToMovement(bool bOrient);
	void OnRightMousePressed();
	void OnRightMouseReleased();
	void OnMouseX(float Value);
	void OnMouseY(float Value);
	void OnToggleSkillBook();
	void OnToggleInventory();
	void OnToggleCharacter();
	void OnCloseShop();

	// Space: ziplama (Enhanced Input mapping'leri temizlendigi icin C++ ile suruyoruz).
	void OnJumpPressed();
	void OnJumpReleased();

	// Z: en yakin dusmani sec (KO: KM_TARGET_NEAREST_ENEMY -> TargetSelect + RotateTo).
	void OnTargetNearestEnemy();

	// R: secili hedefe surekli saldiriyi ac/kapa (KO: KM_TOGGLE_ATTACK toggle).
	void OnToggleAttack();

	// KO'nun TargetSelect'i: hedefi kaydeder; hedef degistiyse auto-attack iptal; karakteri hedefe dondurur.
	void SelectTarget(AMMOCharacter* NewTarget);

	// Navmesh var mi (yol bulma kullanilabilir mi)? Yoksa duz-cizgi harekete dusulur.
	bool HasNavmesh() const;

	// Hedefe/konuma dogru hareket ettir: navmesh varsa yol bulma (throttle'li), yoksa duz-cizgi.
	void DrivePawnTowardLocation(const FVector& Dest);
	void DrivePawnTowardActor(AActor* Goal);

	// Skill kitabi acik mi (HUD'dan).
	bool IsSkillBookOpen() const;

	// Herhangi bir UI penceresi (kitap/envanter) acik mi.
	bool IsAnyUIWindowOpen() const;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Control")
	float MoveAcceptanceRadius = 110.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Control")
	float AttackReach = 180.f;

	// Auto-attack hedefi (kovala + vur dongusu). KO'daki m_bAttackContinous karsiligi: bu gecerliyse saldiri acik.
	TWeakObjectPtr<AMMOCharacter> TargetEnemy;

	// Z ile secilen hedef (KO'daki m_iIDTarget). Saldiri baslatmaz; R ile saldiriya gecilir.
	TWeakObjectPtr<AMMOCharacter> SelectedTarget;

	// Saldirisi reddedilen hedef ve zamani (ReleaseAttackTarget). RejectedTargetSeconds boyunca
	// ayni hedefe tiklamak yalniz secer, kovalamaz.
	TWeakObjectPtr<AMMOCharacter> RejectedTarget;
	float RejectedTargetTime = -100.f;
	static constexpr float RejectedTargetSeconds = 3.f;

	// Z ile hedef arama menzili.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Control")
	float TargetSearchRange = 3000.f;

	FVector MoveToLocation = FVector::ZeroVector;
	bool bHasMoveTarget = false;
	bool bRotatingCamera = false;

	// Sol tus: basili-tut/dokun = imlecin oldugu yere git (kamerayi ASLA cevirmez).
	// Kamera cevirme sadece sag tus basiliyken (bRotatingCamera) yapilir.
	bool bLeftPressed = false; // sol tus su an basili (pencere yokken)

	// Navmesh yol bulmayi cok sik yeniden hesaplamamak icin son istek zamani.
	float LastPlayerRepathTime = -100.f;
};
