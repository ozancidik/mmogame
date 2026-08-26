// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOVendorNPC.h"

AMMOVendorNPC::AMMOVendorNPC()
{
	// Govde mavi (satici belli olsun); mesh/animasyon/collision temel sinifta kurulur.
	NpcTint = FLinearColor(0.15f, 0.35f, 0.9f);

	VendorName = TEXT("Satici Merek");

	// Karsilama replikleri (docs/NpcDialogue.md — aksi ama durust silah saticisi).
	GreetingLines = {
		TEXT("Gozunle yiyecegine altinla al, yigit. Bakmak bedava degil ama tezgahim oyle."),
		TEXT("Celigim namusludur; fiyatim da oyle. Pazarlik etme, canimi sikarsin."),
		TEXT("Ucuz zirh ucuz mezar demektir; ben soyledim, gerisi senin bilecegin is."),
		TEXT("Dogru durust para say da sana dogru durust celik vereyim; ticaret budur."),
		TEXT("Suratimi begenmedin, biliyorum. Ama bu sehirde kazik atmayan tek ben kaldim.")
	};
}
