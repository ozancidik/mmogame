// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOHealerNPC.h"

AMMOHealerNPC::AMMOHealerNPC()
{
	// Govde acik mor/lavanta (sifaci belli olsun); mesh/animasyon/collision temel sinifta kurulur.
	NpcTint = FLinearColor(0.62f, 0.45f, 0.95f);

	HealerName = TEXT("Sifaci Lyra");

	// Karsilama/onay replikleri (docs/NpcDialogue.md — sefkatli, yumusak sesli tapinak sifacisi).
	// Iyilestirme basarili olunca bunlardan biri onay balonu olarak gosterilir.
	GreetingLines = {
		TEXT("Gel, yorgun yolcu. Burada yaralarin da, yukun de hafifler."),
		TEXT("Korkma, isik hala seninle. Ben yalniz onun bir daha parlamasina yardim ederim."),
		TEXT("Nefes al, derince. Sifa cogu zaman ilactan once sukunetle baslar."),
		TEXT("Kanin durdu, ruhun da dinlensin. Otur, biraz da onu iyilestirelim."),
		TEXT("Git artik sag salim, ama unutma: her yara sana bir daha yasadigini fisildar."),
		TEXT("Yaran derin ama umudun daha derin olsun; iyilesmek de bir cesarettir."),
		TEXT("Tanricanin huzuru yolunu aydinlatsin; sen sag ol yeter, gerisi gelir.")
	};
}
