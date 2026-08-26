// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MMOPlayerController.generated.h"

class AMMOCharacter;
class AMMOTalkerNPC;

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

	// HUD imleci icin: su an bir dusmani hedefliyor muyuz (kilic imleci)?
	UFUNCTION(BlueprintPure, Category = "MMO")
	bool IsInAttackMode() const;

	// Z ile secilen hedef (HUD hedef bari icin). Olu/gecersizse nullptr.
	UFUNCTION(BlueprintPure, Category = "MMO")
	AMMOCharacter* GetSelectedTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// --- Konusan NPC diyalogu (LLM) ---
	// Istemci NPC'ye tiklar -> sunucuya sorar. Sunucu mesafeyi dogrular, LLM'den (veya yedek
	// replikten) metin uretir, sonra sadece istekte bulunan istemciye ClientReceive... ile gonderir.
	UFUNCTION(Server, Reliable)
	void ServerRequestNpcDialogue(AMMOTalkerNPC* Npc);

	UFUNCTION(Client, Reliable)
	void ClientReceiveNpcDialogue(const FString& SpeakerName, const FString& Text);

	// Ayni NPC'ye tekrar tekrar spam'i engellemek icin son istek zamani.
	float LastNpcTalkRequestTime = -100.f;

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
