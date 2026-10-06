// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOSaveGame.h"

/**
 * Yuva basina yazim sirasi (numara) takibi. Cagiranlar her yazimdan once BeginWrite ile artan bir
 * numara alir; asenkron callback geldiginde ayni yuvaya daha YENI bir yazim baslamissa IsNewest
 * false doner ve eski yazimin sonucu yok sayilir. IMMOCharacterStore::GetWriteTracker() bir
 * TSharedRef verir: callback'ler onu yakalayabilir, magaza yok olsa bile gecerli kalir.
 */
class MYPROJECT_API FMMOSlotWriteTracker
{
public:
	/** Yuva icin yeni (daha onceki hepsinden buyuk) yazim numarasi verir. Yuva adi buyuk/kucuk harf duyarsiz. */
	uint64 BeginWrite(const FString& SlotName)
	{
		const uint64 Ticket = ++LastTicket;
		Newest.Add(SlotName.ToLower(), Ticket);
		return Ticket;
	}

	/** Ticket hala bu yuvadaki en yeni yazim mi. */
	bool IsNewest(const FString& SlotName, uint64 Ticket) const
	{
		const uint64* Found = Newest.Find(SlotName.ToLower());
		return Found && *Found == Ticket;
	}

private:
	uint64 LastTicket = 0;
	TMap<FString, uint64> Newest;
};

/**
 * Karakter kalicilik arayuzu (saf C++, UObject DEGIL). Sunucu tarafi; senkron API ve
 * bulut uygulamalarina uygun asenkron callback adaptoru.
 *
 * Amac: karakter kaydinin nereye yazildigini (bugun yerel .sav, ileride bulut) tek bir arkaya
 * almak (docs/ACCOUNT_BACKEND_COMPARISON.md Asama 1). "Yuva adi" (SlotName) bugunku kayit
 * kimligidir (GetSaveSlotName: "MMOSave_<ad>"); hesap sistemi gelince karakter kimligine donusecek.
 *
 * Sozlesme (yerel dosya uygulamasinin bugunku davranisi):
 *  - Hepsi YALNIZ sunucuda/host'ta cagrilir (kimlik/otorite kontrolu cagiranda).
 *  - Load: yuva yoksa ya da okunamazsa false. Out yalniz true donunce anlamlidir. Verinin
 *    gecerliligi (bValid) cagiranda denetlenir; magaza veriyi yorumlamaz.
 *  - Save: yazim basarisiz olursa false; cagiran basarisizlikta durumunu ilerletmemelidir.
 *  - Exists: yuva diskte/depoda var mi.
 *  - ListSlotNames: karakter yuvalarinin adlari (klan adi cakisma taramasi icin). Bos olabilir.
 *
 * ASENKRON SOZLESME: Gelecekteki bulut magazasi SaveAsync/LoadAsync'i gecikmeli tamamlar. Yerel
 * magaza varsayilan adaptoru senkron islemi yapip callback'i ayni karede cagirir. Bolge gecisi
 * SaveCharacterForTravelAsync callback'leri tamamlanmadan ServerTravel baslatmaz.
 *
 * YAZIM SIRASI (yuva basina): cagiranlar (AMMOCharacter) her yazimdan once GetWriteTracker()->BeginWrite(Yuva)
 * ile numara alir; ayni yuvaya daha YENI bir yazim baslamissa eski asenkron yazimin sonucu YOK SAYILIR
 * (basari sayilmaz, ActiveSaveSlot guncellenmez, bolge gecisi iptal olur) ve eski anlik goruntu yeni
 * veriyi geri sarmis gibi islem gormez. Bu YALNIZ callback tarafinin korumasidir: verinin depoda hangi
 * sirayla INDIGINI belirlemez. Bulut magazasi ek olarak yuva basina seri yazim kuyrugu ya da
 * karsilastir-ve-yaz (Revision/ExpectedRevision) uygulamalidir (yerel senkron magaza icin gerekmez).
 * Bilinen sinir: zaman asimi sonrasi ucusta kalan bir yazim iptal EDILEMEZ (docs/NETWORK_AUDIT.md O6).
 */
class MYPROJECT_API IMMOCharacterStore
{
public:
	using FLoadComplete = TFunction<void(bool bSuccess, FMMOCharacterSaveData&& Data)>;
	using FSaveComplete = TFunction<void(bool bSuccess)>;

	virtual ~IMMOCharacterStore() = default;

	virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) = 0;
	virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) = 0;
	virtual bool Exists(const FString& SlotName) = 0;
	virtual void ListSlotNames(TArray<FString>& Out) = 0;

	/**
	 * Bulut uygulamalari icin asenkron giris noktalari. Yerel magaza varsayilan olarak senkron
	 * islemi yapip callback'i ayni karede cagirir. Uzak uygulama callback'i daha sonra ama oyun
	 * is parcaciginda cagirmalidir; SaveAsync, In verisini donmeden once kendi omrune kopyalamalidir.
	 */
	virtual void LoadAsync(const FString& SlotName, FLoadComplete&& Completion)
	{
		FMMOCharacterSaveData Data;
		const bool bSuccess = Load(SlotName, Data);
		if (Completion)
		{
			Completion(bSuccess, MoveTemp(Data));
		}
	}

	virtual void SaveAsync(const FString& SlotName, const FMMOCharacterSaveData& In, FSaveComplete&& Completion)
	{
		const bool bSuccess = Save(SlotName, In);
		if (Completion)
		{
			Completion(bSuccess);
		}
	}

	/** Yuva basina yazim sirasi takipcisi (bkz. sinif yorumu: YAZIM SIRASI). */
	const TSharedRef<FMMOSlotWriteTracker>& GetWriteTracker() const { return WriteTracker; }

private:
	TSharedRef<FMMOSlotWriteTracker> WriteTracker = MakeShared<FMMOSlotWriteTracker>();
};
