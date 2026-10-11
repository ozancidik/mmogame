// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mail/MMOMailStore.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FMMOMailStore::SanitizeName(const FString& Name)
{
	FString Safe;
	Safe.Reserve(FMath::Min(Name.Len(), MaxNameLength));
	for (const TCHAR Ch : Name)
	{
		if (Safe.Len() >= MaxNameLength)
		{
			break;
		}
		if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
		{
			Safe.AppendChar(Ch);
		}
		else if (Ch == TEXT(' '))
		{
			Safe.AppendChar(TEXT('_'));
		}
	}
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
	// Atomik yazim (.tmp -> Move) hedefi once siler: arada cokerse yalniz .tmp kalir. Ana dosya yoksa onu geri koy
	// (aksi halde kutu "bos" sayilir ve icindeki altin kaybolurdu).
	const FString Tmp = Path + TEXT(".tmp");
	if (!IFileManager::Get().FileExists(*Path) && IFileManager::Get().FileExists(*Tmp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Yarim kalan yazim kurtarildi: %s"), *Tmp);
		if (!IFileManager::Get().Move(*Path, *Tmp, true, true))
		{
			return false; // kurtarilamadi: bos sayip ustune yazma
		}
	}
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		return true; // kutu yok = bos
	}
	if (!FromJson(Raw, OutMails))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Bozuk posta kutusu .bad olarak ayrildi: %s"), *Path);
		IFileManager::Get().Move(*(Path + TEXT(".bad")), *Path, true);
		// Eski bir .tmp de duruyorsa onu da ayir: ana dosya artik yok, bir sonraki Load o .tmp'yi "yarim yazim" sanip geri
		// koyardi (zaten toplanmis ya da iade edilmis posta dirilirdi = altin kopyalama).
		if (IFileManager::Get().FileExists(*Tmp))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Bozuk kutunun eski .tmp'si .tmp.bad olarak ayrildi: %s"), *Tmp);
			if (!IFileManager::Get().Move(*(Tmp + TEXT(".bad")), *Tmp, true))
			{
				IFileManager::Get().Delete(*Tmp, false, true, true);
			}
		}
		OutMails.Reset();
	}
	return true;
}

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
	IFileManager& FM = IFileManager::Get();
	if (!FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		// Ana dosyaya dokunulmadi; cagiran basarisiz sayar (gonderimde iade eder). Yarim/tam .tmp kalirsa ve ana dosya yoksa
		// (ilk posta) bir sonraki Load onu geri koyardi: iade edilmis posta teslim de edilirdi. Bu yuzden .tmp silinir.
		FM.Delete(*Tmp, false, true, true);
		if (!FM.FileExists(*Path) && FM.FileExists(*Tmp))
		{
			UE_LOG(LogTemp, Error, TEXT("[MMOMAIL] Basarisiz yazimin .tmp'si silinemedi (kurtarmada geri gelebilir): %s"), *Tmp);
		}
		return false;
	}
	if (FM.Move(*Path, *Tmp, true, true))
	{
		return true;
	}
	// A4: Move basarisiz (kilit, antivirus vb.): dogrudan nihai yola yazmayi dene.
	UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Kutu tasima (Move) basarisiz, dogrudan yazim deneniyor: %s"), *Path);
	if (FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		FM.Delete(*Tmp, false, true, true);
		return true;
	}
	if (FM.FileExists(*Path))
	{
		// Ana dosya (eski ya da yarim yazilmis) duruyor: .tmp'yi sil, eski icerik gecerli; cagiran basarisiz sayar.
		// (Yarim yazilmis ana dosya bir sonraki Load'da .bad'a ayrilir: posta kaybolabilir, kopyalanmaz.)
		FM.Delete(*Tmp, false, true, true);
		return false;
	}
	// Ana dosya yok (ilk posta), .tmp tam ve yeni icerikte. Yazim BASARISIZ sayilir ve .tmp silinir: cagiran iade eder,
	// sonraki Load .tmp'yi geri koyup iade edilmis postayi bir de teslim etmesin (cift teslim). Silme de basarisizsa .tmp
	// kurtarmada geri gelecegi icin yazim GERCEKLESMIS sayilir (iade edilmez; altin kopyalanmaz).
	if (FM.Delete(*Tmp, false, true, true) || !FM.FileExists(*Tmp))
	{
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Basarisiz yazimin .tmp'si silinemedi; kurtarmada teslim edilecek, yazim basarili sayildi: %s"), *Tmp);
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
}
