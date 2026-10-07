// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/MMODialogueSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"

bool UMMODialogueSubsystem::RunBudgetSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !World->GetGameInstance())
	{ OutError = TEXT("server game world required"); return false; }
	// Kayitli GI subsystem'i DEGIL: Initialize/Deinitialize ve dosya/HTTP yollarina girilmez.
	UMMODialogueSubsystem* D = NewObject<UMMODialogueSubsystem>(World->GetGameInstance());
	auto Check = [&](bool Condition, const TCHAR* Label)
	{ if (!Condition) { OutError = Label; return false; } ++OutChecks; return true; };
	auto Reset = [&]()
	{
		D->MaxDailyRequests = 200; D->MaxSessionRequests = 400;
		D->MaxRequestsPerMinute = 30; D->MaxConcurrentRequests = 4;
		D->SessionRequestCount = 0; D->DailyRequestCount = 0; D->DailyBudgetDay = 10;
		D->InFlightRequests = 0; D->RecentRequestTimes.Reset(); D->PlayerDailyCounts.Reset();
	};
	Reset();
	if (!Check(NormalizePlayerKey(TEXT("  ALICE  ")) == TEXT("alice")
		&& NormalizePlayerKey(TEXT("\t\r\n")) == TEXT("?")
		&& NormalizePlayerKey(FString::ChrN(100, TEXT('a'))).Len() == 64
		&& !NormalizePlayerKey(TEXT("ab\tcd\nEF")).Contains(TEXT("\t")), TEXT("player key normalization"))) return false;
	D->MaxDailyRequests = 1500; const bool bDefaultQuota = D->GetPerPlayerDailyQuota() == 375;
	D->MaxDailyRequests = 1; const bool bSmallQuota = D->GetPerPlayerDailyQuota() == 50;
	D->MaxDailyRequests = 0;
	if (!Check(bDefaultQuota && bSmallQuota && D->GetPerPlayerDailyQuota() == 0, TEXT("quota formula"))) return false;
	Reset();
	if (!Check(D->TryReserveRequest(100, TEXT("alice")) && D->SessionRequestCount == 1
		&& D->DailyRequestCount == 1 && D->InFlightRequests == 1 && D->PlayerDailyCounts.FindRef(TEXT("alice")) == 1
		&& D->RecentRequestTimes.Num() == 1, TEXT("reservation counters"))) return false;
	Reset(); D->PlayerDailyCounts.Add(TEXT("alice"), 50);
	if (!Check(!D->TryReserveRequest(100, TEXT("alice")) && D->DailyRequestCount == 0
		&& D->InFlightRequests == 0 && D->RecentRequestTimes.IsEmpty(), TEXT("player quota charged rejected request"))) return false;
	Reset(); D->SessionRequestCount = 400;
	if (!Check(!D->TryReserveRequest(100, TEXT("alice")) && D->DailyRequestCount == 0, TEXT("session limit"))) return false;
	Reset(); D->DailyRequestCount = 200;
	if (!Check(!D->TryReserveRequest(100, TEXT("alice")) && D->SessionRequestCount == 0, TEXT("daily limit"))) return false;
	Reset(); D->RecentRequestTimes.Init(99, 30);
	if (!Check(!D->TryReserveRequest(100, TEXT("alice")) && D->DailyRequestCount == 0, TEXT("minute limit"))) return false;
	Reset(); D->InFlightRequests = 4;
	if (!Check(!D->TryReserveRequest(100, TEXT("alice")) && D->DailyRequestCount == 0, TEXT("concurrency limit"))) return false;
	Reset(); D->MaxRequestsPerMinute = 1; D->RecentRequestTimes.Add(40);
	const bool bBoundaryRejected = !D->TryReserveRequest(100, TEXT("alice"));
	if (!Check(bBoundaryRejected && D->TryReserveRequest(100.001, TEXT("alice"))
		&& D->RecentRequestTimes.Num() == 1, TEXT("60 second window boundary"))) return false;
	Reset(); D->TryReserveRequest(100, TEXT("alice"));
	if (!Check(D->ReleaseRequestBudget(TEXT("alice"), 10, true) && D->SessionRequestCount == 0
		&& D->DailyRequestCount == 0 && D->InFlightRequests == 0 && D->PlayerDailyCounts.FindRef(TEXT("alice")) == 0
		&& D->RecentRequestTimes.Num() == 1, TEXT("refunded attempt lost minute throttle or leaked budget"))) return false;
	Reset(); D->SessionRequestCount = 2; D->DailyRequestCount = 3; D->InFlightRequests = 1;
	D->PlayerDailyCounts.Add(TEXT("alice"), 3);
	if (!Check(!D->ReleaseRequestBudget(TEXT("alice"), 9, true) && D->SessionRequestCount == 1
		&& D->DailyRequestCount == 3 && D->PlayerDailyCounts.FindRef(TEXT("alice")) == 3
		&& D->InFlightRequests == 0, TEXT("old day refund debited current day"))) return false;
	Reset();
	for (int32 i = 0; i < MaxTrackedPlayers; ++i) D->PlayerDailyCounts.Add(FString::Printf(TEXT("key%d"), i), 0);
	if (!Check(!D->TryReserveRequest(100, TEXT("new-key")) && D->DailyRequestCount == 0
		&& D->SessionRequestCount == 0 && D->InFlightRequests == 0 && D->RecentRequestTimes.IsEmpty()
		&& D->PlayerDailyCounts.Num() == MaxTrackedPlayers, TEXT("full player table bypassed per-player quota"))) return false;
	if (!Check(D->TryReserveRequest(100, TEXT("key0")) && D->PlayerDailyCounts.FindRef(TEXT("key0")) == 1,
		TEXT("full player table blocked tracked player"))) return false;
	D->PlayerDailyCounts.FindChecked(TEXT("key1")) = 50;
	if (!Check(!D->TryReserveRequest(100, TEXT("key1")) && D->DailyRequestCount == 1, TEXT("tracked quota at full table"))) return false;
	D->MaxDailyRequests = 0;
	if (!Check(D->TryReserveRequest(100, TEXT("unlimited-key")) && D->PlayerDailyCounts.Num() == MaxTrackedPlayers,
		TEXT("explicit unlimited mode or memory ceiling changed"))) return false;
	const int32 SessionBeforeDay = D->SessionRequestCount; const int32 FlightBeforeDay = D->InFlightRequests;
	if (!Check(D->ResetDailyBudgetForDay(11) && D->DailyBudgetDay == 11 && D->DailyRequestCount == 0
		&& D->PlayerDailyCounts.IsEmpty() && D->SessionRequestCount == SessionBeforeDay
		&& D->InFlightRequests == FlightBeforeDay && D->RecentRequestTimes.Num() == 2, TEXT("day reset altered session/rate state"))) return false;
	D->DailyRequestCount = 3; D->PlayerDailyCounts.Add(TEXT("alice"), 3);
	if (!Check(!D->ResetDailyBudgetForDay(11) && D->DailyRequestCount == 3
		&& D->PlayerDailyCounts.FindRef(TEXT("alice")) == 3, TEXT("same day reset counters"))) return false;
	Reset(); D->MaxRequestsPerMinute = 1; D->TryReserveRequest(100, TEXT("alice"));
	D->ReleaseRequestBudget(TEXT("alice"), 10, true);
	if (!Check(!D->TryReserveRequest(101, TEXT("alice")) && D->SessionRequestCount == 0,
		TEXT("refunded attempts bypassed minute cap"))) return false;
	Reset(); D->TryReserveRequest(100, TEXT("alice")); D->RecentRequestTimes.RemoveSingle(100);
	if (!Check(D->ReleaseRequestBudget(TEXT("alice"), 10, true) && D->RecentRequestTimes.IsEmpty()
		&& D->DailyRequestCount == 0 && D->SessionRequestCount == 0 && D->InFlightRequests == 0,
		TEXT("unstarted request cleanup"))) return false;
	Reset(); D->PlayerDailyCounts.Add(TEXT("alice"), 0); D->ReleaseRequestBudget(TEXT("alice"), 10, true);
	if (!Check(D->DailyRequestCount == 0 && D->SessionRequestCount == 0 && D->InFlightRequests == 0
		&& D->PlayerDailyCounts.FindRef(TEXT("alice")) == 0, TEXT("refund underflow"))) return false;
	Reset(); D->TryReserveRequest(100, TEXT("alice"));
	if (!Check(!D->ReleaseRequestBudget(TEXT("alice"), 10, false) && D->InFlightRequests == 0
		&& D->DailyRequestCount == 1 && D->SessionRequestCount == 1 && D->PlayerDailyCounts.FindRef(TEXT("alice")) == 1,
		TEXT("retained attempt was refunded"))) return false;
	Reset(); D->SessionRequestCount = 399;
	const bool bLastSession = D->TryReserveRequest(100, TEXT("alice"));
	if (!Check(bLastSession && !D->TryReserveRequest(101, TEXT("bob")) && D->SessionRequestCount == 400,
		TEXT("last session reservation boundary"))) return false;
	Reset(); D->DailyRequestCount = 199;
	const bool bLastDaily = D->TryReserveRequest(100, TEXT("alice"));
	if (!Check(bLastDaily && !D->TryReserveRequest(101, TEXT("bob")) && D->DailyRequestCount == 200,
		TEXT("last daily reservation boundary"))) return false;
	Reset(); D->PlayerDailyCounts.Add(TEXT("alice"), 49);
	const bool bLastPlayer = D->TryReserveRequest(100, NormalizePlayerKey(TEXT(" ALICE ")));
	if (!Check(bLastPlayer && !D->TryReserveRequest(101, NormalizePlayerKey(TEXT("Alice")))
		&& D->PlayerDailyCounts.FindRef(TEXT("alice")) == 50 && D->PlayerDailyCounts.Num() == 1,
		TEXT("normalized player quota boundary"))) return false;
	Reset();
	for (int32 i = 0; i < MaxTrackedPlayers - 1; ++i) D->PlayerDailyCounts.Add(FString::Printf(TEXT("key%d"), i), 0);
	const bool bLastTracked = D->TryReserveRequest(100, TEXT("last-key"));
	return Check(bLastTracked && !D->TryReserveRequest(101, TEXT("next-key")) && D->DailyRequestCount == 1
		&& D->PlayerDailyCounts.Num() == MaxTrackedPlayers && D->PlayerDailyCounts.FindRef(TEXT("last-key")) == 1,
		TEXT("last tracked player boundary"));
}

