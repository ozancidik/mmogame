#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMOTradeTeardownProbe.generated.h"

UCLASS()
class UMMOTradeTeardownProbe : public UObject
{
	GENERATED_BODY()
public:
	int32 Calls = 0;
	TFunction<void(int32)> Callback;
	UFUNCTION() void OnTrade()
	{
		++Calls;
		if (Callback) Callback(Calls);
	}
};
