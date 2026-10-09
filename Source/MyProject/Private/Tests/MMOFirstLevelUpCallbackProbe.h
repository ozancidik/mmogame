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
		if (bFired) return;
		bFired = true; // Nested refill must not recursively grant XP forever.
		if (Callback) Callback();
	}
};
