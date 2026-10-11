// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mail/MMOMailStore.h"
#include "Core/MMONameRules.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	// Y1: kurtarma/karantina tasimalari tekrar denemesiz (motorun 10x0.5 sn beklemesi sunucuyu dondururdu). Basarisiz bir
	// tasimadan sonra ayni kutu 60 sn denenmez; bu surede Load false doner (kutu bos sayilmaz, uzerine yazilmaz).
	constexpr double MailRecoveryBackoffSeconds = 60.0;

	TMap<FString, double>& MailRecoveryBackoffMap()
	{
		static TMap<FString, double> Map; // yalniz oyun is parcacigi (sunucu)
		return Map;
	}

	bool MailInRecoveryBackoff(const FString& Path)
	{
		const double* Until = MailRecoveryBackoffMap().Find(Path);
		return Until && FPlatformTime::Seconds() < *Until;
	}

	void MailStartRecoveryBackoff(const FString& Path)
	{
		MailRecoveryBackoffMap().Add(Path, FPlatformTime::Seconds() + MailRecoveryBackoffSeconds);
	}

	bool MailMoveNoRetry(const FString& Dest, const FString& Src)
	{
		return IFileManager::Get().Move(*Dest, *Src, true, true, false, true);
	}

	enum class EMailReadResult : uint8 { Ok, Missing, Unreadable, Corrupt };

	EMailReadResult MailReadBoxFile(const FString& File, TArray<FMMOMail>& Out)
	{
		Out.Reset();
		if (!IFileManager::Get().FileExists(*File))
		{
			return EMailReadResult::Missing;
		}
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *File))
		{
			return EMailReadResult::Unreadable;
		}
		if (!FMMOMailStore::FromJson(Raw, Out))
		{
			Out.Reset();
			return EMailReadResult::Corrupt;
		}
		return EMailReadResult::Ok;
	}

	// Ana kutu yok. Kurtarma sirasi: 1) .bak (basarisiz ya da yarida kalan bir yazimdan ONCEKI gecerli kutu; yazim basarili
	// bildirilmedigi icin cagiran ona gore davranmadi), 2) .tmp (ana dosya hic yokken yarida kalan yazim). .bak secilirse ayni
	// yazimin .tmp'si gecersizdir ve .tmp.bad'e ayrilir. true = ana dosya yerinde ya da kurtarilacak dosya yok.
	bool MailRecoverMissingBox(const FString& Path, const FString& Tmp, const FString& Bak)
	{
		IFileManager& FM = IFileManager::Get();
		const bool bHasBak = FM.FileExists(*Bak);
		if (!bHasBak && !FM.FileExists(*Tmp))
		{
			return true;
		}
		if (MailInRecoveryBackoff(Path))
		{
			return false;
		}
		const FString& Src = bHasBak ? Bak : Tmp;
		if (!MailMoveNoRetry(Path, Src))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Yarim kalan yazim kurtarilamadi (60 sn sonra yeniden denenecek): %s"), *Src);
			MailStartRecoveryBackoff(Path);
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Yarim kalan yazim kurtarildi: %s"), *Src);
		if (bHasBak && FM.FileExists(*Tmp) && !MailMoveNoRetry(Tmp + TEXT(".bad"), Tmp))
		{
			FM.Delete(*Tmp, false, true, true);
		}
		return true;
	}
}

FString FMMOMailStore::SanitizeName(const FString& Name)
{
	// Tur 4 (Y-NEW-1): yalniz ASCII (karakter kayit yuvasi ile ayni kural, MMONameRules::SlotKey). Turkce harfli eski kutular
	// (MMOMail_<ad>.json) icin migrasyon yok; o adlar artik oyuncu adi olarak secilemez.
	const FString Safe = MMONameRules::SlotKey(Name, MaxNameLength);
	return Safe.IsEmpty() ? FString(TEXT("Default")) : Safe;
}

FString FMMOMailStore::GetBoxPath(const FString& Name)
{
	// Kucuk harf: Linux sunucuda "Ali" ve "ali" ayri dosya olup altin kaybettirmesin (kimlik karsilastirmasi zaten harf duyarsiz).
	// "MMOMail_" oneki: Windows'ta ayrilmis adlar (con, nul, aux, com1...) tek basina dosya adi olamaz.
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOMail"), FString(TEXT("MMOMail_")) + SanitizeName(Name).ToLower() + TEXT(".json"));
}