namespace
{
	struct FMMODialogueBudgetTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestDialogueBudget"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMODialogueSubsystem::RunBudgetSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_DIALOGUE_BUDGET_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMODialogueBudgetTestExec GMMODialogueBudgetTestExec;
}
#endif

namespace
{
	// Saglayiciya gore anahtarin okunacagi ortam degiskeninin adi.
	const TCHAR* ApiKeyEnvVar(EMMOLLMProvider Provider)
	{
		return Provider == EMMOLLMProvider::NVIDIA ? TEXT("NVIDIA_API_KEY") : TEXT("ANTHROPIC_API_KEY");
	}

	// Anahtar SADECE ortam degiskeninden okunur; koda gomulmez, .ini'ye yazilmaz.
	FString GetApiKeyFor(EMMOLLMProvider Provider)
	{
		return FPlatformMisc::GetEnvironmentVariable(ApiKeyEnvVar(Provider));
	}
}

bool UMMODialogueSubsystem::HasApiKey() const
{
	return !GetApiKeyFor(Provider).IsEmpty();
}

namespace
{
	// UTC gun numarasi (1 Ocak 0001'den beri gecen gun).
	int64 UtcDayNumber()
	{
		return FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerDay;
	}

	FString DailyBudgetFilePath()
	{
		return FPaths::ProjectSavedDir() / TEXT("MMO/LLMDailyBudget.txt");
	}

