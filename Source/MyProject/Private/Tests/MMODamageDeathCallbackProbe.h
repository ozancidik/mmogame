#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MMODamageDeathCallbackProbe.generated.h"

// Native damage regression listener. No replicated or persisted state.
UCLASS()
class UMMODamageDeathCallbackProbe : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void()> HealthCallback;
    int32 HealthCalls = 0;
    int32 DeathCalls = 0;
    bool bFired = false;

    UFUNCTION()
    void OnHealth(float NewHealth, float MaxHealth)
    {
        ++HealthCalls;
        if (bFired) return;
        bFired = true;
        if (HealthCallback) HealthCallback();
    }

    UFUNCTION()
    void OnDeath() { ++DeathCalls; }
};
