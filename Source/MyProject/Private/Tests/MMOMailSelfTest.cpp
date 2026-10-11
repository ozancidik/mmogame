// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOMailComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Tests/MMOPurchaseCallbackProbe.h"
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
	Check(FMMOMailStore::GetBoxPath(TEXT("A b")).EndsWith(TEXT("MMOMail/MMOMail_a_b.json")) || FMMOMailStore::GetBoxPath(TEXT("A b")).EndsWith(TEXT("MMOMail\\MMOMail_a_b.json")), TEXT("name_box_path"));
	Check(FMMOMailStore::GetBoxPath(TEXT("ALI")) == FMMOMailStore::GetBoxPath(TEXT("ali")) && FMMOMailStore::GetBoxPath(TEXT("Al i")) == FMMOMailStore::GetBoxPath(TEXT("al_i")), TEXT("name_box_path_case_insensitive"));

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
		Check(FMMOMailStore::Count(NameStore.ToUpper()) == 2 && FMMOMailStore::Count(NameStore.ToLower()) == 2, TEXT("store_case_variants_share_one_box"));
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
		// Bozuk dosya: .bad'a ayrilir, o yukleme BASARISIZ (false; tur 3 Y2: bos sayilip uzerine yazilmaz), sonra kutu bos
		// baslar ve ekleme yeniden calisir.
		const FString Path = FMMOMailStore::GetBoxPath(NameBad);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(TEXT("{{{ garbage"), *Path);
		TArray<FMMOMail> Loaded;
		Check(!FMMOMailStore::Load(NameBad, Loaded) && Loaded.IsEmpty() && IFileManager::Get().FileExists(*(Path + TEXT(".bad"))) && !IFileManager::Get().FileExists(*Path), TEXT("store_corrupt_box_quarantined"));
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
	A->AdvanceTimeForTest(6.0); // B3: basarisiz alici denemesi de cooldown baslatir
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

	// --- D1: "/posta gonder" ayristirma; altin YALNIZ "<rakamlar>g" bicimi ---
	{
		struct FMailParseCase { const TCHAR* Args; const TCHAR* Name; int32 Gold; const TCHAR* Text; const TCHAR* Label; };
		const int32 Over = FMMOMailStore::MaxGoldPerMail + 1;
		const FMailParseCase Cases[] = {
			{ TEXT("Ali 500g selam"),               TEXT("Ali"), 500,     TEXT("selam"),                TEXT("parse_gold_suffix") },
			{ TEXT("Ali 500G selam"),               TEXT("Ali"), 500,     TEXT("selam"),                TEXT("parse_gold_suffix_upper") },
			{ TEXT("Ali 0g selam"),                 TEXT("Ali"), 0,       TEXT("selam"),                TEXT("parse_zero_gold") },
			{ TEXT("Ali 100 kere tesekkurler"),     TEXT("Ali"), 0,       TEXT("100 kere tesekkurler"), TEXT("parse_plain_number_is_text") },
			{ TEXT("Ali +"),                        TEXT("Ali"), 0,       TEXT("+"),                    TEXT("parse_symbol_is_text") },
			{ TEXT("Ali 99999999999g x"),           TEXT("Ali"), Over,    TEXT("x"),                    TEXT("parse_huge_caps_to_limit_plus_one") },
			{ TEXT("Ali 1000001g x"),               TEXT("Ali"), Over,    TEXT("x"),                    TEXT("parse_limit_plus_one") },
			{ TEXT("Ali 1000000g"),                 TEXT("Ali"), 1000000, TEXT(""),                     TEXT("parse_limit_exact_no_text") },
			{ TEXT("Ali 500g"),                     TEXT("Ali"), 500,     TEXT(""),                     TEXT("parse_gold_without_text") },
			{ TEXT(""),                             TEXT(""),    0,       TEXT(""),                     TEXT("parse_empty_args") },
			{ TEXT("Ali"),                          TEXT("Ali"), 0,       TEXT(""),                     TEXT("parse_name_only") },
			{ TEXT("  Ali   500g   selam dunya  "), TEXT("Ali"), 500,     TEXT("selam dunya"),          TEXT("parse_extra_spaces") },
			{ TEXT("Ali g selam"),                  TEXT("Ali"), 0,       TEXT("g selam"),              TEXT("parse_bare_g_is_text") },
			{ TEXT("Ali -5g selam"),                TEXT("Ali"), 0,       TEXT("-5g selam"),            TEXT("parse_negative_is_text") },
			{ TEXT("Ali 5x0g selam"),               TEXT("Ali"), 0,       TEXT("5x0g selam"),           TEXT("parse_non_digit_is_text") },
		};
		for (const FMailParseCase& PC : Cases)
		{
			FString N, T; int32 G = -1;
			ParseSendArgs(PC.Args, N, G, T);
			Check(N.Equals(PC.Name, ESearchCase::CaseSensitive) && G == PC.Gold && T.Equals(PC.Text, ESearchCase::CaseSensitive), PC.Label);
		}
		FString N, T; int32 G = 0;
		ParseSendArgs(TEXT("Ali 99999999999g x"), N, G, T);
		const int32 GoldBefore = InvA->Gold;
		Check(A->SendMail(NameB, G, T, true) == EMMOMailResult::BadGold && InvA->Gold == GoldBefore, TEXT("parse_huge_rejected_by_send"));
	}

	// --- K1: ucret dusulur -> gonderen karakter kaydedilir -> SONRA kutuya yazilir; kayit olmazsa iade, kutu bos ---
	FMMOMailStore::DeleteBox(NameB);
	InvA->Gold = 10000; InvB->Gold = 0;
	{
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(true);
		const int32 Calls0 = A->GetSaveCallsForTest();
		Check(A->SendMail(NameB, 300, TEXT("k1"), true) == EMMOMailResult::Ok, TEXT("k1_send_ok"));
		Check(A->GetSaveCallsForTest() >= Calls0 + 1 && FMMOMailStore::Count(NameB) == 1 && InvA->Gold == 10000 - 300 - FeeFor(300), TEXT("k1_sender_saved_charged_and_boxed"));
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(false);
		const int32 Calls1 = A->GetSaveCallsForTest();
		const int32 GoldBefore = InvA->Gold;
		Check(A->SendMail(NameB, 300, TEXT("k1-hata"), true) == EMMOMailResult::StoreError, TEXT("k1_save_fail_store_error"));
		Check(InvA->Gold == GoldBefore, TEXT("k1_save_fail_refunds_gold"));
		Check(FMMOMailStore::Count(NameB) == 1, TEXT("k1_save_fail_box_not_appended"));
		Check(A->GetSaveCallsForTest() == Calls1 + 1, TEXT("k1_save_attempted_before_box"));
		A->SetSaveResultForTest(true);
	}

	// --- Bolge gecisi kilidi (kayit yuklenmedi / gecis kaydi onaylandi): gonderim ve toplama yok ---
	{
		A->AdvanceTimeForTest(6.0);
		A->SetTravelLockedForTest(true);
		const int32 GA = InvA->Gold;
		const int32 CallsA = A->GetSaveCallsForTest();
		Check(A->SendMail(NameB, 100, TEXT("kilit"), true) == EMMOMailResult::StoreError && InvA->Gold == GA && FMMOMailStore::Count(NameB) == 1
			&& A->GetSaveCallsForTest() == CallsA, TEXT("lock_send_rejected_no_charge"));
		A->SetTravelLockedForTest(false);
		Check(A->SendMail(NameB, 0, TEXT("kilit-sonrasi"), true) == EMMOMailResult::Ok && FMMOMailStore::Count(NameB) == 2, TEXT("lock_released_send_ok"));
		B->SetTravelLockedForTest(true);
		Check(B->CollectMail() == 0 && InvB->Gold == 0 && FMMOMailStore::Count(NameB) == 2, TEXT("lock_collect_skipped_box_kept"));
		B->SetTravelLockedForTest(false);
	}

	// --- B2: ad degismis (canli ad != kayit yuvasi) karakter posta gonderemez/toplayamaz ---
	{
		A->AdvanceTimeForTest(6.0);
		A->SetNameChangedForTest(true);
		const int32 GA = InvA->Gold;
		Check(A->SendMail(NameB, 100, TEXT("ad"), true) == EMMOMailResult::NameChanged && InvA->Gold == GA && FMMOMailStore::Count(NameB) == 2, TEXT("rename_send_rejected_no_charge"));
		A->SetNameChangedForTest(false);
		B->SetNameChangedForTest(true);
		Check(B->CollectMail() == 0 && InvB->Gold == 0 && FMMOMailStore::Count(NameB) == 2, TEXT("rename_collect_skipped_box_kept"));
		B->SetNameChangedForTest(false);
		const int32 CallsB = B->GetSaveCallsForTest();
		Check(B->CollectMail() == 2 && InvB->Gold == 300 && FMMOMailStore::Count(NameB) == 0, TEXT("unlocked_collect_delivers_all"));
		Check(B->GetSaveCallsForTest() == CallsB + 1, TEXT("collect_saves_after_gold"));
	}

	// --- B3: alici yok / altin yetersiz ayni metin; altin kontrolu varlik kontrolunden once; basarisiz arama bekleme baslatir ---
	Check(ResultText(EMMOMailResult::UnknownTarget).Equals(ResultText(EMMOMailResult::NoGold), ESearchCase::CaseSensitive), TEXT("b3_unknown_and_nogold_same_text"));
	{
		A->AdvanceTimeForTest(6.0);
		const int32 GA = InvA->Gold;
		InvA->Gold = 5;
		Check(A->SendMail(NameB, 100, TEXT("x"), false) == EMMOMailResult::NoGold, TEXT("b3_nogold_checked_before_existence"));
		InvA->Gold = GA;
		Check(A->SendMail(NameB, 100, TEXT("x"), false) == EMMOMailResult::UnknownTarget && InvA->Gold == GA, TEXT("b3_nogold_no_cooldown_unknown_no_charge"));
		Check(A->SendMail(NameB, 100, TEXT("x"), true) == EMMOMailResult::Cooldown && InvA->Gold == GA && FMMOMailStore::Count(NameB) == 0, TEXT("b3_failed_lookup_starts_cooldown"));
	}

	// --- C1: temizleme adi sessizce degistirmesin; yalniz bosluk '_' ile esdeger ---
	{
		const FString NameV = TEXT("ZTestMail_V_") + Tag;
		FMMOMailStore::DeleteBox(NameV);
		A->AdvanceTimeForTest(6.0);
		Check(A->SendMail(TEXT("ZTestMail.V_") + Tag, 0, TEXT("x"), true) == EMMOMailResult::BadTarget, TEXT("c1_dot_in_name_rejected"));
		Check(A->SendMail(TEXT("ZTestMail!V_") + Tag, 0, TEXT("x"), true) == EMMOMailResult::BadTarget, TEXT("c1_symbol_in_name_rejected"));
		Check(A->SendMail(TEXT("ZTestMail/V_") + Tag, 0, TEXT("x"), true) == EMMOMailResult::BadTarget, TEXT("c1_path_char_rejected"));
		Check(A->SendMail(FString::ChrN(FMMOMailStore::MaxNameLength + 1, TEXT('q')), 0, TEXT("x"), true) == EMMOMailResult::BadTarget
			&& FMMOMailStore::Count(NameV) == 0, TEXT("c1_overlong_name_rejected"));
		Check(A->SendMail(NameV, 0, TEXT("alt cizgi"), true) == EMMOMailResult::Ok && FMMOMailStore::Count(NameV) == 1, TEXT("c1_underscore_name_accepted"));
		A->AdvanceTimeForTest(6.0);
		Check(A->SendMail(TEXT("ZTestMail V_") + Tag, 0, TEXT("bosluk"), true) == EMMOMailResult::Ok && FMMOMailStore::Count(NameV) == 2, TEXT("c1_space_equals_underscore"));
		FMMOMailStore::DeleteBox(NameV);
	}

	// --- O2: altin tavanini (MaxGold) asacak posta kutuda KALIR; asmayanlar teslim edilir ---
	{
		FMMOMailStore::DeleteBox(NameB);
		const int32 Max = UMMOInventoryComponent::MaxGold;
		InvB->Gold = Max - 100;
		FMMOMail M1; M1.From = TEXT("Ali"); M1.Text = TEXT("sigar"); M1.Gold = 60;
		FMMOMail M2 = M1; M2.Text = TEXT("tasar"); M2.Gold = 500;
		FMMOMail M3 = M1; M3.Text = TEXT("yalniz mesaj"); M3.Gold = 0;
		const bool bSeeded = FMMOMailStore::Append(NameB, M1) && FMMOMailStore::Append(NameB, M2) && FMMOMailStore::Append(NameB, M3);
		const int32 CallsB = B->GetSaveCallsForTest();
		Check(bSeeded && B->CollectMail() == 2 && InvB->Gold == Max - 40, TEXT("o2_fitting_mail_delivered"));
		TArray<FMMOMail> Left;
		Check(FMMOMailStore::Load(NameB, Left) && Left.Num() == 1 && Left[0].Text == TEXT("tasar") && Left[0].Gold == 500, TEXT("o2_over_cap_mail_stays_in_box"));
		Check(B->GetSaveCallsForTest() == CallsB + 1, TEXT("o2_delivery_saved"));
		Check(B->CollectMail() == 0 && InvB->Gold == Max - 40 && FMMOMailStore::Count(NameB) == 1, TEXT("o2_retry_still_kept"));
		InvB->AddGoldFrom(-1000, TEXT("test"));
		Check(B->CollectMail() == 1 && InvB->Gold == Max - 1040 + 500 && FMMOMailStore::Count(NameB) == 0, TEXT("o2_delivered_after_spending"));
	}

	// --- D3: yarim kalan atomik yazim (.tmp) kurtarma ---
	{
		const FString NameT = TEXT("ZTestMailT_") + Tag;
		FMMOMailStore::DeleteBox(NameT);
		const FString Path = FMMOMailStore::GetBoxPath(NameT);
		const FString Tmp = Path + TEXT(".tmp");
		TArray<FMMOMail> Seed;
		FMMOMail M; M.From = TEXT("Ali"); M.Text = TEXT("yarim"); M.Gold = 70;
		Seed.Add(M);
		FString Json;
		FMMOMailStore::ToJson(Seed, Json);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Json, *Tmp);
		TArray<FMMOMail> Loaded;
		Check(FMMOMailStore::Load(NameT, Loaded) && Loaded.Num() == 1 && Loaded[0].Gold == 70, TEXT("d3_tmp_only_box_recovered"));
		Check(IFileManager::Get().FileExists(*Path) && !IFileManager::Get().FileExists(*Tmp), TEXT("d3_tmp_promoted_to_main"));
		Seed.Add(M);
		FMMOMailStore::ToJson(Seed, Json);
		FFileHelper::SaveStringToFile(Json, *Tmp); // ana dosya varken eski .tmp
		Check(FMMOMailStore::Count(NameT) == 1, TEXT("d3_main_wins_over_stale_tmp"));
		Check(FMMOMailStore::Append(NameT, M) && FMMOMailStore::Count(NameT) == 2 && !IFileManager::Get().FileExists(*Tmp), TEXT("d3_next_write_replaces_stale_tmp"));
		FMMOMailStore::DeleteBox(NameT);
	}

	// --- Guvenlik tur 2: basarisiz yazimda .tmp temizligi (iade edilen/bosaltilan posta dirilmez), bozuk kutunun eski .tmp'si ---
	IFileManager& FM = IFileManager::Get();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	{
		// .tmp yazilamadi (salt-okunur eski .tmp, ana dosya yok): WriteBox false doner ve .tmp'yi siler; sonraki Load eski icerigi
		// "yarim yazim" sanip geri koymaz.
		const FString NameR = TEXT("ZTestMailR_") + Tag;
		FMMOMailStore::DeleteBox(NameR);
		const FString Path = FMMOMailStore::GetBoxPath(NameR);
		const FString Tmp = Path + TEXT(".tmp");
		TArray<FMMOMail> Stale;
		FMMOMail M; M.From = TEXT("Ali"); M.Text = TEXT("eski"); M.Gold = 70;
		Stale.Add(M);
		FString Json;
		FMMOMailStore::ToJson(Stale, Json);
		FM.MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Json, *Tmp);
		PF.SetReadOnly(*Tmp, true);
		Check(FM.FileExists(*Tmp) && PF.IsReadOnly(*Tmp) && !FM.FileExists(*Path), TEXT("tmp_write_fail_fixture"));
		Check(!FMMOMailStore::Replace(NameR, TArray<FMMOMail>()), TEXT("tmp_write_fail_returns_false"));
		Check(!FM.FileExists(*Tmp), TEXT("tmp_write_fail_cleans_tmp"));
		TArray<FMMOMail> Loaded;
		Check(FMMOMailStore::Load(NameR, Loaded) && Loaded.IsEmpty(), TEXT("tmp_write_fail_no_resurrect"));
		FMMOMailStore::DeleteBox(NameR);
	}
	{
		// Ilk posta (ana dosya yok), .tmp tam yazildi ama ana yola tasinamiyor ve dogrudan yazilamiyor (yol bir dizin): gonderim
		// basarisiz sayilir, altin iade edilir, .tmp silinir; dizin kalkinca kutu bos kalir (iade edilen posta teslim de edilmez).
		// Not: motor Move'u 10x0.5 sn dener ve bir "Error moving file" satiri basar (calistirici yalniz bu satiri bekler).
		const FString NameD = TEXT("ZTestMailD_") + Tag;
		FMMOMailStore::DeleteBox(NameD);
		const FString Path = FMMOMailStore::GetBoxPath(NameD);
		const FString Tmp = Path + TEXT(".tmp");
		FM.MakeDirectory(*Path, true);
		Check(FM.DirectoryExists(*Path) && !FM.FileExists(*Path) && FMMOMailStore::Count(NameD) == 0, TEXT("tmp_first_mail_fixture"));
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(true);
		InvA->Gold = 10000;
		const EMMOMailResult R = A->SendMail(NameD, 300, TEXT("dizin"), true);
		Check(R == EMMOMailResult::StoreError && InvA->Gold == 10000, TEXT("tmp_first_mail_write_fail_refunds"));
		Check(!FM.FileExists(*Tmp), TEXT("tmp_first_mail_write_fail_cleans_tmp"));
		FM.DeleteDirectory(*Path, false, true);
		TArray<FMMOMail> Loaded;
		Check(!FM.DirectoryExists(*Path) && FMMOMailStore::Load(NameD, Loaded) && Loaded.IsEmpty(), TEXT("tmp_refunded_first_mail_not_resurrected"));
		FMMOMailStore::DeleteBox(NameD);
	}
	{
		// Bozuk ana dosya + eski .tmp: ana dosya .bad, .tmp .tmp.bad olur (o yukleme false, tur 3 Y2); sonraki Load .tmp'yi geri
		// koymaz; DeleteBox ikisini de siler.
		const FString NameQ = TEXT("ZTestMailQ_") + Tag;
		FMMOMailStore::DeleteBox(NameQ);
		const FString Path = FMMOMailStore::GetBoxPath(NameQ);
		const FString Tmp = Path + TEXT(".tmp");
		TArray<FMMOMail> Old;
		FMMOMail M; M.From = TEXT("Ali"); M.Text = TEXT("toplanmis"); M.Gold = 90;
		Old.Add(M);
		FString Json;
		FMMOMailStore::ToJson(Old, Json);
		FM.MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(TEXT("{{{ bozuk"), *Path);
		FFileHelper::SaveStringToFile(Json, *Tmp);
		TArray<FMMOMail> Loaded;
		Check(!FMMOMailStore::Load(NameQ, Loaded) && Loaded.IsEmpty() && FM.FileExists(*(Path + TEXT(".bad"))) && FM.FileExists(*(Tmp + TEXT(".bad")))
			&& !FM.FileExists(*Tmp) && !FM.FileExists(*Path), TEXT("corrupt_box_stale_tmp_quarantined"));
		Check(FMMOMailStore::Load(NameQ, Loaded) && Loaded.IsEmpty(), TEXT("corrupt_box_stale_tmp_not_resurrected"));
		FMMOMailStore::DeleteBox(NameQ);
		Check(!FM.FileExists(*(Path + TEXT(".bad"))) && !FM.FileExists(*(Tmp + TEXT(".bad"))), TEXT("deletebox_removes_quarantine_files"));
	}

	// --- Guvenlik tur 2: iade, gonderim sirasinda altin callback'i araya girdiginde gercek dusulen miktarla yapilir ---
	{
		FMMOMailStore::DeleteBox(NameB);
		TStrongObjectPtr<UMMOPurchaseCallbackProbe> Probe(NewObject<UMMOPurchaseCallbackProbe>());
		InvA->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
		// Callback altin HARCAR, sonra kayit basarisiz: iade yalniz postanin dustugu kadar; callback'in harcamasi geri gelmez.
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(false);
		InvA->Gold = 10000;
		Probe->Calls = 0;
		Probe->Callback = [InvA](int32) { InvA->AddGoldFrom(-1000, TEXT("test-harcama")); };
		const EMMOMailResult RSpend = A->SendMail(NameB, 300, TEXT("iade-harcama"), true);
		Check(RSpend == EMMOMailResult::StoreError && FMMOMailStore::Count(NameB) == 0 && Probe->Calls >= 1, TEXT("refund_spend_callback_store_error_box_unchanged"));
		Check(InvA->Gold == 10000 - 1000, TEXT("refund_spend_callback_exact_refund"));
		// Callback altin EKLER (bakiye net dusmez): posta kaydedilmeden reddedilir (alici odenmemis eki almaz), altin uretilmez.
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(true);
		InvA->Gold = 10000;
		Probe->Calls = 0;
		Probe->Callback = [InvA](int32) { InvA->AddGoldFrom(1000, TEXT("test-hediye")); };
		const int32 SaveCalls0 = A->GetSaveCallsForTest();
		const EMMOMailResult RGift = A->SendMail(NameB, 300, TEXT("iade-hediye"), true);
		Check(RGift == EMMOMailResult::StoreError && FMMOMailStore::Count(NameB) == 0 && A->GetSaveCallsForTest() == SaveCalls0,
			TEXT("refund_gift_callback_mail_refused_before_save"));
		Check(InvA->Gold <= 10000 + 1000, TEXT("refund_gift_callback_no_gold_created"));
		UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_NOTE] gift_callback final_gold=%d start=10000 gift=1000 charge=%d (oyuncu ucreti+eki kaybeder, altin uretilmez)"),
			InvA->Gold, 300 + FeeFor(300));
		// Gozlem (kontrol degil): callback bakiyeyi ucretten azina indirirse ucret hic dusulmez (tur 3: kirpilmaz), posta
		// reddedilir ve iade yalniz dusulen ek kadardir: final_gold no_creation_max'i asmamali.
		A->AdvanceTimeForTest(6.0);
		A->SetSaveResultForTest(false);
		InvA->Gold = 1000;
		Probe->Calls = 0;
		Probe->Callback = [InvA](int32) { InvA->AddGoldFrom(-495, TEXT("test-harcama")); };
		const EMMOMailResult RClamp = A->SendMail(NameB, 500, TEXT("kirpma"), true);
		UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_NOTE] fee_clamp_refund result=%d final_gold=%d no_creation_max=%d"),
			static_cast<int32>(RClamp), InvA->Gold, 1000 - 495);
		InvA->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
		A->SetSaveResultForTest(true);
		FMMOMailStore::DeleteBox(NameB);
	}

	FMMOMailStore::DeleteBox(NameA); FMMOMailStore::DeleteBox(NameB);
	for (TStrongObjectPtr<AMMOCharacter>* P : {&CA, &CB, &CC}) { if (P->IsValid()) (*P)->Destroy(); }
	return bPassed; // Ayni senkron kare: PlayerState adi, kayit yukleme, /posta komutu, zamanlayici ve ag kaniti degil.
}
// Bu testin KANITLAMADIKLARI: kilit/ad-degisimi gercek karar mantigi (IsOwnerLocked/IsNameChanged test modunda bayraktan okunur;
// gercek kosul IsInitialLoadComplete/IsTravelSaveCommitted/GetSaveSlotName != ActiveSaveSlot burada kosulmaz), gercek
// SaveCharacterForTravel diske yazimi, kutuya yazma (Append) hatasinda iade + tekrar kayit dali, A4 Move hatasi yedegi
// (dosya kilidi motorda Error satiri ve bekleme uretir), A3 toplama dongusunde sahibin dusmesi, 10 sn zamanlayici, ag/PIE.
// Guvenlik tur 2 bolumu: .tmp temizligi salt-okunur eski .tmp ve dizin olan kutu yoluyla benzetilir (gercek disk dolu/antivirus
// kilidi degil); .tmp silme de basarisiz olursa "yazim basarili sayilir" dali (son care) kosulmaz. Iade callback'i
// UMMOPurchaseCallbackProbe ile benzetilir; Blueprint OnGoldChanged degil.

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