	// Yalniz rakamlardan olusan, en fazla MaxDigits haneli negatif olmayan sayi. Bozuk metin -> false.
	bool ParseDigits(const FString& Text, int32 MaxDigits, int64& Out)
	{
		const FString T = Text.TrimStartAndEnd();
		if (T.IsEmpty() || T.Len() > MaxDigits)
		{
			return false;
		}
		for (const TCHAR C : T)
		{
			if (C < TEXT('0') || C > TEXT('9'))
			{
				return false;
			}
		}
		Out = FCString::Atoi64(*T);
		return true;
	}
}

void UMMODialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadDailyLimitConfig();
}

void UMMODialogueSubsystem::Deinitialize()
{
	// Kapanista bekleyen sayac degisikligini zorla yaz (yazim seyreltildigi icin).
	if (bBudgetDirty)
	{
		WriteDailyBudgetFile();
	}
	Super::Deinitialize();
}

void UMMODialogueSubsystem::LoadDailyLimitConfig()
{
	// Oncelik: komut satiri > DefaultGame.ini [MMO] LlmDailyRequests > sinif varsayilani (1500).
	int32 Value = 0;
	if (GConfig && GConfig->GetInt(TEXT("MMO"), TEXT("LlmDailyRequests"), Value, GGameIni))
	{
		MaxDailyRequests = FMath::Max(0, Value);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("MMOLlmDaily="), Value))
	{
		MaxDailyRequests = FMath::Max(0, Value);
	}
	UE_LOG(LogTemp, Display, TEXT("[MMODialogue] Gunluk LLM tavani=%d (oyuncu basi kota=%d; 0=sinirsiz)."),
		MaxDailyRequests, GetPerPlayerDailyQuota());
}

