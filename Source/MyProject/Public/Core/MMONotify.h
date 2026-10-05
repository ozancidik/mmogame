// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Sunucu mantiginin oyuncuya kisa durum mesaji gostermesi icin tek yol (NETWORK_AUDIT O11 madde 3).
 * Eskiden mesajlar "GEngine && IsLocallyControlled()" ile ekrana basiliyordu; sunucuda uzak
 * oyuncunun pawn'i yerel olmadigindan "Canta dolu!", "Altin yetersiz" gibi mesajlar uzak
 * istemcilere hic ulasmiyordu (listen-server'da bile).
 */
namespace MMONotify
{
	/**
	 * Mesaj turu. Result: sonuc satiri (satin alma, gorev, sinif, kayit...) - uzak istemcide kalici
	 * sohbet sistem satiri (Reliable). Hint: gecici ret/ipucu ("Altin yetersiz", "Menzilde hedef yok")
	 * - uzak istemcide karakter ustunde kisa balon (Unreliable ClientShowHint), sohbeti doldurmaz.
	 * Yerel (host) pawn'da ikisi de ekran mesajidir.
	 */
	enum class EMMONotifyKind : uint8
	{
		Result,
		Hint
	};

	/**
	 * OwnerActor bir APawn ise: yerel kontrollu pawn'da ekrana basar; uzak istemcinin pawn'inda (sunucuda)
	 * Kind'a gore sahibine chat RPC'siyle (Result, sistem kanali) ya da hint RPC'siyle (Hint) iletir.
	 * Ayni pawn + ayni metin ~0.5 sn icinde tekrar gonderilmez (uzak yol; ret yolu spam'i RPC kuyrugunu sisirmesin).
	 * Pawn degilse hicbir sey yapmaz. Kind verilmezse Result (eski davranis).
	 */
	MYPROJECT_API void ToOwner(const AActor* OwnerActor, const FString& Text, const FColor& Color, float Duration,
		EMMONotifyKind Kind = EMMONotifyKind::Result);
}
