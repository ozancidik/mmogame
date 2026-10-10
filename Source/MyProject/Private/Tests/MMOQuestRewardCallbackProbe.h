#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOQuestRewardCallbackProbe.generated.h"

// Reflected listeners exercise the real synchronous gameplay delegates.
UCLASS()
class UMMOQuestRewardCallbackProbe : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> Callback;
	int32 Calls = 0;
	void Fire() { ++Calls; if (Calls == 1 && Callback) Callback(); }
	UFUNCTION() void OnGold(int32 Gold) { Fire(); }
	UFUNCTION() void OnXP(int32 XP, int32 NextLevel) { Fire(); }
	UFUNCTION() void OnInventory() { Fire(); }
	UFUNCTION() void OnLevel(int32 Level) { Fire(); }
};