FString UMMODialogueSubsystem::NormalizePlayerKey(const FString& PlayerKey)
{
	FString Key = PlayerKey.TrimStartAndEnd().ToLower().Left(64);
	for (TCHAR& C : Key)
	{
		if (C < 32) // sekme/satir sonu dosya bicimini bozmasin
		{
			C = TEXT(' ');
		}
	}
	return Key.IsEmpty() ? FString(TEXT("?")) : Key;
}

void UMMODialogueSubsystem::RollDailyBudget()
{
	if (!bDailyBudgetLoaded)
	{
		bDailyBudgetLoaded = true;
		// Dosya bicimi: 1. satir "<utc gun numarasi> <sayac>", sonraki satirlar "<anahtar>\t<sayac>" (oyuncu basi).
		// Yok/bozuk/eksik ise bugun icin 0'dan baslanir. Once ana dosya, o yoksa/gecersizse gecici dosya
		// (silme ile tasima arasinda kesilen yazimdan kurtarma) denenir.
		const FString Path = DailyBudgetFilePath();
		for (const FString& Candidate : { Path, Path + TEXT(".tmp") })
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Candidate))
			{
				continue;
			}
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			FString DayPart, CountPart;
			int64 Day = 0, Count = 0;
			if (Lines.Num() == 0 || !Lines[0].TrimStartAndEnd().Split(TEXT(" "), &DayPart, &CountPart)
				|| !ParseDigits(DayPart, 10, Day) || !ParseDigits(CountPart, 9, Count))
			{
				continue; // bu aday bozuk
			}
			DailyBudgetDay = Day;
			DailyRequestCount = static_cast<int32>(Count);
			PlayerDailyCounts.Reset();
			for (int32 i = 1; i < Lines.Num() && PlayerDailyCounts.Num() < MaxTrackedPlayers; ++i)
			{
				FString KeyPart, PlayerCountPart;
				int64 PlayerCount = 0;
				if (Lines[i].Split(TEXT("\t"), &KeyPart, &PlayerCountPart) && !KeyPart.IsEmpty()
					&& ParseDigits(PlayerCountPart, 9, PlayerCount))
				{
					PlayerDailyCounts.Add(NormalizePlayerKey(KeyPart), static_cast<int32>(PlayerCount));
				}
			}
			break;
		}
	}
	if (ResetDailyBudgetForDay(UtcDayNumber()))
	{
		NoteBudgetChanged(true);
	}
}

bool UMMODialogueSubsystem::ResetDailyBudgetForDay(int64 Today)
{
	if (DailyBudgetDay == Today) return false;
	DailyBudgetDay = Today;
	DailyRequestCount = 0;
	PlayerDailyCounts.Reset();
	return true;
}

