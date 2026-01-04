// TeleportInterface.cpp
// Implementation of default teleport interface methods
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam

#include "Code/Utility/TeleportInterface.h"

bool ITeleportInterface::CanTeleport(FName Channel)
{
    // Default: allow all teleports
    // Override in implementing classes for channel restrictions
    return true;
}

void ITeleportInterface::OnTeleportExecuted(const FVector& TargetLocation, const FRotator& TargetRotation)
{
    // Default: no-op
    // Override for custom teleport effects
}