// Copyright Epic Games, Inc. All Rights Reserved.

#include "Security/PacketValidator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

FPacketValidator::FPacketValidator()
{
}

FPacketValidator::~FPacketValidator()
{
}

bool FPacketValidator::ValidateMovement(APlayerController* PlayerController, const FVector& NewLocation, float DeltaTime)
{
	if (!PlayerController || DeltaTime <= 0.0f)
	{
		return false;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	const FVector OldLocation = Pawn->GetActorLocation();

	// Validate movement speed
	if (!ValidateMovementSpeed(OldLocation, NewLocation, DeltaTime))
	{
		RecordValidationFailure(PlayerController, TEXT("Excessive movement speed detected"));
		return false;
	}

	// Validate no teleportation through walls
	if (!ValidateMovementCollision(PlayerController, OldLocation, NewLocation))
	{
		RecordValidationFailure(PlayerController, TEXT("Teleportation through collision detected"));
		return false;
	}

	// Validate teleport distance
	const float Distance = FVector::Dist(OldLocation, NewLocation);
	if (Distance > MaxTeleportDistance)
	{
		RecordValidationFailure(PlayerController, FString::Printf(TEXT("Excessive teleport distance: %.0f > %.0f"), Distance, MaxTeleportDistance));
		return false;
	}

	return true;
}

bool FPacketValidator::ValidateRPC(const FName& RPCName, const TArray<uint8>& Payload, APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	// Validate payload size
	if (!ValidateRPCPayloadSize(RPCName, Payload.Num()))
	{
		RecordValidationFailure(PlayerController, FString::Printf(TEXT("RPC payload too large: %s (%d bytes)"), *RPCName.ToString(), Payload.Num()));
		return false;
	}

	// Validate RPC frequency
	if (!ValidateRPCFrequency(PlayerController, RPCName))
	{
		RecordValidationFailure(PlayerController, FString::Printf(TEXT("RPC frequency exceeded: %s"), *RPCName.ToString()));
		return false;
	}

	// Specific RPC validations
	if (RPCName == TEXT("ServerAttackTarget"))
	{
		// Attack RPCs should have minimal payload
		if (Payload.Num() > 64)
		{
			RecordValidationFailure(PlayerController, TEXT("Attack RPC payload too large"));
			return false;
		}
	}
	else if (RPCName == TEXT("ServerUseConsumable"))
	{
		// Consumable RPCs should have item ID payload
		if (Payload.Num() < 8 || Payload.Num() > 128)
		{
			RecordValidationFailure(PlayerController, TEXT("Consumable RPC payload size invalid"));
			return false;
		}
	}
	else if (RPCName == TEXT("ServerTradeConfirm"))
	{
		// Trade confirm should have small payload
		if (Payload.Num() > 256)
		{
			RecordValidationFailure(PlayerController, TEXT("Trade confirm payload too large"));
			return false;
		}
	}

	return true;
}

bool FPacketValidator::ValidatePacketIntegrity(const TArray<uint8>& PacketData, uint32 ExpectedSequence)
{
	// Basic integrity checks
	if (PacketData.Num() == 0)
	{
		return false;
	}

	// Check minimum packet size
	if (PacketData.Num() < 4)
	{
		return false;
	}

	// Check maximum packet size (prevent buffer overflow attacks)
	if (PacketData.Num() > 65536)
	{
		return false;
	}

	// TODO: Implement actual checksum validation
	// TODO: Implement sequence number validation
	// TODO: Implement encryption validation

	return true;
}

void FPacketValidator::RecordValidationFailure(APlayerController* PlayerController, const FString& Reason)
{
	if (!PlayerController)
	{
		return;
	}

	int32& FailureCount = ValidationFailures.FindOrAdd(PlayerController);
	FailureCount++;

	UE_LOG(LogTemp, Warning, TEXT("[AntiCheat] Validation failure for player %s: %s (Count: %d)"),
		*PlayerController->GetName(), *Reason, FailureCount);

	// Log to analytics/security system
	if (FailureCount >= MaxValidationFailures)
	{
		UE_LOG(LogTemp, Error, TEXT("[AntiCheat] Player %s exceeded max validation failures (%d). Action required."),
			*PlayerController->GetName(), MaxValidationFailures);
	}
}

int32 FPacketValidator::GetValidationFailureCount(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return 0;
	}

	const int32* Count = ValidationFailures.Find(PlayerController);
	return Count ? *Count : 0;
}