void UMMODialogueSubsystem::NoteBudgetChanged(bool bForce)
{
	bBudgetDirty = true;
	++UnsavedBudgetChanges;
	// Her cagrida degil: 10 degisiklikte ya da son yazimdan 30 sn sonra (ya da zorla) diske iner.
	if (bForce || UnsavedBudgetChanges >= 10 || FPlatformTime::Seconds() - LastBudgetSaveTime >= 30.0)
	{
		WriteDailyBudgetFile();
	}
}

void UMMODialogueSubsystem::WriteDailyBudgetFile()
{
	// Atomik yazim: once gecici dosyaya, sonra ana dosyanin yerine tasi (yarim yazim ana dosyayi bozmaz).
	// Yazilamazsa sayac bellekte calismaya devam eder; yalniz yeniden baslatmada kaybolur.
	bBudgetDirty = false;
	UnsavedBudgetChanges = 0;
	LastBudgetSaveTime = FPlatformTime::Seconds();

	FString Text = FString::Printf(TEXT("%lld %d\n"), DailyBudgetDay, DailyRequestCount);
	for (const TPair<FString, int32>& Pair : PlayerDailyCounts)
	{
		Text += FString::Printf(TEXT("%s\t%d\n"), *Pair.Key, Pair.Value);
	}

	const FString Path = DailyBudgetFilePath();
	const FString TmpPath = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *TmpPath)
		|| !IFileManager::Get().Move(*Path, *TmpPath, true, true, false, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Gunluk sayac dosyasi yazilamadi; bellekte devam."));
		IFileManager::Get().Delete(*TmpPath, false, true, true);
		bBudgetDirty = true; // sonraki sinirda yeniden denenir
	}
}

bool UMMODialogueSubsystem::RefundRequest(const FString& PlayerKey, int64 RequestDay)
{
	SessionRequestCount = FMath::Max(0, SessionRequestCount - 1);
	if (RequestDay == DailyBudgetDay)
	{
		DailyRequestCount = FMath::Max(0, DailyRequestCount - 1);
		if (int32* PlayerCount = PlayerDailyCounts.Find(PlayerKey))
		{
			*PlayerCount = FMath::Max(0, *PlayerCount - 1);
		}
		return true;
	}
	return false;
}

bool UMMODialogueSubsystem::ReleaseRequestBudget(const FString& PlayerKey, int64 RequestDay, bool bRefund)
{
	InFlightRequests = FMath::Max(0, InFlightRequests - 1);
	return bRefund && RefundRequest(PlayerKey, RequestDay);
}

void UMMODialogueSubsystem::RequestNpcLine(const FString& NpcName, const FString& NpcRole,
	const FString& PlayerInfo, FMMOOnDialogueReady OnReady, const FString& PlayerKey)
{
	// GUVENLIK (SECURITY_AUDIT Y-4, prompt enjeksiyonu): LLM'e oyuncunun yazdigi HICBIR metin gecmez.
	// NpcName/NpcRole tasarimci verisidir (NPC aktorunun alanlari), PlayerInfo cagiranlarda sunucunun
	// kurdugu sabit sablondur ("seviye N bir maceraci"; oyuncu adi/sohbet dahil degil). Yeni cagiran
	// eklenirse serbest oyuncu metnini buraya baglama.
	// Sistem promptu = NPC'nin kimligi ve uslup kurallari. Oyun ici oldugu icin
	// kisa, ekranda tek balonluk repliklere zorluyoruz.
	const FString SystemPrompt = FString::Printf(TEXT(
		"Sen bir MMORPG oyununda '%s' adli bir NPC'sin. Rolun: %s. "
		"Oyuncuya, karakterine sadik kalarak TEK bir kisa replikle (en fazla 2 cumle) cevap ver. "
		"Turkce konus. Sistem/oyun terimlerini, madde imlerini veya XML etiketlerini kullanma; "
		"sadece NPC'nin agzindan cikacak sozu yaz."),
		*NpcName, *NpcRole);

	const FString UserPrompt = PlayerInfo.IsEmpty()
		? TEXT("Oyuncu seninle konusmaya basladi. Onu selamla.")
		: FString::Printf(TEXT("Oyuncu seninle konusuyor. Oyuncu hakkinda bilinenler: %s"), *PlayerInfo);

	SendRequest(SystemPrompt, UserPrompt, OnReady, PlayerKey);
}

