// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Anti-cheat packet validation middleware
 * Validates incoming packets for common cheat patterns
 */
class FPacketValidator
{
public:
	FPacketValidator();
	~FPacketValidator();

	/**
	 * Validates a movement packet for speed hacks, teleportation, etc.
	 * @param PlayerController The controller sending the movement
	 * @param NewLocation The new location being requested
	 * @param DeltaTime Time since last update
	 * @return true if movement is valid, false if suspicious
	 */
	bool ValidateMovement(class APlayerController* PlayerController, const FVector& NewLocation, float DeltaTime);

	/**
	 * Validates an RPC packet for common cheat patterns
	 * @param RPCName Name of the RPC being called
	 * @param Payload The RPC payload data
	 * @param PlayerController The controller sending the RPC
	 * @return true if RPC is valid, false if suspicious
	 */
	bool ValidateRPC(const FName& RPCName, const TArray<uint8>& Payload, class APlayerController* PlayerController);

	/**
	 * Validates packet integrity (checksum, sequence, encryption)
	 * @param PacketData The raw packet data
	 * @param ExpectedSequence Expected sequence number
	 * @return true if packet integrity is valid
	 */
	bool ValidatePacketIntegrity(const TArray<uint8>& PacketData, uint32 ExpectedSequence);

	/**
	 * Records a validation failure for a player
	 * @param PlayerController The player who failed validation
	 * @param Reason The reason for the failure
	 */
	void RecordValidationFailure(class APlayerController* PlayerController, const FString& Reason);

	/**
	 * Gets the validation failure count for a player
	 * @param PlayerController The player to check
	 * @return Number of validation failures
	 */
	int32 GetValidationFailureCount(class APlayerController* PlayerController) const;

	/**
	 * Resets validation failure count for a player (e.g., on reconnect)
	 * @param PlayerController The player to reset
	 */
	void ResetValidationFailures(class APlayerController* PlayerController);

	/**
	 * Checks if a player should be flagged/kicked based on validation failures
	 * @param PlayerController The player to check
	 * @return true if player should be actioned
	 */
	bool ShouldActionPlayer(class APlayerController* PlayerController) const;

private:
	/** Tracks validation failures per player */
	TMap<TWeakObjectPtr<class APlayerController>, int32> ValidationFailures;

	/** Maximum allowed validation failures before action */
	static constexpr int32 MaxValidationFailures = 10;

	/** Maximum movement speed (units per second) */
	static constexpr float MaxMovementSpeed = 2000.0f;

	/** Maximum teleport distance per frame */
	static constexpr float MaxTeleportDistance = 5000.0f;

	/** Validates movement speed is within reasonable bounds */
	bool ValidateMovementSpeed(const FVector& OldLocation, const FVector& NewLocation, float DeltaTime) const;

	/** Validates movement doesn't teleport through walls */
	bool ValidateMovementCollision(class APlayerController* PlayerController, const FVector& OldLocation, const FVector& NewLocation) const;

	/** Validates RPC payload size is reasonable */
	bool ValidateRPCPayloadSize(const FName& RPCName, int32 PayloadSize) const;

	/** Validates RPC frequency is not excessive */
	bool ValidateRPCFrequency(class APlayerController* PlayerController, const FName& RPCName) const;
};