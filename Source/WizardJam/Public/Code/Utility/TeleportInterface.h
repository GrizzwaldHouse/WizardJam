// TeleportInterface.h
// Interface for actors that can be teleported between locations
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam
//
// PURPOSE:
// Defines the contract for teleportable actors. Any actor implementing this
// interface can be moved by TeleportPoint actors based on channel permissions.
//
// USAGE:
// 1. Inherit from ITeleportInterface in your actor class
// 2. Implement CanTeleport() to control which channels are allowed
// 3. Implement OnTeleportExecuted() for custom teleport effects
// 4. Bind to GetOnTeleportStart()/GetOnTeleportComplete() delegates for events

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TeleportInterface.generated.h"

class AActor;

// Delegate fired when teleportation begins
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnTeleportStart,
    AActor*, TeleportingActor,
    FVector, TargetLocation
);

// Delegate fired when teleportation completes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnTeleportComplete,
    AActor*, TeleportedActor,
    bool, bSuccess
);

UINTERFACE(MinimalAPI, BlueprintType)
class UTeleportInterface : public UInterface
{
    GENERATED_BODY()
};

class WIZARDJAM_API ITeleportInterface
{
    GENERATED_BODY()

public:
    // Check if this actor can teleport on the specified channel
    // Override to implement channel-based restrictions
    virtual bool CanTeleport(FName Channel);

    // Called when teleportation is executed
    // Override to add custom effects (screen fade, sounds, particles)
    virtual void OnTeleportExecuted(const FVector& TargetLocation, const FRotator& TargetRotation);

    // Get the teleport start delegate for this actor
    virtual FOnTeleportStart& GetOnTeleportStart() = 0;

    // Get the teleport complete delegate for this actor
    virtual FOnTeleportComplete& GetOnTeleportComplete() = 0;
};