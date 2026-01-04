// SpellChannelTypes.h
// Developer: Marcus Daley
// Date: December 21, 2025
// Purpose: Defines spell channel types used throughout WizardJam
// Usage: Include this header in any file that needs ESpellChannel (GameMode, SpellCollectible, ElementalWall, etc.)

#pragma once

#include "CoreMinimal.h"
#include "SpellChannelTypes.generated.h"

/**
 * ESpellChannel
 * Represents the four elemental spell types in WizardJam.
 * Used for:
 *   - SpellCollectible pickup identification
 *   - ElementalWall matching requirements
 *   - Player spell inventory tracking
 *   - Projectile element typing
 *   - Arena zone gating
 */
UENUM(BlueprintType)
enum class ESpellChannel : uint8
{
    None        UMETA(DisplayName = "None"),
    Flame       UMETA(DisplayName = "Flame"),
    Ice         UMETA(DisplayName = "Ice"),
    Lightning   UMETA(DisplayName = "Lightning"),
    Arcane      UMETA(DisplayName = "Arcane")
};