bool FMMOMailStore::ToJson(const TArray<FMMOMail>& Mails, FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FMMOMail& M : Mails)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("from"), M.From);
		O->SetStringField(TEXT("text"), M.Text);
		O->SetNumberField(TEXT("gold"), M.Gold);
		O->SetNumberField(TEXT("sent"), static_cast<double>(M.SentUnix));
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("mails"), Arr);
	OutJson.Reset();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool FMMOMailStore::FromJson(const FString& Json, TArray<FMMOMail>& OutMails)
{
	OutMails.Reset();
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("mails"), Arr) || !Arr)
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		if (OutMails.Num() >= MaxMailsPerBox)
		{
			break;
		}
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!V.IsValid() || !V->TryGetObject(O) || !O || !O->IsValid())
		{
			continue;
		}
		FMMOMail M;
		double Gold = 0.0, Sent = 0.0;
		(*O)->TryGetStringField(TEXT("from"), M.From);
		(*O)->TryGetStringField(TEXT("text"), M.Text);
		(*O)->TryGetNumberField(TEXT("gold"), Gold);
		(*O)->TryGetNumberField(TEXT("sent"), Sent);
		M.Gold = static_cast<int32>(FMath::Clamp(Gold, 0.0, static_cast<double>(MaxGoldPerMail)));
		M.SentUnix = static_cast<int64>(FMath::Max(0.0, Sent));
		M.From = M.From.Left(MaxNameLength);
		M.Text = M.Text.Left(MaxTextLength);
		if (M.From.IsEmpty() || (M.Text.IsEmpty() && M.Gold <= 0))
		{
			continue;
		}
		OutMails.Add(MoveTemp(M));
	}
	return true;
}

