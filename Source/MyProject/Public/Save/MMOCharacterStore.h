// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOSaveGame.h"

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
};
