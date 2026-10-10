#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOPurchaseCallbackProbe.generated.h"

UCLASS()
class UMMOPurchaseCallbackProbe : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void(int32)> Callback;
	int32 Calls = 0;
	void Fire(int32 Gold) { ++Calls; if (Calls == 1 && Callback) Callback(Gold); }
	UFUNCTION() void OnGold(int32 Gold) { Fire(Gold); }
	UFUNCTION() void OnInventory() { Fire(-1); }
};