bool FMMOMailStore::Load(const FString& Name, TArray<FMMOMail>& OutMails)
{
	OutMails.Reset();
	if (!IsDeliverableName(Name))
	{
		return false;
	}
	const FString Path = GetBoxPath(Name);
	const FString Tmp = Path + TEXT(".tmp");
	const FString Bak = Path + TEXT(".bak");
	IFileManager& FM = IFileManager::Get();
	// Atomik yazim (.tmp -> Move) hedefi once siler: arada cokerse ana dosya yok olabilir. Once .bak, yoksa .tmp geri konur
	// (aksi halde kutu "bos" sayilir ve icindeki altin kaybolurdu). Geri konamazsa false: bos sayip ustune yazma.
	if (!FM.FileExists(*Path) && !MailRecoverMissingBox(Path, Tmp, Bak))
	{
		return false;
	}
	switch (MailReadBoxFile(Path, OutMails))
	{
	case EMailReadResult::Missing:
		return true; // kutu yok = bos
	case EMailReadResult::Unreadable:
		return false; // var olan kutu okunamadi: bos sayip ustune yazma
	case EMailReadResult::Ok:
		// Basarili yazimin silinemeyen .bak'i: ileride ana dosya kaybolursa eski (toplanmis) kutuyu diriltmesin.
		if (FM.FileExists(*Bak))
		{
			FM.Delete(*Bak, false, true, true);
		}
		return true;
	case EMailReadResult::Corrupt:
		break;
	}
	// Bozuk kutu: bu cagri BASARISIZ (false). Dosya .bad'a ayrilir; ayni anda .bak varsa (basarisiz yazimin gecerli eski
	// kutusu) geri konup kullanilir. Ayrilamazsa 60 sn beklenir: bozuk dosya ne bos sayilir ne de uzerine yazilir.
	if (MailInRecoveryBackoff(Path))
	{
		return false;
	}
	if (!MailMoveNoRetry(Path + TEXT(".bad"), Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Bozuk posta kutusu ayrilamadi (60 sn sonra yeniden denenecek): %s"), *Path);
		MailStartRecoveryBackoff(Path);
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Bozuk posta kutusu .bad olarak ayrildi: %s"), *Path);
	// Eski bir .tmp de duruyorsa onu da ayir: ana dosya artik yok, bir sonraki Load o .tmp'yi "yarim yazim" sanip geri
	// koyardi (zaten toplanmis ya da iade edilmis posta dirilirdi = altin kopyalama).
	if (FM.FileExists(*Tmp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Bozuk kutunun eski .tmp'si .tmp.bad olarak ayrildi: %s"), *Tmp);
		if (!MailMoveNoRetry(Tmp + TEXT(".bad"), Tmp))
		{
			FM.Delete(*Tmp, false, true, true);
		}
	}
	if (FM.FileExists(*Bak) && MailRecoverMissingBox(Path, Tmp, Bak) && MailReadBoxFile(Path, OutMails) == EMailReadResult::Ok)
	{
		return true;
	}
	OutMails.Reset();
	return false;
}

#if !UE_BUILD_SHIPPING
void FMMOMailStore::ResetRecoveryBackoffForTest()
{
	MailRecoveryBackoffMap().Reset();
}
#endif

// DEGISMEZ KURAL: false donerse kutu yazimdan onceki haliyle kalir (ya da kurtarmada o hale doner); true = yeni icerik diskte.
// Sira: 1) mevcut ana dosyanin .bak kopyasi (alinamazsa hic yazilmaz)  2) .tmp'ye yaz  3) .tmp -> ana (Move)
// 4) olmazsa dogrudan ana yola yaz  5) o da olmazsa .bak geri konur, .tmp silinir. Basarida .bak silinir.
static bool WriteBox(const FString& Name, const TArray<FMMOMail>& Mails)
{
	FString Json;
	if (!FMMOMailStore::ToJson(Mails, Json))
	{
		return false;
	}
	const FString Path = FMMOMailStore::GetBoxPath(Name);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Tmp = Path + TEXT(".tmp");
	const FString Bak = Path + TEXT(".bak");
	IFileManager& FM = IFileManager::Get();
	const bool bHadMain = FM.FileExists(*Path);
	if (bHadMain && FM.Copy(*Bak, *Path, true, true) != COPY_OK)
	{
		// Yedeksiz yazim eski kutuyu riske atar (QA ORTA-1): hic dokunmadan basarisiz.
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Kutu yedegi (.bak) alinamadi, yazim yapilmadi: %s"), *Path);
		FM.Delete(*Bak, false, true, true);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		// Ana dosyaya dokunulmadi; cagiran basarisiz sayar (gonderimde iade eder). Yarim/eski .tmp kalirsa ve ana dosya yoksa
		// (ilk posta) bir sonraki Load onu geri koyardi: iade edilmis posta teslim de edilirdi. Bu yuzden .tmp silinir.
		FM.Delete(*Tmp, false, true, true);
		FM.Delete(*Bak, false, true, true);
		// Tur 4 (QA DUSUK-2): ilk postada (ana dosya yok) silinemeyen .tmp kurtarmada ana kutu olarak geri konup iade edilmis
		// postayi teslim ederdi. .tmp.bad'a ayrilir (kurtarma onu okumaz); o da olmazsa asagidaki ilk-posta politikasiyla ayni:
		// yazim GERCEKLESMIS sayilir (true, iade yok; altin kopyalanmaz, en kotu kaybolur).
		if (!bHadMain && FM.FileExists(*Tmp) && !MailMoveNoRetry(Tmp + TEXT(".bad"), Tmp))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Basarisiz ilk yazimin .tmp'si silinemedi/ayrilamadi; teslim edilebilir, yazim basarili sayildi: %s"), *Tmp);
			return true;
		}
		return false;
	}
	// Tur 4 (Y-NEW-2): tekrar denemesiz Move. Motorun varsayilan 10x0.5 sn denemesi kilitli bir .tmp/ana dosyada sunucuyu ~5.5 sn
	// dondururdu; basarisizlikta asagidaki dogrudan yazim / .bak geri koyma yolu ayni kalir.
	bool bWritten = MailMoveNoRetry(Path, Tmp);
	if (!bWritten)
	{
		// A4: Move basarisiz (kilit, antivirus vb.): dogrudan nihai yola yazmayi dene.
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Kutu tasima (Move) basarisiz, dogrudan yazim deneniyor: %s"), *Path);
		bWritten = FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	if (bWritten)
	{
		FM.Delete(*Tmp, false, true, true);
		FM.Delete(*Bak, false, true, true);
		return true;
	}
	if (bHadMain)
	{
		// Move hedefi silip tasiyamamis ya da dogrudan yazim yarim kalmis olabilir: eski kutu .bak'tan geri konur. Geri
		// konamazsa .bak durur ve sonraki Load onu .tmp'den ONCE kurtarir (bozuk ana dosya da .bad'a ayrilip .bak geri konur).
		if (!MailMoveNoRetry(Path, Bak))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Eski kutu .bak'tan hemen geri konamadi; sonraki yuklemede kurtarilacak: %s"), *Bak);
		}
		FM.Delete(*Tmp, false, true, true);
		return false;
	}
	// Onceden kutu yoktu (ilk posta): eski durum = dosya yok. Yarim/tam yazilmis ana dosya ve .tmp silinir: cagiran iade eder,
	// sonraki Load iade edilmis postayi bir de teslim etmesin (cift teslim). Silinemeyen dosya kurtarmada/okumada teslim
	// edilebilecegi icin o durumda yazim GERCEKLESMIS sayilir (iade edilmez; altin kopyalanmaz, en kotu kaybolur).
	// Tur 4 (QA DUSUK-3): ana dosya KOSULSUZ silinmez. bHadMain bir FileExists yanlis negatifinden geldiyse burada duran dosya
	// gercek (eski) kutudur: silinmek yerine .bad'a ayrilir (incelenebilir; Load .bad'i okumaz, iade edilen posta teslim edilmez).
	if (FM.FileExists(*Path) && !MailMoveNoRetry(Path + TEXT(".bad"), Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Basarisiz ilk yazimin ana dosyasi .bad'a ayrilamadi: %s"), *Path);
	}
	FM.Delete(*Tmp, false, true, true);
	if (FM.FileExists(*Tmp))
	{
		MailMoveNoRetry(Tmp + TEXT(".bad"), Tmp); // silinemeyen .tmp kurtarmada geri konmasin (QA DUSUK-2)
	}
	if (!FM.FileExists(*Path) && !FM.FileExists(*Tmp))
	{
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Basarisiz ilk yazimin dosyasi silinemedi; teslim edilebilir, yazim basarili sayildi: %s"), *Path);
	return true;
}

bool FMMOMailStore::Append(const FString& Name, const FMMOMail& Mail)
{
	if (!IsDeliverableName(Name) || Mail.From.IsEmpty() || Mail.From.Len() > MaxNameLength
		|| Mail.Text.Len() > MaxTextLength || Mail.Gold < 0 || Mail.Gold > MaxGoldPerMail
		|| (Mail.Text.IsEmpty() && Mail.Gold <= 0))
	{
		return false;
	}
	TArray<FMMOMail> Box;
	if (!Load(Name, Box) || Box.Num() >= MaxMailsPerBox)
	{
		return false;
	}
	Box.Add(Mail);
	return WriteBox(Name, Box);
}

bool FMMOMailStore::Replace(const FString& Name, const TArray<FMMOMail>& Mails)
{
	if (!IsDeliverableName(Name) || Mails.Num() > MaxMailsPerBox)
	{
		return false;
	}
	return WriteBox(Name, Mails);
}

bool FMMOMailStore::TakeAll(const FString& Name, TArray<FMMOMail>& OutMails)
{
	OutMails.Reset();
	if (!Load(Name, OutMails))
	{
		return false;
	}
	if (OutMails.IsEmpty())
	{
		return true;
	}
	// Once kutuyu bosalt, sonra cagiran odul versin: yazma basarisizsa hicbir sey verilmez (cift odeme yok).
	if (!WriteBox(Name, TArray<FMMOMail>()))
	{
		OutMails.Reset();
		return false;
	}
	return true;
}

int32 FMMOMailStore::Count(const FString& Name)
{
	TArray<FMMOMail> Box;
	return Load(Name, Box) ? Box.Num() : 0;
}

void FMMOMailStore::DeleteBox(const FString& Name)
{
	const FString Path = GetBoxPath(Name);
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bad")), false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".tmp")), false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".tmp.bad")), false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
}
