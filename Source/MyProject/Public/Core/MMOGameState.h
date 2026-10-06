// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MMOGameState.generated.h"

/**
 * MMO oyun durumu: sunucunun herkese acik kurallarini istemcilere replike eder.
 * Su an tek alan: PvP acik mi (NETWORK_AUDIT dedicated engeli 4).
 *
 * TEK KAYNAK AMMOGameMode::bAllowPvP'dir (yalniz sunucuda vardir); buradaki deger onun AYNASIDIR.
 * Yazma yolu: AMMOGameMode::SetPvPAllowed -> SetPvPEnabled. Istemci bunu yalniz OKUR
 * (AMMOCharacter::IsPvPAllowed), karar yine sunucuda verilir.
 */
UCLASS()
class MYPROJECT_API AMMOGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMMOGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Yalniz sunucuda cagrilir (GameMode aynalar).
	void SetPvPEnabled(bool bEnabled);

	bool IsPvPEnabled() const { return bPvPEnabled; }

protected:
	// PvP acik mi (GameMode::bAllowPvP aynasi). Istemci tarafinda on kontrol ve ipucu icin okunur.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MMO|GameState")
	bool bPvPEnabled = false;
};
