// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMONotify.h"
#include "Player/MMOPlayerController.h"
#include "Characters/MMOCharacter.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"

namespace
{
	// Uzak yol dedupe: pawn basina son gonderilen metin ve zamani.
	constexpr double RemoteDedupeSeconds = 0.5;

	struct FLastRemoteNotify
	{
		FString Text;
		double Time = 0.0;
	};

	TMap<TWeakObjectPtr<const APawn>, FLastRemoteNotify>& GetLastRemoteNotifies()
	{
		static TMap<TWeakObjectPtr<const APawn>, FLastRemoteNotify> Map;
		return Map;
	}

	// true = bu pawn'a ayni metin az once gitti, bu seferlik atla.
	bool IsRemoteDuplicate(const APawn* Pawn, const FString& Text)
	{
		TMap<TWeakObjectPtr<const APawn>, FLastRemoteNotify>& Map = GetLastRemoteNotifies();
		const double Now = FPlatformTime::Seconds();
		if (Map.Num() > 64)
		{
			for (auto It = Map.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid() || Now - It.Value().Time > 5.0)
				{
					It.RemoveCurrent();
				}
			}
		}
		FLastRemoteNotify& Last = Map.FindOrAdd(TWeakObjectPtr<const APawn>(Pawn));
		if (Last.Text == Text && Now - Last.Time < RemoteDedupeSeconds)
		{
			return true;
		}
		Last.Text = Text;
		Last.Time = Now;
		return false;
	}
}

void MMONotify::ToOwner(const AActor* OwnerActor, const FString& Text, const FColor& Color, float Duration, EMMONotifyKind Kind)
{
	const APawn* Pawn = Cast<APawn>(OwnerActor);
	if (!Pawn || Text.IsEmpty())
	{
		return;
	}
	if (Pawn->IsLocallyControlled())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Text);
		}
		return;
	}
	// Sunucuda uzak oyuncunun pawn'i. Istemci makinesindeki baska oyuncularin pawn'larinda
	// kontrolcu yoktur, bu durumda hicbir sey yapilmaz.
	if (!Pawn->HasAuthority())
	{
		return;
	}
	AMMOPlayerController* PC = Cast<AMMOPlayerController>(Pawn->GetController());
	if (!PC || IsRemoteDuplicate(Pawn, Text))
	{
		return;
	}
	// Gecici ipucu: karakter ustunde balon (Unreliable, kalici satir birakmaz). Renk/sure uzakta kullanilmaz.
	if (Kind == EMMONotifyKind::Hint)
	{
		if (AMMOCharacter* Char = const_cast<AMMOCharacter*>(Cast<AMMOCharacter>(Pawn)))
		{
			Char->ClientShowHint(Text);
			return;
		}
	}
	// Sonuc satiri: sahibine sistem satiri (kanal 2).
	PC->ClientReceiveChat(FString(), Text, 2);
}