void UMMODialogueSubsystem::RequestRaw(const FString& SystemPrompt, const FString& UserPrompt,
	FMMOOnDialogueReady OnReady, const FString& PlayerKey)
{
	SendRequest(SystemPrompt, UserPrompt, OnReady, PlayerKey);
}

bool UMMODialogueSubsystem::TryReserveRequest(double NowSec, const FString& PlayerKey)
{
	// Sunucu geneli hiz siniri: dakikada MaxRequestsPerMinute, ayni anda MaxConcurrentRequests.
	RecentRequestTimes.RemoveAll([NowSec](double T) { return NowSec - T > 60.0; });
	if (RecentRequestTimes.Num() >= MaxRequestsPerMinute || InFlightRequests >= MaxConcurrentRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Hiz siniri asildi (%d/dk, %d eszamanli); istek atlandi."),
			RecentRequestTimes.Num(), InFlightRequests);
		return false;
	}
	// Oturum butcesi (SECURITY_AUDIT Y-4): dolunca istek atilmaz, NPC yedek replige duser.
	if (MaxSessionRequests > 0 && SessionRequestCount >= MaxSessionRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Oturum LLM butcesi doldu (%d); istek atlandi."), MaxSessionRequests);
		return false;
	}
	// Gunluk (oturum-otesi) tavan: UTC gun degisince sayac sifirlanir; dolunca yedek replik.
	if (MaxDailyRequests > 0 && DailyRequestCount >= MaxDailyRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Gunluk LLM butcesi doldu (%d); istek atlandi."), MaxDailyRequests);
		return false;
	}
	// Oyuncu basi gunluk kota: tek oyuncu genel tavani bitirip digerlerini yedek replige dusurmesin.
	const int32 PlayerQuota = GetPerPlayerDailyQuota();
	if (PlayerQuota > 0)
	{
		const int32* Used = PlayerDailyCounts.Find(PlayerKey);
		// Yeni anahtari izleyemiyorsak oyuncu kotasini uygulayamayiz. Genel tavani
		// tuketmesine izin vermek yerine yedek replige don; mevcut oyuncular devam eder.
		if (!Used && PlayerDailyCounts.Num() >= MaxTrackedPlayers)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Oyuncu kota tablosu dolu; yeni anahtar icin istek atlandi."));
			return false;
		}
		if (Used && *Used >= PlayerQuota)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Oyuncu gunluk LLM kotasi doldu (%s, %d); istek atlandi."),
				*PlayerKey, PlayerQuota);
			return false;
		}
	}
	RecentRequestTimes.Add(NowSec);
	++InFlightRequests;
	++SessionRequestCount;
	++DailyRequestCount;
	if (PlayerDailyCounts.Contains(PlayerKey) || PlayerDailyCounts.Num() < MaxTrackedPlayers)
	{
		++PlayerDailyCounts.FindOrAdd(PlayerKey);
	}
	return true;
}

