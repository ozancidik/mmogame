#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOGoldMetricProbe.generated.h"

UCLASS()
class UMMOGoldMetricProbe : public UObject
{
	GENERATED_BODY()
public:
	TArray<int32> Values;
	TFunction<void(int32)> Callback;
	UFUNCTION() void OnGold(int32 Gold)
	{
		Values.Add(Gold);
		if (Callback) Callback(Values.Num());
	}
};
