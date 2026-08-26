// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOQuestGiverNPC.h"

AMMOQuestGiverNPC::AMMOQuestGiverNPC()
{
	// Govde altin/amber ton (gorevli belli olsun); mesh/animasyon/collision temel sinifta kurulur.
	NpcTint = FLinearColor(0.95f, 0.75f, 0.1f);

	NpcName = TEXT("Yasli Doran");

	// Karsilama replikleri (docs/NpcDialogue.md — bilge, sakin, bilmece gibi konusan yasli).
	GreetingLines = {
		TEXT("Otur da dinlen, yolcu. En uzun yollari hep en yorgun ayaklar yurur."),
		TEXT("Bir isim var sana; korkma, tasiyabileceginden agirini omzuna koymam."),
		TEXT("Sabret. Tohum topragin altinda curudugunu sanirken aslinda kok salar."),
		TEXT("Yolun catallandiginda dur, dinle; dogru yon cogu zaman sessizce fisildar."),
		TEXT("Git simdi, donesin diye. Her veda, yeni bir oykunun ilk satiridir.")
	};
}