void UMMODialogueSubsystem::SendRequest(const FString& SystemPrompt, const FString& UserPrompt,
	FMMOOnDialogueReady OnReady, const FString& PlayerKeyRaw)
{
	const FString ApiKey = GetApiKeyFor(Provider);
	if (ApiKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] %s tanimli degil; istek atlaniyor."), ApiKeyEnvVar(Provider));
		OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		return;
	}

	const double NowSec = FPlatformTime::Seconds();
	const FString PlayerKey = NormalizePlayerKey(PlayerKeyRaw);
	RollDailyBudget();
	if (!TryReserveRequest(NowSec, PlayerKey))
	{
		OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		return;
	}
	const int64 RequestDay = DailyBudgetDay;
	NoteBudgetChanged();

	const bool bNvidia = (Provider == EMMOLLMProvider::NVIDIA);

	// --- Istek govdesini saglayiciya gore kur ---
	FString Body;
	FString Url;
	if (bNvidia)
	{
		// NVIDIA NIM = OpenAI-uyumlu /chat/completions:
		// { "model", "max_tokens", "temperature",
		//   "messages": [ {role:system,...}, {role:user,...} ] }
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), NvidiaModel);
		Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
		Root->SetNumberField(TEXT("temperature"), Temperature);

		const TSharedRef<FJsonObject> SysMsg = MakeShared<FJsonObject>();
		SysMsg->SetStringField(TEXT("role"), TEXT("system"));
		SysMsg->SetStringField(TEXT("content"), SystemPrompt);
		const TSharedRef<FJsonObject> UsrMsg = MakeShared<FJsonObject>();
		UsrMsg->SetStringField(TEXT("role"), TEXT("user"));
		UsrMsg->SetStringField(TEXT("content"), UserPrompt);

		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(SysMsg));
		Messages.Add(MakeShared<FJsonValueObject>(UsrMsg));
		Root->SetArrayField(TEXT("messages"), Messages);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Root, Writer);
		Url = TEXT("https://integrate.api.nvidia.com/v1/chat/completions");
	}
	else
	{
		// Anthropic Messages API:
		// { "model", "max_tokens", "system", "output_config":{effort}, "messages":[{user}] }
		// NOT: thinking parametresini GONDERMIYORUZ; effort=low ile hizli+ucuz tutuyoruz.
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), Model);
		Root->SetNumberField(TEXT("max_tokens"), AnthropicMaxTokens); // dusunme + metin birlikte (bkz. baslik)
		Root->SetStringField(TEXT("system"), SystemPrompt);

		const TSharedRef<FJsonObject> OutputConfig = MakeShared<FJsonObject>();
		OutputConfig->SetStringField(TEXT("effort"), Effort);
		Root->SetObjectField(TEXT("output_config"), OutputConfig);

		const TSharedRef<FJsonObject> UserMsg = MakeShared<FJsonObject>();
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), UserPrompt);

		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(UserMsg));
		Root->SetArrayField(TEXT("messages"), Messages);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Root, Writer);
		Url = TEXT("https://api.anthropic.com/v1/messages");
	}

	// --- HTTP istegini kur ---
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bNvidia)
	{
		// OpenAI-uyumlu kimlik: Bearer token.
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	}
	else
	{
		Request->SetHeader(TEXT("x-api-key"), ApiKey);
		Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	}

	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// Yanit callback'i. TWeakObjectPtr ile subsystem yok olduysa guvenli kal.
	TWeakObjectPtr<UMMODialogueSubsystem> WeakThis(this);
	// Eszamanli sayac yuvasi tam BIR kez birakilir: ya tamamlanma callback'inde ya da ProcessRequest
	// baslatilamazsa asagida (callback hic gelmeyebilir; sayac sizip tum NPC konusmalarini kilitlemesin).
	const TSharedRef<bool> bSlotReleased = MakeShared<bool>(false);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnReady, bNvidia, bSlotReleased, PlayerKey, RequestDay](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
		{
			// Subsystem yok olduysa (dunya kapandi vs.) hicbir sey yapma — niyet edilen omur korumasi.
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (*bSlotReleased)
			{
				return; // baslatma basarisizligi zaten islendi (cift cevap yok)
			}
			*bSlotReleased = true;
			WeakThis->ReleaseRequestBudget(PlayerKey, RequestDay, false);

			if (!bConnectedOk || !Resp.IsValid())
			{
				// Zaman asimi SAYILIR (istek saglayiciya ulasmis/faturalanmis olabilir); baglanti hatasi/iptal iade.
				const bool bTimedOut = Req.IsValid() && Req->GetFailureReason() == EHttpFailureReason::TimedOut;
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Baglanti/istek hatasi%s."),
					bTimedOut ? TEXT(" (zaman asimi; sayac iade edilmedi)") : TEXT(" (sayac iade edildi)"));
				if (!bTimedOut)
				{
					if (WeakThis->RefundRequest(PlayerKey, RequestDay)) WeakThis->NoteBudgetChanged();
				}
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
				return;
			}

			const int32 Code = Resp->GetResponseCode();
			const FString RespBody = Resp->GetContentAsString();

			if (Code != 200)
			{
				// 401/403/429/5xx...: saglayici yanit uretmedi -> sayac iade (genel/oyuncu/oturum).
				if (WeakThis->RefundRequest(PlayerKey, RequestDay)) WeakThis->NoteBudgetChanged();
				// Hata govdesi kisaltilir: bazi saglayicilar iletide anahtar parcasi yankilar (SECURITY_AUDIT D-10).
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] HTTP %d: %s"), Code, *RespBody.Left(200));
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
				return;
			}

			FString Text, Error;
			const bool bOk = bNvidia ? ParseOpenAIResponse(RespBody, Text, Error)
			                         : ParseAnthropicResponse(RespBody, Text, Error);
			if (bOk)
			{
				OnReady.ExecuteIfBound(true, Text);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Yanit ayristirilamadi: %s"), *Error);
				OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
			}
		});

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODialogue] Istek baslatilamadi."));
		if (!*bSlotReleased)
		{
			*bSlotReleased = true;
			RecentRequestTimes.RemoveSingle(NowSec); // atilmayan istek dakika sinirini tuketmesin
			if (ReleaseRequestBudget(PlayerKey, RequestDay, true)) NoteBudgetChanged();
			OnReady.ExecuteIfBound(false, TEXT("(NPC su an konusamiyor.)"));
		}
	}
}

