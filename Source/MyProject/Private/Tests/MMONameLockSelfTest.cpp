// Copyright Epic Games, Inc. All Rights Reserved.

// K-1 "guvenlik tur 2" ad kilidi testleri (SECURITY_AUDIT K-1 F1/F5/F3):
//  F1: uzak (yonetici disi) oyuncu, ActiveSaveSlot'u DISKTE kayitli karakterin adini degistiremez; kayitsiz yeni karakter ve
//      yuklemesi bitmemis (bos ActiveSaveSlot) karakter degistirebilir; yerel (host) denetimli pawn muaf (test bayragi kapali).
//  F5: "bu ad kayitli bir karaktere ait" reddi ad sogumasini baslatir (ikinci deneme hemen sogumaya takilir).
//  F3: yuklu karakterde kaydi olmayan canli adla LoadCharacter() hicbir seyi degistirmez (yuva, seri, yukleme bayragi,
//      envanter, basarim durumu).
// Kimlik yalniz GUID'li gecici yuvalarla kurulur; sonunda .sav ve basarim yan dosyalari silinir.

#include "Characters/MMOCharacter.h"

#if !UE_BUILD_SHIPPING
#include "AIController.h"
#include "Components/MMOAchievementComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Core/MMOGameMode.h"
#include "Core/MMONameRules.h"
#include "Core/MMONotify.h"
#include "Engine/GameInstance.h"
#include "Mail/MMOMailStore.h"
#include "Player/MMOPlayerController.h"
#include "UI/MMOHUD.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 NameLockPawnCount = 5;

	// Tur 4 (Y8/DUSUK-5) testi: gercek magazaya devreder, Exists cagrilarini sayar (ad kilidi reddinden sonra soguma
	// suresindeki istek diske bakmamali). UMMOCharacterStoreSubsystem::SwapStoreForTest ile ayni karede takilip sokulur.
	class FMMONameLockCountingStore final : public IMMOCharacterStore
	{
	public:
		explicit FMMONameLockCountingStore(IMMOCharacterStore& InInner) : Inner(InInner) {}
		virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) override { return Inner.Load(SlotName, Out); }
		virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) override { return Inner.Save(SlotName, In); }
		virtual bool Exists(const FString& SlotName) override { ++ExistsCalls; return Inner.Exists(SlotName); }
		virtual void ListSlotNames(TArray<FString>& Out) override { Inner.ListSlotNames(Out); }
		int32 ExistsCalls = 0;
	private:
		IMMOCharacterStore& Inner;
	};

	// Kaynak ASCII kalsin: Unicode harfler UTF-16 koduyla eklenir (orn. 0x00D6 buyuk O-umlaut, 0x0131 noktasiz i, 0x015F s-cedilla).
	FString MMONameLockMix(const FString& Prefix, const TArray<TCHAR>& Codes, const TCHAR* Suffix)
	{
		FString Out = Prefix;
		for (const TCHAR Code : Codes)
		{
			Out.AppendChar(Code);
		}
		Out += Suffix;
		return Out;
	}

	bool RunMMONameLockSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0; OutError.Reset();
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{ OutError = TEXT("disposable authority headless game world required"); return false; }
		IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(World);
		if (!Store) { OutError = TEXT("store unavailable"); return false; }

		bool bPassed = true;
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_NAMELOCK_CASE] %s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Label; }
		};

		// --- Tur 4 (Y-NEW-1): MMONameRules saf mantik (yalniz ASCII ad kurali, yuva anahtari, kanonik anahtar) ---
		const TCHAR UpperOUml = 0x00D6, LowerOUml = 0x00F6, DotlessI = 0x0131, SCedilla = 0x015F;
		{
			using namespace MMONameRules;
			const ESearchCase::Type CS = ESearchCase::CaseSensitive;
			Check(IsAsciiAlnum(TEXT('a')) && IsAsciiAlnum(TEXT('Z')) && IsAsciiAlnum(TEXT('0')) && IsAsciiAlnum(TEXT('9'))
				&& !IsAsciiAlnum(TEXT('_')) && !IsAsciiAlnum(TEXT(' ')) && !IsAsciiAlnum(UpperOUml) && !IsAsciiAlnum(LowerOUml)
				&& !IsAsciiAlnum(DotlessI) && !IsAsciiAlnum(SCedilla), TEXT("y1_rules_ascii_alnum_only"));
			Check(IsAllowedNameChar(TEXT(' ')) && IsAllowedNameChar(TEXT('_')) && IsAllowedNameChar(TEXT('-')) && !IsAllowedNameChar(TEXT('.'))
				&& !IsAllowedNameChar(TEXT('/')) && !IsAllowedNameChar(TEXT('\n')) && !IsAllowedNameChar(UpperOUml) && !IsAllowedNameChar(DotlessI)
				&& !IsAllowedNameChar(SCedilla), TEXT("y1_rules_allowed_chars"));
			Check(HasOnlyAllowedChars(TEXT("Ali Veli_1-2")) && !HasOnlyAllowedChars(TEXT("Ali.")) && !HasOnlyAllowedChars(MMONameLockMix(FString(), { UpperOUml }, TEXT("mer")))
				&& !HasOnlyAllowedChars(MMONameLockMix(TEXT("Ku"), { SCedilla }, TEXT(""))) && !HasOnlyAllowedChars(MMONameLockMix(TEXT("K"), { DotlessI }, TEXT("r"))),
				TEXT("y1_rules_has_only_allowed"));
			Check(SlotKey(TEXT("A B")).Equals(TEXT("A_B"), CS) && SlotKey(TEXT("AbC")).Equals(TEXT("AbC"), CS) && SlotKey(TEXT("a/b.c")).Equals(TEXT("abc"), CS)
				&& SlotKey(MMONameLockMix(FString(), { UpperOUml }, TEXT("mer ")) + MMONameLockMix(FString(), { DotlessI, SCedilla }, TEXT("en"))).Equals(TEXT("mer_en"), CS)
				&& SlotKey(FString::ChrN(40, TEXT('x'))).Len() == 32 && SlotKey(TEXT("abcdef"), 3).Equals(TEXT("abc"), CS), TEXT("y1_rules_slot_key"));
			Check(CanonicalKey(TEXT("A B")).Equals(TEXT("a_b"), CS) && CanonicalKey(TEXT("A_B")).Equals(TEXT("a_b"), CS) && CanonicalKey(TEXT("a_b")).Equals(TEXT("a_b"), CS)
				&& CanonicalKey(TEXT("Omer")).Equals(CanonicalKey(TEXT("omer")), CS)
				&& CanonicalKey(MMONameLockMix(FString(), { UpperOUml }, TEXT("mer"))).Equals(CanonicalKey(MMONameLockMix(FString(), { LowerOUml }, TEXT("mer"))), CS)
				&& CanonicalKey(FString()).Equals(TEXT("default"), CS) && CanonicalKey(MMONameLockMix(FString(), { UpperOUml, DotlessI, SCedilla }, TEXT(""))).Equals(TEXT("default"), CS),
				TEXT("y1_rules_canonical_key"));
			Check(SameCanonical(TEXT("MMOSave_Ali"), TEXT("mmosave_ALI")) && !SameCanonical(TEXT("MMOSave_Ali"), TEXT("MMOSave_Alix")), TEXT("y1_rules_same_canonical"));
			bool bHintAscii = true;
			for (const TCHAR* P = AllowedCharsHint(); *P; ++P) { bHintAscii &= (*P > 0 && *P < 128); }
			Check(bHintAscii && FCString::Strlen(AllowedCharsHint()) > 10, TEXT("y1_rules_hint_ascii"));
			// Tur 5: ayrilmis adlar (Default/default, Player+rakam; bos/temizlenince bos ad kanonik "default") - Gezgin adlari, rakamsiz
			// "Player" ve "PlayerOne" ayrilmis degil.
			Check(IsReservedName(TEXT("Default")) && IsReservedName(TEXT("default")) && IsReservedName(TEXT("DEFAULT")) && IsReservedName(TEXT("Player123"))
				&& IsReservedName(TEXT("player7")) && IsReservedName(TEXT("Player256")) && IsReservedName(FString()) && IsReservedName(TEXT("???"))
				&& !IsReservedName(TEXT("Player")) && !IsReservedName(TEXT("PlayerOne")) && !IsReservedName(TEXT("Player12a")) && !IsReservedName(TEXT("Player 12"))
				&& !IsReservedName(TEXT("Gezgin")) && !IsReservedName(TEXT("Gezgin2")) && !IsReservedName(TEXT("Gezgin123456")) && !IsReservedName(TEXT("Defaulter"))
				&& !IsReservedName(TEXT("Ali")), TEXT("y5_rules_reserved_names"));
			// Posta ayni kurali kullanir: Default/default ayni davranir, Player+rakam teslim edilmez.
			Check(!FMMOMailStore::IsDeliverableName(TEXT("Default")) && !FMMOMailStore::IsDeliverableName(TEXT("default"))
				&& !FMMOMailStore::IsDeliverableName(TEXT("Player123")) && FMMOMailStore::IsDeliverableName(TEXT("Gezgin2"))
				&& FMMOMailStore::IsDeliverableName(TEXT("Player")), TEXT("y5_mail_reserved_not_deliverable"));
			// Gozlem (kontrol degil): eski kural FChar::IsAlnum bu harfleri kabul ediyor mu (mutasyon/ClanLegacy yorumu icin).
			UE_LOG(LogTemp, Display, TEXT("[MMO_NAMELOCK_NOTE] fchar_isalnum O_umlaut=%d dotless_i=%d s_cedilla=%d"),
				FChar::IsAlnum(UpperOUml) ? 1 : 0, FChar::IsAlnum(DotlessI) ? 1 : 0, FChar::IsAlnum(SCedilla) ? 1 : 0);
		}

		// Ad en fazla 20 karakter (ServerSetName): "ZNL" + 8 hex + 1-2 harf.
		const FString Tag = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		auto NameOf = [&Tag](const TCHAR* Suffix) { return FString(TEXT("ZNL")) + Tag + Suffix; };
		auto SlotOf = [](const FString& Name) { return FString(TEXT("MMOSave_")) + Name; };
		const FString NReg = NameOf(TEXT("R"));    // P0: kayitli karakter
		const FString NRegTo = NameOf(TEXT("RT")); // P0'in denedigi yeni ad
		const FString NNew = NameOf(TEXT("N"));    // P1: kayitsiz yeni karakter
		const FString NNewTo = NameOf(TEXT("NT"));
		const FString NEmpty = NameOf(TEXT("E"));  // P2: yuklemesi bitmemis
		const FString NEmptyTo = NameOf(TEXT("ET"));
		const FString NCool = NameOf(TEXT("C"));   // P3: F5 soguma
		const FString NCoolTo = NameOf(TEXT("CT"));
		const FString NTaken = NameOf(TEXT("K"));  // cevrimdisi kayitli ad (hicbir pawn'da degil)
		const FString NLoad = NameOf(TEXT("L"));   // P4: F3 yeniden yukleme
		const FString NLoadX = NameOf(TEXT("LX")); // P4'un kaydi olmayan canli adi
		const FString NHost = NameOf(TEXT("H"));   // yerel (host) denetimli kayitli karakter
		const FString NHostTo = NameOf(TEXT("HT"));
		// Tur 4 adlari: kanonik cakisma sahipleri (S T / Om / Unicode), istekci, yeni baglanan, Y8 kilitli karakter, Y3/G karakteri.
		const FString NHoldSpace = NameOf(TEXT("S T"));
		const FString NHoldCase = NameOf(TEXT("Om"));
		const FString NHoldUni = MMONameLockMix(NameOf(TEXT("")), { UpperOUml, DotlessI, SCedilla }, TEXT("x")); // yuva anahtari ...x
		const FString NReq = NameOf(TEXT("Q"));
		const FString NReqOk = NameOf(TEXT("Rok"));
		const FString NNewcomer = NameOf(TEXT("Nc"));
		const FString NFree = NameOf(TEXT("Free"));
		const FString NPreFree = NameOf(TEXT("Pfree"));
		const FString NLockReg = NameOf(TEXT("Y"));  // Y8: kayitli, uzak kurallar
		const FString NLockTo1 = NameOf(TEXT("YA"));
		const FString NLockTo2 = NameOf(TEXT("YB"));
		const FString NGate = NameOf(TEXT("G"));     // Y3/G: kaydi olmayan canli ad
		const FString NGateX = NameOf(TEXT("GX"));
		const TArray<FString> AllNames = { NReg, NRegTo, NNew, NNewTo, NEmpty, NEmptyTo, NCool, NCoolTo, NTaken, NLoad, NLoadX, NHost, NHostTo,
			NHoldCase, NameOf(TEXT("S_T")), NameOf(TEXT("x")), NReq, NReqOk, NNewcomer, NFree, NPreFree, NameOf(TEXT("Z")), NLockReg, NLockTo1, NLockTo2, NGate, NGateX };
		for (const FString& N : AllNames)
		{
			if (Store->Exists(SlotOf(N))) { OutError = TEXT("temporary slot collision"); return false; }
		}
		const FString AchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"));

		TArray<AMMOCharacter*> Pawns;
		TArray<AMMOPlayerController*> Controllers;
		TArray<AMMOCharacter*> ExtraPawns;               // tur 4 bolumu (temizlikte ayni kurallarla kaldirilir)
		TArray<AMMOPlayerController*> ExtraControllers;
		AMMOCharacter* HostPawn = nullptr;
		AAIController* HostAI = nullptr;
		APlayerState* HostPS = nullptr;
		const TCHAR* InitialNames[NameLockPawnCount] = { *NReg, *NNew, *NEmpty, *NCool, *NLoad };
		bool bFixture = true;
		for (int32 I = 0; I < NameLockPawnCount; ++I)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
				FVector(2100000. + I * 3000., 2100000., 5000.), FRotator::ZeroRotator, Params);
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>();
			Pawns.Add(Pawn); Controllers.Add(PC);
			if (!Pawn || !PC) { bFixture = false; break; }
			Pawn->SetActorTickEnabled(false); PC->SetActorTickEnabled(false);
			PC->Possess(Pawn);
			if (!Pawn->GetPlayerState()) { bFixture = false; break; }
			Pawn->GetPlayerState()->SetPlayerName(InitialNames[I]);
		}
		Check(bFixture && Pawns.Num() == NameLockPawnCount, TEXT("fixture_possessed_pawns"));

		auto LiveName = [](const AMMOCharacter* P) { return P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString(); };
		auto InvSnapshot = [](const AMMOCharacter* P)
		{
			FMMOCharacterSaveData D;
			if (P->GetInventory()) P->GetInventory()->FillSaveData(D);
			return D;
		};
		auto SameInv = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{
			TArray<FString> Diff; FMMOCharacterSaveJson::FindDifferences(A, B, Diff); return Diff.IsEmpty();
		};

		if (bFixture)
		{
			// --- F1: kayitli karakter (ActiveSaveSlot diskte var), uzak + yonetici disi: ad degisimi REDDEDILIR ---
			AMMOCharacter* P0 = Pawns[0];
			P0->LoadCharacter(); // kayit yok: yeni karakter dali, ActiveSaveSlot atanir
			const bool bSaved0 = P0->SaveCharacterForTravel();
			Check(bSaved0 && Store->Exists(SlotOf(NReg)) && P0->GetActiveSaveSlot() == SlotOf(NReg), TEXT("f1_fixture_registered_slot_on_disk"));
			P0->SetRemoteNameRulesForTest(true);
			const FMMOCharacterSaveData Inv0 = InvSnapshot(P0);
			const UMMOAchievementComponent* A0 = P0->GetAchievementComponent();
			const TArray<FName> Unlocked0 = A0 ? A0->Unlocked : TArray<FName>();
			const uint32 Serial0 = P0->GetLoadSerial();
			P0->MMOSetName(NRegTo);
			Check(LiveName(P0) == NReg && P0->GetSaveSlotName() == SlotOf(NReg), TEXT("f1_registered_remote_rename_rejected"));
			Check(P0->GetActiveSaveSlot() == SlotOf(NReg) && P0->GetLoadSerial() == Serial0 && P0->IsInitialLoadComplete()
				&& !Store->Exists(SlotOf(NRegTo)), TEXT("f1_registered_reject_keeps_save_identity"));
			Check(SameInv(Inv0, InvSnapshot(P0)) && A0 && A0->Unlocked == Unlocked0, TEXT("f1_registered_reject_keeps_gold_items_unlocked"));

			// Host/yerel denetleyici muafiyeti. UE 5.7'de NetDriver yokken ULocalPlayer'siz (test icin spawn edilen) bir
			// APlayerController IsLocalController()=false doner, yani yukaridaki PC'li pawn'lar bayraksiz da "uzak" sayilir.
			// Yerel denetimi AController tabanli (AI) denetleyici verir (standalone'da IsLocalController=true): ayni kayitli yuva,
			// once bayrak ACIK (uzak benzetimi) -> RED, sonra bayrak KAPALI (yerel/host kurallari) -> izin. Tur 3 (Y8): F1 reddi
			// de soguma baslatir; muafiyet adimi bu yuzden sogumayi test kancasiyla sifirlayip dener.
			FMMOCharacterSaveData HostData;
			const bool bHostSlot = Store->Load(SlotOf(NReg), HostData) && Store->Save(SlotOf(NHost), HostData);
			FActorSpawnParameters HostParams;
			HostParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			HostPawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2100000., 2110000., 5000.), FRotator::ZeroRotator, HostParams);
			HostAI = World->SpawnActor<AAIController>(AAIController::StaticClass(), FVector(2100000., 2110000., 5000.), FRotator::ZeroRotator, HostParams);
			HostPS = World->SpawnActor<APlayerState>(APlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, HostParams);
			const bool bHostFixture = bHostSlot && HostPawn && HostAI && HostPS;
			if (bHostFixture)
			{
				HostPawn->SetActorTickEnabled(false);
				HostAI->Possess(HostPawn);
				HostPS->SetPlayerName(NHost);
				HostPawn->SetPlayerState(HostPS);
				HostPawn->SetSaveIdentityForTest(true, SlotOf(NHost), 1);
			}
			Check(bHostFixture && HostPawn->IsLocallyControlled() && Store->Exists(SlotOf(NHost)) && LiveName(HostPawn) == NHost,
				TEXT("f1_fixture_local_host_registered"));
			if (bHostFixture)
			{
				HostPawn->SetRemoteNameRulesForTest(true);
				HostPawn->MMOSetName(NHostTo);
				Check(LiveName(HostPawn) == NHost, TEXT("f1_same_slot_remote_flag_rejected"));
				HostPawn->SetRemoteNameRulesForTest(false);
				HostPawn->ResetSetNameCooldownForTest();
				HostPawn->MMOSetName(NHostTo);
				Check(LiveName(HostPawn) == NHostTo && HostPawn->GetActiveSaveSlot() == SlotOf(NHost), TEXT("f1_host_local_exempt_registered_rename_allowed"));
			}

			// --- F1: kayitsiz yeni karakter (ActiveSaveSlot atanmis ama diskte YOK): ad degisebilir ---
			AMMOCharacter* P1 = Pawns[1];
			P1->LoadCharacter();
			Check(P1->IsInitialLoadComplete() && P1->GetActiveSaveSlot() == SlotOf(NNew) && !Store->Exists(SlotOf(NNew)), TEXT("f1_fixture_unsaved_new_character"));
			P1->SetRemoteNameRulesForTest(true);
			P1->MMOSetName(NNewTo);
			Check(LiveName(P1) == NNewTo && P1->GetSaveSlotName() == SlotOf(NNewTo), TEXT("f1_unsaved_new_character_rename_allowed"));
			Check(P1->GetActiveSaveSlot() == SlotOf(NNew), TEXT("f1_unsaved_rename_does_not_move_active_slot"));

			// --- F1: yukleme bitmemis (ActiveSaveSlot bos): ad degisebilir ---
			AMMOCharacter* P2 = Pawns[2];
			P2->SetSaveIdentityForTest(false, FString(), 0);
			P2->SetRemoteNameRulesForTest(true);
			P2->MMOSetName(NEmptyTo);
			Check(LiveName(P2) == NEmptyTo && P2->GetActiveSaveSlot().IsEmpty(), TEXT("f1_empty_active_slot_rename_allowed"));

			// --- F5: cevrimdisi kayitli ada gecis reddi sogumayi baslatir; hemen ardindan BOS bir ad da reddedilir ---
			FMMOCharacterSaveData TakenData;
			const bool bTaken = Store->Load(SlotOf(NReg), TakenData) && Store->Save(SlotOf(NTaken), TakenData);
			Check(bTaken && Store->Exists(SlotOf(NTaken)), TEXT("f5_fixture_offline_registered_name"));
			AMMOCharacter* P3 = Pawns[3];
			P3->LoadCharacter(); // kayitsiz yeni karakter: F1 kilidi uygulanmaz, ad kurallari soguma/kayitli-ad dalina iner
			P3->SetRemoteNameRulesForTest(true);
			P3->MMOSetName(NTaken);
			Check(LiveName(P3) == NCool && P3->GetSaveSlotName() == SlotOf(NCool), TEXT("f5_registered_name_rejected"));
			P3->MMOSetName(NCoolTo);
			Check(LiveName(P3) == NCool && !Store->Exists(SlotOf(NCoolTo)), TEXT("f5_rejection_starts_cooldown"));

			// --- F3: yuklu karakter, canli adin kaydi yok: LoadCharacter hicbir seyi degistirmez ---
			AMMOCharacter* P4 = Pawns[4];
			P4->LoadCharacter();
			const bool bSaved4 = P4->SaveCharacterForTravel();
			UMMOAchievementComponent* A4 = P4->GetAchievementComponent();
			if (A4) { A4->NotifyMobKilled(FName(TEXT("Goblin"))); } // basarimlar S4 yuvasina baglanir, sayac 1
			Check(bSaved4 && A4 && A4->GetCounters().KillTotal == 1 && P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_fixture_loaded_registered"));
			const uint32 Serial4 = P4->GetLoadSerial();
			const FMMOCharacterSaveData Inv4 = InvSnapshot(P4);
			P4->GetPlayerState()->SetPlayerName(NLoadX); // MMOLoad oncesi canli ad degisti (host/yonetici yolu)
			const bool bReload = P4->LoadCharacter();
			Check(!bReload, TEXT("f3_reload_unsaved_name_returns_false"));
			Check(P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_reload_keeps_active_slot"));
			Check(P4->GetLoadSerial() == Serial4, TEXT("f3_reload_keeps_load_serial"));
			Check(P4->IsInitialLoadComplete(), TEXT("f3_reload_keeps_initial_load_complete"));
			Check(SameInv(Inv4, InvSnapshot(P4)) && !Store->Exists(SlotOf(NLoadX)), TEXT("f3_reload_no_starter_pack_no_new_slot"));
			if (A4) { A4->NotifyMobKilled(FName(TEXT("Goblin"))); }
			Check(A4 && A4->GetCounters().KillTotal == 2, TEXT("f3_reload_keeps_achievement_state"));
			// Kontrol: kayitli ada donunce yeniden yukleme yine calisir (erken donus yalniz kayitsiz ad icin).
			P4->GetPlayerState()->SetPlayerName(NLoad);
			const bool bReload2 = P4->LoadCharacter();
			Check(bReload2 && P4->GetLoadSerial() == Serial4 + 1 && P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_reload_registered_name_still_loads"));

			// ================= Tur 4 (Y-NEW-1): kanonik ad cakismasi + Unicode ad temizleme =================
			const ESearchCase::Type CS = ESearchCase::CaseSensitive;
			auto SpawnPossessed = [&](const FString& Name, double Dx) -> AMMOCharacter*
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
					FVector(2100000. + Dx, 2130000., 5000.), FRotator::ZeroRotator, Params);
				AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>();
				ExtraPawns.Add(Pawn); ExtraControllers.Add(PC);
				if (!Pawn || !PC) return nullptr;
				Pawn->SetActorTickEnabled(false); PC->SetActorTickEnabled(false);
				PC->Possess(Pawn);
				if (!Pawn->GetPlayerState()) return nullptr;
				Pawn->GetPlayerState()->SetPlayerName(Name);
				return Pawn;
			};
			AMMOGameMode* GM = Cast<AMMOGameMode>(World->GetAuthGameMode());
			AMMOCharacter* HoldSpace = SpawnPossessed(NHoldSpace, 0.);
			AMMOCharacter* HoldCase = SpawnPossessed(NHoldCase, 3000.);
			AMMOCharacter* HoldUni = SpawnPossessed(NHoldUni, 6000.);
			AMMOCharacter* Req = SpawnPossessed(NReq, 9000.);
			AMMOPlayerController* Newcomer = World->SpawnActor<AMMOPlayerController>();
			ExtraControllers.Add(Newcomer);
			if (Newcomer) { Newcomer->SetActorTickEnabled(false); }
			const bool bY1Fixture = GM && HoldSpace && HoldCase && HoldUni && Req && Newcomer && Newcomer->PlayerState;
			Check(bY1Fixture, TEXT("y1_fixture_online_holders"));
			if (bY1Fixture)
			{
				Newcomer->PlayerState->SetPlayerName(NNewcomer);
				// Yuva adi: ASCII disi harfler atilir (eski FChar::IsAlnum yolu onlari dosya adina tasirdi).
				Check(HoldUni->GetSaveSlotName().Equals(SlotOf(NameOf(TEXT("x"))), CS), TEXT("y1_save_slot_drops_unicode_letters"));
				Check(HoldSpace->GetSaveSlotName().Equals(SlotOf(NameOf(TEXT("S_T"))), CS), TEXT("y1_save_slot_space_to_underscore"));

				// PreLogin: cevrimici ad ile ayni kanonik anahtar (bosluk/'_' ve harf buyuklugu esdeger) ikinci baglantida reddedilir.
				auto PreLoginError = [GM](const FString& Requested)
				{
					FString Err;
					GM->PreLogin(FString(TEXT("?Name=")) + Requested, TEXT("127.0.0.1"), FUniqueNetIdRepl(), Err);
					return Err;
				};
				Check(!PreLoginError(NameOf(TEXT("s_t"))).IsEmpty() && !PreLoginError(NameOf(TEXT("S_T"))).IsEmpty(), TEXT("y1_prelogin_space_underscore_rejected"));
				Check(!PreLoginError(NameOf(TEXT("om"))).IsEmpty() && !PreLoginError(NameOf(TEXT("OM"))).IsEmpty(), TEXT("y1_prelogin_case_variant_rejected"));
				Check(!PreLoginError(MMONameLockMix(NameOf(TEXT("")), { LowerOUml }, TEXT("x"))).IsEmpty(), TEXT("y1_prelogin_unicode_cleaned_collision_rejected"));
				Check(PreLoginError(NPreFree).IsEmpty(), TEXT("y1_prelogin_free_name_accepted"));
				// Tur 5: temizleme adi degistiriyorsa (Unicode harf ya da izinsiz isaret) cakisma olmasa da ret; ret metni ASCII.
				const FString ErrUni = PreLoginError(MMONameLockMix(NameOf(TEXT("")), { SCedilla }, TEXT("Z")));
				const FString ErrPunct = PreLoginError(NameOf(TEXT("!")));
				bool bErrAscii = !ErrUni.IsEmpty();
				for (const TCHAR Ch : ErrUni) { bErrAscii &= (Ch > 0 && Ch < 128); }
				Check(bErrAscii && !ErrPunct.IsEmpty(), TEXT("y5_prelogin_altered_name_rejected"));
				Check(!PreLoginError(TEXT("Default")).IsEmpty() && !PreLoginError(TEXT("default")).IsEmpty() && !PreLoginError(TEXT("Player123")).IsEmpty(),
					TEXT("y5_prelogin_reserved_rejected"));
				// Yalniz bas/son bosluk kirpmasi ad degisikligi sayilmaz; ?Name= verilmeyen misafir burada reddedilmez (ChangeName Gezgin verir).
				FString GuestErr;
				GM->PreLogin(FString(), TEXT("127.0.0.1"), FUniqueNetIdRepl(), GuestErr);
				Check(PreLoginError(FString(TEXT(" ")) + NPreFree + TEXT(" ")).IsEmpty() && GuestErr.IsEmpty(), TEXT("y5_prelogin_trim_and_guest_accepted"));

				// ChangeName (baslangic adi): ayni kanonik anahtar cevrimiciyse rastgele Gezgin adi atanir.
				auto NewcomerName = [Newcomer]() { return Newcomer->PlayerState ? Newcomer->PlayerState->GetPlayerName() : FString(); };
				GM->ChangeName(Newcomer, NameOf(TEXT("s_t")), false);
				const FString AfterSpace = NewcomerName();
				GM->ChangeName(Newcomer, NameOf(TEXT("OM")), false);
				const FString AfterCase = NewcomerName();
				GM->ChangeName(Newcomer, MMONameLockMix(NameOf(TEXT("")), { LowerOUml }, TEXT("x")), false);
				const FString AfterUni = NewcomerName();
				Check(AfterSpace.StartsWith(TEXT("Gezgin"), CS) && AfterCase.StartsWith(TEXT("Gezgin"), CS), TEXT("y1_changename_canonical_collision_gets_gezgin"));
				Check(AfterUni.StartsWith(TEXT("Gezgin"), CS), TEXT("y1_changename_unicode_cleaned_collision_gets_gezgin"));
				// Tur 5: Unicode harfli ad artik kirpilip kabul EDILMEZ (eski kontrol "S-cedilla + Z -> Z kabul" idi: "Omer" -> "mer"
				// gibi baskasinin kaydini aciyordu); izinsiz isaret ("Ali!") ve ayrilmis adlar (Default, motor misafir adi Player<Id>)
				// da rastgele Gezgin adi alir; atanan Gezgin adinin diskte kaydi yoktur.
				GM->ChangeName(Newcomer, MMONameLockMix(NameOf(TEXT("")), { SCedilla }, TEXT("Z")), false);
				const FString AfterUniFree = NewcomerName();
				Check(AfterUniFree.StartsWith(TEXT("Gezgin"), CS) && !AfterUniFree.Equals(NameOf(TEXT("Z")), CS), TEXT("y5_changename_unicode_gets_gezgin"));
				GM->ChangeName(Newcomer, NameOf(TEXT("!")), false);
				const FString AfterPunct = NewcomerName();
				Check(AfterPunct.StartsWith(TEXT("Gezgin"), CS) && !AfterPunct.Equals(NameOf(TEXT("")), CS), TEXT("y5_changename_punct_gets_gezgin"));
				bool bReservedGezgin = true;
				for (const TCHAR* Reserved : { TEXT("Player256"), TEXT("Player123"), TEXT("Default"), TEXT("default") })
				{
					GM->ChangeName(Newcomer, Reserved, false);
					const FString Got = NewcomerName();
					bReservedGezgin &= Got.StartsWith(TEXT("Gezgin"), CS) && !MMONameRules::IsReservedName(Got) && !Store->Exists(SlotOf(Got));
				}
				Check(bReservedGezgin, TEXT("y5_changename_reserved_gets_gezgin"));
				// Kontrol: gecerli ad (bas/son bosluk kirpilarak) aynen kalir.
				GM->ChangeName(Newcomer, FString(TEXT(" ")) + NFree + TEXT(" "), false);
				Check(NewcomerName().Equals(NFree, CS), TEXT("y5_changename_trimmed_free_name_kept"));

				// ServerSetName: canli bir oyuncuyla ayni kanonik anahtara gecis reddedilir; Unicode harfli ad reddedilir.
				Req->MMOSetName(NameOf(TEXT("s_t")));
				const bool bSpaceKept = LiveName(Req).Equals(NReq, CS);
				Req->MMOSetName(NameOf(TEXT("S_T")));
				Check(bSpaceKept && LiveName(Req).Equals(NReq, CS), TEXT("y1_setname_space_underscore_rejected"));
				Req->MMOSetName(NameOf(TEXT("om")));
				const bool bCaseKept = LiveName(Req).Equals(NReq, CS);
				Req->MMOSetName(NameOf(TEXT("OM")));
				Check(bCaseKept && LiveName(Req).Equals(NReq, CS), TEXT("y1_setname_case_variant_rejected"));
				Req->MMOSetName(MMONameLockMix(NameOf(TEXT("")), { UpperOUml }, TEXT("y")));
				Check(LiveName(Req).Equals(NReq, CS), TEXT("y1_setname_unicode_rejected"));
				// Tur 5: ayrilmis adlar reddedilir; pawn'i olmayan bagli oyuncunun (Newcomer: PlayerState var, pawn yok) adi da
				// cevrimici sayilir (PlayerArray kontrolu).
				Req->MMOSetName(TEXT("Player123"));
				const bool bPlayerKept = LiveName(Req).Equals(NReq, CS);
				Req->MMOSetName(TEXT("default"));
				Check(bPlayerKept && LiveName(Req).Equals(NReq, CS), TEXT("y5_setname_reserved_rejected"));
				Req->MMOSetName(NFree.ToLower());
				Check(LiveName(Req).Equals(NReq, CS) && NewcomerName().Equals(NFree, CS), TEXT("y5_setname_pawnless_online_name_rejected"));
				Req->MMOSetName(NReqOk); // kontrol: serbest ad kabul edilir (bu adim sogumayi baslatir; sona konuldu)
				Check(LiveName(Req).Equals(NReqOk, CS), TEXT("y1_setname_free_name_accepted"));

				// Klan adi: yeni klan yalniz ASCII; tur 4 oncesi kayittan gelen Unicode harfli ad yuklemede korunur.
				const FString UniClan = MMONameLockMix(TEXT("Kartal"), { SCedilla }, TEXT(""));
				Check(!UMMOClanComponent::IsValidClanName(UniClan) && !UMMOClanComponent::IsValidClanName(MMONameLockMix(TEXT("K"), { UpperOUml }, TEXT("pru")))
					&& UMMOClanComponent::IsValidClanName(TEXT("Kartal1")), TEXT("y1_clan_name_ascii_only"));
				UMMOClanComponent* UniCC = HoldUni->GetClanComponent();
				if (UniCC) { UniCC->CmdCreate(UniClan); }
				Check(UniCC && !UniCC->IsInClan(), TEXT("y1_clan_create_unicode_rejected"));
				if (UniCC)
				{
					FMMOCharacterSaveData Legacy;
					Legacy.ClanName = MMONameLockMix(TEXT("Eski"), { SCedilla, DotlessI }, TEXT("K"));
					UniCC->ApplySaveData(Legacy);
					const FString KeptLegacy = UniCC->ClanName;
					FMMOCharacterSaveData Junk;
					Junk.ClanName = TEXT("a b!");
					UniCC->ApplySaveData(Junk);
					Check(KeptLegacy.Equals(Legacy.ClanName, CS) && !UniCC->IsInClan(), TEXT("y1_clan_legacy_unicode_name_kept_on_load"));
					UniCC->ApplySaveData(FMMOCharacterSaveData());
				}
			}

			// ================= Tur 4 (DUSUK-5) + tur 3 (Y8): ad kilidi reddinden hemen sonra =================
			// Ikinci istek "X sn bekle" degil kilit metnini gorur ve sogumada diske (Exists) hic bakilmaz.
			AMMOCharacter* LockPawn = SpawnPossessed(NLockReg, 12000.);
			AMMOPlayerController* LockPC = LockPawn ? Cast<AMMOPlayerController>(LockPawn->GetController()) : nullptr;
			AMMOHUD* LockHUD = nullptr;
			if (LockPC) { LockPC->ClientSetHUD(AMMOHUD::StaticClass()); LockHUD = Cast<AMMOHUD>(LockPC->GetHUD()); }
			UMMOCharacterStoreSubsystem* StoreSub = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UMMOCharacterStoreSubsystem>() : nullptr;
			bool bLockSaved = false;
			if (LockPawn) { LockPawn->LoadCharacter(); bLockSaved = LockPawn->SaveCharacterForTravel(); }
			const bool bY8Fixture = LockPawn && LockHUD && StoreSub && StoreSub->GetStore() && bLockSaved && Store->Exists(SlotOf(NLockReg));
			Check(bY8Fixture, TEXT("y8_fixture_registered_remote_with_hud"));
			if (bY8Fixture)
			{
				LockPawn->SetRemoteNameRulesForTest(true);
				TUniquePtr<FMMONameLockCountingStore> CountingOwned = MakeUnique<FMMONameLockCountingStore>(*StoreSub->GetStore());
				FMMONameLockCountingStore* Counting = CountingOwned.Get();
				TUniquePtr<IMMOCharacterStore> RealStore = StoreSub->SwapStoreForTest(MoveTemp(CountingOwned));
				auto LockLines = [LockHUD]()
				{
					int32 N = 0;
					for (const FMMOChatLine& Ln : LockHUD->GetChatLinesForTests()) { if (Ln.Text.StartsWith(TEXT("Ad kilitli"), ESearchCase::CaseSensitive)) ++N; }
					return N;
				};
				const int32 Lines0 = LockLines();
				const int32 Exists0 = Counting->ExistsCalls;
				LockPawn->MMOSetName(NLockTo1);
				const int32 Exists1 = Counting->ExistsCalls;
				Check(LiveName(LockPawn).Equals(NLockReg, CS) && Exists1 == Exists0 + 1 && LockLines() == Lines0 + 1, TEXT("y8_lock_reject_reads_disk_once_shows_lock_text"));
				// Ayni metin 0.5 sn icinde tekrar gonderilmez (MMONotify): araya baska bir satir girer ki ikinci kilit metni gorulebilsin.
				MMONotify::ToOwner(LockPawn, TEXT("[test] ara satir"), FColor::White, 1.f);
				LockPawn->MMOSetName(NLockTo2);
				const TArray<FMMOChatLine>& AfterLines = LockHUD->GetChatLinesForTests();
				Check(LiveName(LockPawn).Equals(NLockReg, CS) && LockLines() == Lines0 + 2 && AfterLines.Num() > 0
					&& AfterLines.Last().Text.StartsWith(TEXT("Ad kilitli"), ESearchCase::CaseSensitive), TEXT("y8_cooldown_after_lock_shows_lock_text"));
				Check(Counting->ExistsCalls == Exists1, TEXT("y8_cooldown_after_lock_no_disk_exists"));
				TUniquePtr<IMMOCharacterStore> Back = StoreSub->SwapStoreForTest(MoveTemp(RealStore));
				Check(StoreSub->GetStore() == Store && Back.Get() == Counting, TEXT("y8_store_restored"));
			}

			// ================= Tur 4 (Y-NEW-6) + tur 3 (Y3): ActiveSaveSlot dolu, ilk yukleme kapali, canli adin kaydi yok =================
			AMMOCharacter* GatePawn = SpawnPossessed(NGate, 15000.);
			Check(GatePawn && !Store->Exists(SlotOf(NGate)) && !Store->Exists(SlotOf(NGateX)), TEXT("g_fixture_unsaved_live_name"));
			if (GatePawn)
			{
				const FMMOCharacterSaveData InvG = InvSnapshot(GatePawn);
				// G: ayni adla yeniden possess (kayit yok, ayni yuva) -> kayit kapisi yeniden acilir; seri/envanter/yuva degismez.
				GatePawn->SetSaveIdentityForTest(false, SlotOf(NGate), 7);
				const bool bG1 = GatePawn->LoadCharacter();
				Check(!bG1 && GatePawn->IsInitialLoadComplete() && GatePawn->GetLoadSerial() == 7 && GatePawn->GetActiveSaveSlot().Equals(SlotOf(NGate), CS)
					&& SameInv(InvG, InvSnapshot(GatePawn)) && !Store->Exists(SlotOf(NGate)), TEXT("g_same_name_repossess_reopens_save_gate"));
				// Harf buyuklugu farkli canli ad ayni kanonik yuvadir: kapi yine acilir.
				GatePawn->SetSaveIdentityForTest(false, SlotOf(NGate), 7);
				GatePawn->GetPlayerState()->SetPlayerName(NGate.ToLower());
				const bool bG2 = GatePawn->LoadCharacter();
				Check(!bG2 && GatePawn->IsInitialLoadComplete() && GatePawn->GetLoadSerial() == 7 && GatePawn->GetActiveSaveSlot().Equals(SlotOf(NGate), CS),
					TEXT("g_canonical_case_variant_reopens"));
				// Bolge gecisi kaydi onaylanmissa G dali kapali kalir.
				GatePawn->SetSaveIdentityForTest(false, SlotOf(NGate), 7);
				GatePawn->GetPlayerState()->SetPlayerName(NGate);
				GatePawn->SetTravelSaveCommitted(true);
				const bool bG3 = GatePawn->LoadCharacter();
				Check(!bG3 && !GatePawn->IsInitialLoadComplete() && GatePawn->GetLoadSerial() == 7, TEXT("g_travel_committed_stays_closed"));
				GatePawn->SetTravelSaveCommitted(false);
				// Y3: farkli (kaydi olmayan) canli ad -> false, kapi kapali kalir, yeni karakter dali (seri++, baslangic paketi, yeni yuva) YOK.
				GatePawn->SetSaveIdentityForTest(false, SlotOf(NGate), 7);
				GatePawn->GetPlayerState()->SetPlayerName(NGateX);
				const bool bG4 = GatePawn->LoadCharacter();
				Check(!bG4 && !GatePawn->IsInitialLoadComplete(), TEXT("y3_unloaded_other_unsaved_name_gate_stays_closed"));
				Check(GatePawn->GetLoadSerial() == 7 && GatePawn->GetActiveSaveSlot().Equals(SlotOf(NGate), CS) && SameInv(InvG, InvSnapshot(GatePawn))
					&& !Store->Exists(SlotOf(NGateX)) && !Store->Exists(SlotOf(NGate)), TEXT("y3_unloaded_other_name_no_fork_no_serial"));
			}
		}

		// Temizlik: once kaliciligi kapat (EndPlay/UnPossess kaydi olmasin), sonra controller/pawn, sonra GUID'li dosyalar.
		Pawns.Append(ExtraPawns);
		Controllers.Append(ExtraControllers);
		for (AMMOCharacter* Pawn : Pawns)
		{
			if (IsValid(Pawn))
			{
				Pawn->SetRemoteNameRulesForTest(false);
				Pawn->SetSaveIdentityForTest(false, Pawn->GetActiveSaveSlot(), Pawn->GetLoadSerial());
				Pawn->SetTravelSaveCommitted(true);
			}
		}
		for (AMMOPlayerController* PC : Controllers)
		{
			if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
		}
		for (AMMOCharacter* Pawn : Pawns) { if (IsValid(Pawn)) Pawn->Destroy(); }
		if (IsValid(HostPawn))
		{
			HostPawn->SetRemoteNameRulesForTest(false);
			HostPawn->SetSaveIdentityForTest(false, HostPawn->GetActiveSaveSlot(), HostPawn->GetLoadSerial());
			HostPawn->SetTravelSaveCommitted(true);
			HostPawn->SetPlayerState(nullptr);
		}
		if (IsValid(HostAI)) { HostAI->UnPossess(); HostAI->Destroy(); }
		if (IsValid(HostPawn)) HostPawn->Destroy();
		if (IsValid(HostPS)) HostPS->Destroy();
		for (const FString& N : AllNames)
		{
			const FString Slot = SlotOf(N);
			if (Store->Exists(Slot) && !UGameplayStatics::DeleteGameInSlot(Slot, 0))
			{
				bPassed = false;
				if (OutError.IsEmpty()) OutError = TEXT("temporary slot cleanup failed");
			}
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				IFileManager::Get().Delete(*(FPaths::Combine(AchDir, Slot + TEXT(".json")) + Ext), false, true, true);
			}
		}
		return bPassed; // Ayni senkron kare: ad kurallari ve LoadCharacter kapilari dogrulanir; ag/RPC/PIE kaniti degil.
	}
}
// Bu testin KANITLAMADIKLARI: gercek uzak istemci (ServerSetName RPC'si, IsLocallyControlled=false olan ag baglantisi; burada
// SetRemoteNameRulesForTest bayragi uzak + yonetici disi oyuncuyu benzetir), yonetici muafiyetinin kendisi (MMOAdmin::IsServerAdmin
// ULocalPlayer'li host PC ister; burada yerel muafiyet AI denetleyiciyle, yani UsesRemoteNameRules=false dalindan kanitlanir),
// dedicated sunucuda yonetici yoklugu, 30 sn
// sogumanin zamanla bitmesi (tek karede yalniz baslatildigi gorulur), magaza yokken muhafazakar ret, gercek MMOLoad komut
// yetki kapisi, ?Name= ile yeniden baglanma akisi, PIE ve ag replikasyonu.
// Tur 4 bolumu: PreLogin gercek ag baglantisi olmadan dogrudan cagrilir (UniqueId bos, adres sahte); ChangeName'in 3 sn sonraki
// bildirim zamanlayicisi kosulmaz; Y8 kilit metni standalone'da yerel calisan istemci RPC'si + test HUD'u ile, Exists sayisi
// SwapStoreForTest ile takilan sayac magazasiyla olculur; tur 5 misafir/ayrilmis ad kurali motorun gercek InitNewPlayer
// ("Player<Id>") yolu yerine ChangeName dogrudan cagrilarak; Y-NEW-6 G dali LoadCharacter dogrudan cagrilarak (0.6 sn AutoLoad
// zamanlayicisi ve gercek UnPossess/Possess dongusu olmadan) dogrulanir. Eski Turkce harfli klan adinin korunmasi FChar::IsAlnum'un
// bu platformdaki sonucuna baglidir (NOTE satiri).

namespace
{
	class FMMONameLockTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestNameLock"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = RunMMONameLockSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_NAMELOCK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMONameLockTestExec GMMONameLockTestExec;
}
#endif