void FPacketValidator::ResetValidationFailures(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	ValidationFailures.Remove(PlayerController);
}

bool FPacketValidator::ShouldActionPlayer(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	const int32* Count = ValidationFailures.Find(PlayerController);
	return Count && *Count >= MaxValidationFailures;
}

bool FPacketValidator::ValidateMovementSpeed(const FVector& OldLocation, const FVector& NewLocation, float DeltaTime) const
{
	const float Distance = FVector::Dist(OldLocation, NewLocation);
	const float Speed = Distance / DeltaTime;

	// Allow some margin for network latency
	const float MaxAllowedSpeed = MaxMovementSpeed * 1.2f;

	return Speed <= MaxAllowedSpeed;
}

bool FPacketValidator::ValidateMovementCollision(APlayerController* PlayerController, const FVector& OldLocation, const FVector& NewLocation) const
{
	if (!PlayerController)
	{
		return false;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn)
	{
		return false;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return true; // Can't validate, allow
	}

	// Trace from old to new location to check for collision
	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PacketValidatorMovement), false, Pawn);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bReturnFaceIndex = false;

	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		OldLocation,
		NewLocation,
		ECC_Visibility,
		QueryParams
	);

	// If we hit something solid, movement might be invalid
	// Allow small penetrations for network latency
	if (bHit && HitResult.bBlockingHit)
	{
		const float PenetrationDepth = FVector::Dist(OldLocation, HitResult.Location);
		const float TotalDistance = FVector::Dist(OldLocation, NewLocation);

		// If we hit very early in the movement, it's suspicious
		if (PenetrationDepth < TotalDistance * 0.1f)
		{
			return false;
		}
	}

	return true;
}

bool FPacketValidator::ValidateRPCPayloadSize(const FName& RPCName, int32 PayloadSize) const
{
	// Define max payload sizes per RPC type
	static const TMap<FName, int32> MaxPayloadSizes = {
		{ TEXT("ServerAttackTarget"), 64 },
		{ TEXT("ServerUseConsumable"), 128 },
		{ TEXT("ServerUseHealthPotion"), 32 },
		{ TEXT("ServerEquipByInstanceId"), 64 },
		{ TEXT("ServerUpgradeByInstanceId"), 64 },
		{ TEXT("ServerTradeConfirm"), 256 },
		{ TEXT("ServerTradeGold"), 64 },
		{ TEXT("ServerAddItem"), 128 },
		{ TEXT("ServerRemoveItem"), 64 },
		{ TEXT("ServerSetGold"), 64 },
		{ TEXT("ServerBuyEquipment"), 64 },
		{ TEXT("ServerSellByInstanceId"), 64 },
		{ TEXT("ServerDepositEquipment"), 64 },
		{ TEXT("ServerWithdrawEquipment"), 64 },
		{ TEXT("ServerAcceptQuest"), 64 },
		{ TEXT("ServerClaimReward"), 64 },
		{ TEXT("ServerSpendStat"), 32 },
		{ TEXT("ServerSetPvP"), 32 },
	};

	const int32* MaxSize = MaxPayloadSizes.Find(RPCName);
	if (MaxSize)
	{
		return PayloadSize <= *MaxSize;
	}

	// Default max payload size for unknown RPCs
	return PayloadSize <= 1024;
}

bool FPacketValidator::ValidateRPCFrequency(APlayerController* PlayerController, const FName& RPCName) const
{
	// TODO: Implement per-RPC rate limiting
	// Track RPC calls per second per player per RPC type
	// For now, allow all
	return true;
}