bool UMMODialogueSubsystem::ParseOpenAIResponse(const FString& Body, FString& OutText, FString& OutError)
{
	// OpenAI/NVIDIA /chat/completions basarili yaniti:
	// { "choices": [ { "message": { "role":"assistant", "content":"..." },
	//                  "finish_reason":"stop" } ] }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON cozulemedi");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!Root->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		OutError = TEXT("choices bos");
		return false;
	}

	const TSharedPtr<FJsonObject> First = (*Choices)[0]->AsObject();
	if (!First.IsValid())
	{
		OutError = TEXT("choice[0] gecersiz");
		return false;
	}

	const TSharedPtr<FJsonObject>* MsgObj = nullptr;
	if (!First->TryGetObjectField(TEXT("message"), MsgObj) || !MsgObj->IsValid())
	{
		OutError = TEXT("message yok");
		return false;
	}

	FString Content;
	if ((*MsgObj)->TryGetStringField(TEXT("content"), Content) && !Content.TrimStartAndEnd().IsEmpty())
	{
		OutText = Content.TrimStartAndEnd();
		return true;
	}

	OutError = TEXT("content bos");
	return false;
}

bool UMMODialogueSubsystem::ParseAnthropicResponse(const FString& Body, FString& OutText, FString& OutError)
{
	// Basarili yanit sekli:
	// {
	//   "role": "assistant",
	//   "stop_reason": "end_turn" | "refusal" | "max_tokens" | ...,
	//   "content": [
	//     { "type": "thinking", "thinking": "" },   // effort>low'da olabilir; atlanir
	//     { "type": "text", "text": "NPC replik..." }
	//   ]
	// }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON cozulemedi");
		return false;
	}

	// Icerigi okumadan ONCE stop_reason'i kontrol et: guvenlik siniflandiricisi
	// istegi reddettiyse content bos/kismi olur ve [0].text okumak coker.
	FString StopReason;
	Root->TryGetStringField(TEXT("stop_reason"), StopReason);
	if (StopReason == TEXT("refusal"))
	{
		OutError = TEXT("Model istegi guvenlik nedeniyle reddetti (refusal)");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
	if (!Root->TryGetArrayField(TEXT("content"), Content) || Content->Num() == 0)
	{
		OutError = TEXT("content bos");
		return false;
	}

	// Ilk 'text' blogunu bul (thinking bloklari atlanir).
	for (const TSharedPtr<FJsonValue>& Block : *Content)
	{
		const TSharedPtr<FJsonObject> Obj = Block->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FString BlockType;
		Obj->TryGetStringField(TEXT("type"), BlockType);
		if (BlockType == TEXT("text"))
		{
			FString Text;
			if (Obj->TryGetStringField(TEXT("text"), Text) && !Text.TrimStartAndEnd().IsEmpty())
			{
				OutText = Text.TrimStartAndEnd();
				return true;
			}
		}
	}

	OutError = TEXT("text blogu bulunamadi");
	return false;
}
