// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Oyuncu/klan adi karakter kurali ve kayit yuvasi kanonik anahtari: TEK karar noktasi (Y-NEW-1, guvenlik tur 4).
 * Eskiden her temizleyici FChar::IsAlnum kullaniyordu (Windows'ta iswalnum: Turkce/Unicode harfleri gecirir). Karsilastirmalar
 * ESearchCase::IgnoreCase ile (yalniz ASCII harf duyarsiz) yapiliyordu; NTFS ise Unicode'da da harf duyarsiz: buyuk ve kucuk
 * O-umlaut ile baslayan "Omer"/"omer" farkli ad sayilip AYNI MMOSave_*.sav dosyasina yaziliyordu (ayni kayda ikinci oturum). Artik adlar yalniz ASCII:
 * [A-Za-z0-9 _-]. Turkce harfli ad kabul edilmez (ICU tabanli kanonik anahtar: docs/DECISIONS_NEEDED.md).
 * Header-only inline: unity derlemede ad cakismasi yok.
 */
namespace MMONameRules
{
	/** Kullaniciya gorunen ret metni (ASCII). */
	inline const TCHAR* AllowedCharsHint()
	{
		return TEXT("Adda yalniz harf (a-z, A-Z; Turkce harf yok), rakam, bosluk, _ ve - kullanilabilir");
	}

	/** ASCII harf ya da rakam (FChar::IsAlnum DEGIL: o Unicode harfleri de kabul eder). */
	inline bool IsAsciiAlnum(TCHAR Ch)
	{
		return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
	}

	/** Oyuncu adinda izinli karakter: ASCII harf/rakam, '_', '-', bosluk. */
	inline bool IsAllowedNameChar(TCHAR Ch)
	{
		return IsAsciiAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-') || Ch == TEXT(' ');
	}

	/** Ad yalniz izinli karakterlerden mi olusuyor (bos ad true doner; uzunluk ayrica denetlenir). */
	inline bool HasOnlyAllowedChars(const FString& Name)
	{
		for (const TCHAR Ch : Name)
		{
			if (!IsAllowedNameChar(Ch))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Kayit yuvasi/dosya adi parcasi: izinli ASCII karakterler kalir, bosluk -> '_', digerleri atilir, en cok MaxLen.
	 * Buyuk/kucuk harf KORUNUR (eski kayit dosyalarinin adi). Bos sonuc mumkun (cagiran "Default" vb. secer).
	 */
	inline FString SlotKey(const FString& Name, int32 MaxLen = 32)
	{
		FString Out;
		Out.Reserve(FMath::Min(Name.Len(), MaxLen));
		for (const TCHAR Ch : Name)
		{
			if (Out.Len() >= MaxLen)
			{
				break;
			}
			if (IsAsciiAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
			{
				Out.AppendChar(Ch);
			}
			else if (Ch == TEXT(' '))
			{
				Out.AppendChar(TEXT('_'));
			}
		}
		return Out;
	}

	/**
	 * Kanonik anahtar: SlotKey + ASCII kucuk harf. Iki ad ayni anahtari veriyorsa AYNI kayit dosyasidir ("A B" = "a_b").
	 * "Ad kullanimda mi / ayni karakter cevrimici mi" kontrolleri ham ada degil buna bakar (PreLogin, ChangeName, ServerSetName).
	 */
	inline FString CanonicalKey(const FString& Name)
	{
		// Bos anahtar AMMOCharacter::GetSaveSlotName gibi "Default" yuvasina duser.
		const FString Key = SlotKey(Name).ToLower();
		return Key.IsEmpty() ? FString(TEXT("default")) : Key;
	}

	/** Iki yuva/dosya adi ayni kanonik dosyayi mi gosteriyor (ASCII harf duyarsiz). */
	inline bool SameCanonical(const FString& A, const FString& B)
	{
		return A.ToLower() == B.ToLower();
	}

	/** Adin kayit yuvasinin kanonik bicimi ("mmosave_" + CanonicalKey): AMMOCharacter::GetSaveSlotName ile SameCanonical esdegeri. */
	inline FString CanonicalSlot(const FString& Name)
	{
		return FString(TEXT("mmosave_")) + CanonicalKey(Name);
	}

	/**
	 * Tur 5 (misafir adi): oyuncunun ISTEYEREK alamayacagi adlar. Kanonik anahtari "default" olan ad (bos/temizlenince bos ad
	 * da buraya duser: MMOSave_Default ortak yedek yuvadir) ve "player" + yalniz rakam (motorun ?Name= verilmeyen istemciye
	 * verdigi "Player<PlayerId>"; Id her sunucu surecinde 256'dan yeniden baslar, baska bir misafir oncekinin kaydina duserdi).
	 * "Player" (rakamsiz), "PlayerOne" ve ChangeName'in atadigi "Gezgin######" adlari ayrilmis DEGIL.
	 */
	inline bool IsReservedName(const FString& Name)
	{
		const FString Key = CanonicalKey(Name);
		if (Key.Equals(TEXT("default"), ESearchCase::CaseSensitive))
		{
			return true;
		}
		constexpr int32 PlayerPrefixLen = 6; // "player"
		if (Key.Len() <= PlayerPrefixLen || !Key.StartsWith(TEXT("player"), ESearchCase::CaseSensitive))
		{
			return false;
		}
		for (int32 I = PlayerPrefixLen; I < Key.Len(); ++I)
		{
			if (Key[I] < TEXT('0') || Key[I] > TEXT('9'))
			{
				return false;
			}
		}
		return true;
	}

	/** Ayrilmis ad ret metni (ASCII). */
	inline const TCHAR* ReservedNameHint()
	{
		return TEXT("Bu ad ayrilmis (Default ve Player+rakam, orn. Player123): baska bir ad sec");
	}
}
