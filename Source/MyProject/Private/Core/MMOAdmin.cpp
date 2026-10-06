// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOAdmin.h"
#include "GameFramework/PlayerController.h"

bool MMOAdmin::IsServerAdmin(const APlayerController* PC)
{
	// Yalniz sunucu tarafinda anlamlidir; istemci dunyasindaki denetleyici yonetici sayilmaz.
	if (!PC || !PC->HasAuthority())
	{
		return false;
	}
	// Dedicated sunucuda kimse "yerel" degildir: ileride token/hesap dogrulamasi eklenene dek yonetici yok.
	if (PC->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	// Listen-server: ev sahibinin kendi yerel denetleyicisi.
	return PC->IsLocalController();
}
