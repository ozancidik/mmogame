// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOMailComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOMailComponent::RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	auto Check = [&](bool Condition, const TCHAR* Label)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_CASE] %s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Label);
		if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Label; }
	};
	const FString Tag = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FString NameA = FString::Printf(TEXT("ZTestMailA_%s"), *Tag);
	const FString NameB = FString::Printf(TEXT("ZTestMailB_%s"), *Tag);
	const FString NameStore = FString::Printf(TEXT("ZTestMailS_%s"), *Tag);
	const FString NameBad = FString::Printf(TEXT("ZTestMailX_%s"), *Tag);

	// --- Ad temizleme ---
	Check(FMMOMailStore::SanitizeName(TEXT("A b/c.d")) == TEXT("A_bcd"), TEXT("name_sanitize_chars"));
	Check(FMMOMailStore::SanitizeName(FString()) == TEXT("Default") && !FMMOMailStore::IsDeliverableName(TEXT("???")) && !FMMOMailStore::IsDeliverableName(TEXT("Default")), TEXT("name_default_not_deliverable"));
	Check(FMMOMailStore::SanitizeName(FString::ChrN(50, TEXT('x'))).Len() == FMMOMailStore::MaxNameLength, TEXT("name_length_cap"));
	Check(FMMOMailStore::GetBoxPath(TEXT("A b")).EndsWith(TEXT("MMOMail/A_b.json")) || FMMOMailStore::GetBoxPath(TEXT("A b")).EndsWith(TEXT("MMOMail\\A_b.json")), TEXT("name_box_path"));

	// --- JSON dogrulama ---
	{
		TArray<FMMOMail> In, Out;
		FMMOMail M; M.From = TEXT("Ali"); M.Text = TEXT("selam"); M.Gold = 250; M.SentUnix = 1700000000;
		In.Add(M);
		FString Json;
		Check(FMMOMailStore::ToJson(In, Json) && FMMOMailStore::FromJson(Json, Out) && Out.Num() == 1, TEXT("json_roundtrip_ok"));
		Check(Out.Num() == 1 && Out[0].From == TEXT("Ali") && Out[0].Text == TEXT("selam") && Out[0].Gold == 250 && Out[0].SentUnix == 1700000000, TEXT("json_roundtrip_equal"));
		Check(!FMMOMailStore::FromJson(TEXT("not json"), Out) && Out.IsEmpty(), TEXT("json_rejects_garbage"));
		FString Hostile = TEXT("{\"mails\":[{\"from\":\"A\",\"text\":\"x\",\"gold\":-5},{\"from\":\"B\",\"text\":\"y\",\"gold\":99999999999},"
			"{\"from\":\"\",\"text\":\"no sender\",\"gold\":1},{\"from\":\"C\",\"text\":\"\",\"gold\":0},{\"from\":\"D\",\"text\":\"");
		Hostile += FString::ChrN(300, TEXT('z'));
		Hostile += TEXT("\",\"gold\":0}]}");
		Check(FMMOMailStore::FromJson(Hostile, Out), TEXT("json_accepts_hostile_shape"));
		Check(Out.Num() == 3 && Out[0].Gold == 0 && Out[1].Gold == FMMOMailStore::MaxGoldPerMail, TEXT("json_clamps_gold_and_drops_invalid"));
		Check(Out.Num() == 3 && Out[2].Text.Len() == FMMOMailStore::MaxTextLength, TEXT("json_truncates_text"));
		FString Many = TEXT("{\"mails\":[");
		for (int32 i = 0; i < 30; ++i) Many += FString::Printf(TEXT("%s{\"from\":\"F\",\"text\":\"m%d\"}"), i ? TEXT(",") : TEXT(""), i);
		Many += TEXT("]}");
		Check(FMMOMailStore::FromJson(Many, Out) && Out.Num() == FMMOMailStore::MaxMailsPerBox, TEXT("json_caps_mail_count"));
	}

	// --- Disk deposu ---
	{
		FMMOMailStore::DeleteBox(NameStore);
		FMMOMail M; M.From = TEXT("Ali"); M.Text = TEXT("bir"); M.Gold = 100;
		Check(FMMOMailStore::Append(NameStore, M) && FMMOMailStore::Count(NameStore) == 1, TEXT("store_append_count"));
		M.Text = TEXT("iki"); M.Gold = 0;
		Check(FMMOMailStore::Append(NameStore, M) && FMMOMailStore::Count(NameStore) == 2, TEXT("store_append_second"));
		TArray<FMMOMail> Taken;
		Check(FMMOMailStore::TakeAll(NameStore, Taken) && Taken.Num() == 2 && Taken[0].Text == TEXT("bir") && Taken[1].Text == TEXT("iki"), TEXT("store_take_all_in_order"));
		Check(FMMOMailStore::Count(NameStore) == 0, TEXT("store_take_empties_box"));
		Check(FMMOMailStore::TakeAll(NameStore, Taken) && Taken.IsEmpty(), TEXT("store_second_take_is_empty_no_regrant"));
		FMMOMail Bad = M;
		Bad.Text = FString::ChrN(FMMOMailStore::MaxTextLength + 1, TEXT('t'));
		FMMOMail Neg = M; Neg.Gold = -1;
		FMMOMail Huge = M; Huge.Gold = FMMOMailStore::MaxGoldPerMail + 1;
		FMMOMail Empty = M; Empty.Text.Reset(); Empty.Gold = 0;
		FMMOMail NoFrom = M; NoFrom.From.Reset();
		Check(!FMMOMailStore::Append(NameStore, Bad) && !FMMOMailStore::Append(NameStore, Neg) && !FMMOMailStore::Append(NameStore, Huge)
			&& !FMMOMailStore::Append(NameStore, Empty) && !FMMOMailStore::Append(NameStore, NoFrom) && !FMMOMailStore::Append(TEXT("Default"), M), TEXT("store_rejects_invalid_mail"));
		Check(FMMOMailStore::Count(NameStore) == 0, TEXT("store_invalid_left_box_empty"));
		M.Text = TEXT("dolgu");
		for (int32 i = 0; i < FMMOMailStore::MaxMailsPerBox; ++i) FMMOMailStore::Append(NameStore, M);
		Check(FMMOMailStore::Count(NameStore) == FMMOMailStore::MaxMailsPerBox && !FMMOMailStore::Append(NameStore, M), TEXT("store_box_full_rejects"));
		FMMOMailStore::DeleteBox(NameStore);
		// Bozuk dosya: .bad'a ayrilir, kutu bos baslar, ekleme yeniden calisir.
		const FString Path = FMMOMailStore::GetBoxPath(NameBad);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(TEXT("{{{ garbage"), *Path);
		TArray<FMMOMail> Loaded;
		Check(FMMOMailStore::Load(NameBad, Loaded) && Loaded.IsEmpty() && IFileManager::Get().FileExists(*(Path + TEXT(".bad"))) && !IFileManager::Get().FileExists(*Path), TEXT("store_corrupt_box_quarantined"));
		Check(FMMOMailStore::Append(NameBad, M) && FMMOMailStore::Count(NameBad) == 1, TEXT("store_recovers_after_corruption"));
		FMMOMailStore::DeleteBox(NameBad);
	}

	// --- Ucret ---
	Check(FeeFor(0) == 10 && FeeFor(1) == 11 && FeeFor(500) == 20 && FeeFor(1000) == 30 && FeeFor(1000000) == 20010, TEXT("fee_formula"));

	// --- Canli bilesen ---
	const FVector Location(1800000, 1800000, 5000);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TStrongObjectPtr<AMMOCharacter> CA(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
	TStrongObjectPtr<AMMOCharacter> CB(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(5000, 0, 0), FRotator::ZeroRotator, Params));
	TStrongObjectPtr<AMMOCharacter> CC(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(10000, 0, 0), FRotator::ZeroRotator, Params));
	UMMOMailComponent* A = CA.IsValid() ? CA->GetMailComponent() : nullptr;
	UMMOMailComponent* B = CB.IsValid() ? CB->GetMailComponent() : nullptr;
	UMMOMailComponent* C = CC.IsValid() ? CC->GetMailComponent() : nullptr;
	UMMOInventoryComponent* InvA = CA.IsValid() ? CA->GetInventory() : nullptr;
	UMMOInventoryComponent* InvB = CB.IsValid() ? CB->GetInventory() : nullptr;
	Check(A && B && C && InvA && InvB, TEXT("live_fixture"));
	if (!(A && B && C && InvA && InvB))
	{
		for (TStrongObjectPtr<AMMOCharacter>* P : {&CA, &CB, &CC}) { if (P->IsValid()) (*P)->Destroy(); }
		return false;
	}
	FMMOMailStore::DeleteBox(NameA); FMMOMailStore::DeleteBox(NameB);
	A->InitForTest(NameA); B->InitForTest(NameB); C->InitForTest(FString());
	{ FMMOCharacterSaveData S; S.Gold = 1000; S.NextInstanceId = 50; InvA->ApplySaveData(S); S.Gold = 7; InvB->ApplySaveData(S); }

	Check(A->SendMail(NameA, 10, TEXT("x"), true) == EMMOMailResult::Self, TEXT("live_rejects_self"));
	Check(A->SendMail(TEXT(""), 10, TEXT("x"), true) == EMMOMailResult::BadTarget && A->SendMail(TEXT("???"), 10, TEXT("x"), true) == EMMOMailResult::BadTarget, TEXT("live_rejects_bad_target"));
	Check(A->SendMail(NameB, -1, TEXT("x"), true) == EMMOMailResult::BadGold && A->SendMail(NameB, FMMOMailStore::MaxGoldPerMail + 1, TEXT("x"), true) == EMMOMailResult::BadGold, TEXT("live_rejects_bad_gold"));
	Check(A->SendMail(NameB, 0, FString::ChrN(201, TEXT('t')), true) == EMMOMailResult::TextTooLong, TEXT("live_rejects_long_text"));
	Check(A->SendMail(NameB, 0, TEXT("   "), true) == EMMOMailResult::EmptyMail, TEXT("live_rejects_empty"));
	Check(A->SendMail(NameB, 10, TEXT("x"), false) == EMMOMailResult::UnknownTarget, TEXT("live_rejects_unknown_recipient"));
	Check(A->SendMail(NameB, 2000, TEXT("x"), true) == EMMOMailResult::NoGold && InvA->Gold == 1000 && FMMOMailStore::Count(NameB) == 0, TEXT("live_no_gold_no_charge"));
	Check(A->SendMail(NameB, 995, TEXT("x"), true) == EMMOMailResult::NoGold && InvA->Gold == 1000, TEXT("live_fee_counts_toward_total"));

	Check(A->SendMail(NameB, 500, TEXT("merhaba"), true) == EMMOMailResult::Ok, TEXT("live_send_ok"));
	Check(InvA->Gold == 1000 - 500 - FeeFor(500) && FMMOMailStore::Count(NameB) == 1, TEXT("live_send_charges_gold_plus_fee"));
	Check(A->SendMail(NameB, 0, TEXT("hemen"), true) == EMMOMailResult::Cooldown && InvA->Gold == 1000 - 500 - FeeFor(500) && FMMOMailStore::Count(NameB) == 1, TEXT("live_cooldown_blocks_without_charge"));
	A->AdvanceTimeForTest(6.0);
	Check(A->SendMail(NameB, 0, TEXT("ikinci"), true) == EMMOMailResult::Ok && InvA->Gold == 1000 - 500 - FeeFor(500) - FeeFor(0) && FMMOMailStore::Count(NameB) == 2, TEXT("live_second_send_after_cooldown"));

	Check(C->CollectMail() == 0, TEXT("live_unnamed_collects_nothing"));
	Check(B->CollectMail() == 2 && InvB->Gold == 7 + 500 && FMMOMailStore::Count(NameB) == 0, TEXT("live_collect_delivers_gold_once"));
	Check(B->CollectMail() == 0 && InvB->Gold == 7 + 500, TEXT("live_second_collect_no_regrant"));

	{
		FMMOMail Fill; Fill.From = TEXT("Ali"); Fill.Text = TEXT("dolgu");
		for (int32 i = 0; i < FMMOMailStore::MaxMailsPerBox; ++i) FMMOMailStore::Append(NameB, Fill);
		A->AdvanceTimeForTest(6.0);
		const int32 GoldBefore = InvA->Gold;
		Check(A->SendMail(NameB, 100, TEXT("tasar"), true) == EMMOMailResult::BoxFull && InvA->Gold == GoldBefore, TEXT("live_box_full_no_charge"));
	}

	FMMOMailStore::DeleteBox(NameA); FMMOMailStore::DeleteBox(NameB);
	for (TStrongObjectPtr<AMMOCharacter>* P : {&CA, &CB, &CC}) { if (P->IsValid()) (*P)->Destroy(); }
	return bPassed; // Ayni senkron kare: PlayerState adi, kayit yukleme, /posta komutu, zamanlayici ve ag kaniti degil.
}

namespace
{
	class FMMOMailTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestMail"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOMailComponent::RunSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOMailTestExec GMMOMailTestExec;
}
#endif
