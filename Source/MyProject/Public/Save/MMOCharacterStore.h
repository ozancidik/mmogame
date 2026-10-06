// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOSaveGame.h"

/**
 * Karakter kalicilik arayuzu (saf C++, UObject DEGIL). Sunucu tarafi, SENKRON API.
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
 * ASENKRON NOTU (ONEMLI): Gelecekteki bulut magazasi (HTTP) dogasi geregi asenkron olacak. Bu
 * arayuz bugun senkron oldugu icin AMMOCharacter::SaveCharacterForTravel() senkron bool doner ve
 * AMMOGameMode bolge gecisi (ServerTravel) bu donusu beklemeden ilerler. SaveCharacterForTravel
 * asenkron hale gelirse travel, kaydin tamamlanma onayi gelene kadar BEKLEMELIDIR (aksi halde
 * gecis, yazilmamis kaydi geride birakir). Asenkron magaza eklenirken bu arayuz tamamlanma
 * geri cagrisiyla genisletilecek; mevcut senkron yollar yerel dosya magazasi icin korunur.
 */
class MYPROJECT_API IMMOCharacterStore
{
public:
	virtual ~IMMOCharacterStore() = default;

	virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) = 0;
	virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) = 0;
	virtual bool Exists(const FString& SlotName) = 0;
	virtual void ListSlotNames(TArray<FString>& Out) = 0;
};
