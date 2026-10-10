// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Tek posta: gonderen adi, kisa metin ve istege bagli altin eki. */
struct FMMOMail
{
	FString From;
	FString Text;
	int32 Gold = 0;
	int64 SentUnix = 0; // UTC saniye
};

/**
 * Posta kutusu deposu (sunucu tarafi, dosya tabanli): Saved/MMOMail/<temiz-ad>.json. Alici cevrimdisi olabilir.
 * Kimlik oyuncu adidir (projedeki bilinen acik, docs/SECURITY_AUDIT.md); dosya adi karakter kayit yuvasi ile ayni
 * temizleme kuralini kullanir (alnum, '_', '-', bosluk -> '_', en fazla 32 karakter).
 * Tum sinirlar burada zorlanir; bilesen ve testler ayni kurallari kullanir.
 */
class MYPROJECT_API FMMOMailStore
{
public:
	static constexpr int32 MaxMailsPerBox = 20;
	static constexpr int32 MaxTextLength = 200;
	static constexpr int32 MaxGoldPerMail = 1000000;
	static constexpr int32 MaxNameLength = 32;

	// Ad -> dosya adi govdesi (bos/gecersiz adlar "Default" olur; Default kutusuna posta GONDERILMEZ).
	static FString SanitizeName(const FString& Name);
	static bool IsDeliverableName(const FString& Name) { return SanitizeName(Name) != TEXT("Default"); }
	static FString GetBoxPath(const FString& Name);

	// JSON (test edilebilir): FromJson girdiyi dogrular (metin kisaltilir, altin [0,MaxGoldPerMail], fazla posta atilir,
	// bos gonderen/metin+altin yok = atilir). Gecersiz JSON'da false.
	static bool ToJson(const TArray<FMMOMail>& Mails, FString& OutJson);
	static bool FromJson(const FString& Json, TArray<FMMOMail>& OutMails);

	// Diskten kutu. Dosya yoksa bos liste ve true; bozuksa .bad'a tasinir, bos liste ve true (kutu yeniden baslar).
	static bool Load(const FString& Name, TArray<FMMOMail>& OutMails);
	// Kutuya ekle. False: kutu dolu, gecersiz ad/metin/altin veya disk hatasi.
	static bool Append(const FString& Name, const FMMOMail& Mail);
	// Kutuyu bosaltip icerigi doner (once diske bos yazilir; cokme halinde ayni posta iki kez verilmez).
	static bool TakeAll(const FString& Name, TArray<FMMOMail>& OutMails);
	static int32 Count(const FString& Name);

	// Yalniz testler/temizlik icin.
	static void DeleteBox(const FString& Name);
};
