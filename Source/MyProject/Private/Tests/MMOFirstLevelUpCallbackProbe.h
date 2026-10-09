#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOFirstLevelUpCallbackProbe.generated.h"

// Native fixture listener for the actual BlueprintAssignable refill delegate.
// No RPC, replicated property or persisted state; unused in Shipping.
UCLASS()
class UMMOFirstLevelUpCallbackProbe : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> Callback;
	bool bFired = false;

	UFUNCTION()
	void OnRefill(float NewHealth, float MaxHealth)
	{
		FireOnce();
	}

	UFUNCTION()
	void OnLevel(int32 NewLevel)
	{
		FireOnce();
	}

	UFUNCTION()
	void OnExperience(int32 CurrentExperience, int32 ExperienceToNextLevel)
	{
		FireOnce();
	}

private:
	void FireOnce()
	{
		if (bFired) return;
		bFired = true; // All delegate routes share this gate, including nested broadcasts.
		if (Callback) Callback();
	}
};
