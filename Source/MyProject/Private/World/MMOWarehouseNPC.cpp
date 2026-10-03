// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOWarehouseNPC.h"

AMMOWarehouseNPC::AMMOWarehouseNPC()
{
	// Govde toprak/kahve (depocu belli olsun); mesh/animasyon/collision temel sinifta kurulur.
	NpcTint = FLinearColor(0.62f, 0.42f, 0.22f);

	WarehouseName = TEXT("Depocu Rana");

	GreetingLines = {
		TEXT("Hos geldin. Canta agir mi geldi? Birak burada, kimse dokunmaz."),
		TEXT("Her esyanin defterde yeri var. Ne getirdiysen, aldiginda ayni bulursun."),
		TEXT("Kis'ta da Yaz'da da ayni defter: buraya biraktigin her yerde seni bekler."),
		TEXT("Yol uzun, sirt kisa. Fazlasini bana birak, hafif yuru.")
	};
}
