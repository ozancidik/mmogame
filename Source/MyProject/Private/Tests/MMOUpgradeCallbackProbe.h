#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOUpgradeCallbackProbe.generated.h"

// Actual dynamic gold delegate listener; instantiated only by the headless dev fixture.
UCLASS()
class UMMOUpgradeCallbackProbe : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> Callback;
	int32 Calls = 0;
	UFUNCTION()
	void OnGold(int32 NewGold)
	{
		++Calls;
		if (Calls == 1 && Callback) Callback();
	}
};
