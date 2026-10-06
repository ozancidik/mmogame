// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

/**
 * Sunucu yoneticisi (admin/debug) yetkisinin TEK karar noktasi (NETWORK_AUDIT dedicated engeli 2).
 * Eskiden bolge gecisi, PvP anahtari ve test komutlari her yerde ayri "IsLocalController()" kontroluyle
 * kilitliydi; yetki modeli degistiginde (hesap/token) hepsini ayri ayri bulmak gerekirdi.
 */
namespace MMOAdmin
{
	/**
	 * Verilen oyuncu denetleyicisi sunucu yoneticisi mi?
	 * Su an (listen-server): yalniz sunucuyu baslatan oyuncunun YEREL denetleyicisi. Uzak istemci false.
	 * Dedicated sunucuda yerel denetleyici olmadigindan HER ZAMAN false: bolge gecisi, PvP anahtari ve
	 * debug RPC'leri kapali kalir.
	 * ILERIDE: dedicated icin hesap/token tabanli yonetici listesi (ornegin -MMOAdminToken veya
	 * hesap kimligi izin listesi) burada, yalniz bu fonksiyonda eklenecek; cagri yerleri degismez.
	 */
	MYPROJECT_API bool IsServerAdmin(const APlayerController* PC);
}
