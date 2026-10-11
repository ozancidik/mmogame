// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/MMOCharacterStore.h"
#include "MMOCharacterStoreSubsystem.generated.h"

/**
 * Karakter magazasina erisimin tek noktasi. Varsayilan: yerel dosya magazasi
 * (FMMOLocalFileCharacterStore). Ileride bulut magazasi burada secilir; cagiranlar degismez.
 * Kullanim: if (IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(this)) { ... }
 */
UCLASS()
class MYPROJECT_API UMMOCharacterStoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// WorldContext gecersizse ya da GameInstance yoksa nullptr (cagiran basarisizlik dalina gecer).
	static IMMOCharacterStore* Get(const UObject* WorldContext);

	IMMOCharacterStore* GetStore() const { return Store.Get(); }
#if !UE_BUILD_SHIPPING
	// Testler: magazayi gecici olarak degistirir (ornek: Exists cagrilarini sayan sarmalayici). Onceki magazayi dondurur;
	// cagiran ayni karede geri koyar. Uretim kodu cagirmaz.
	TUniquePtr<IMMOCharacterStore> SwapStoreForTest(TUniquePtr<IMMOCharacterStore> NewStore) { Swap(Store, NewStore); return NewStore; }
#endif

private:
	TUniquePtr<IMMOCharacterStore> Store;
};
