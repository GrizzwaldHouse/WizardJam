// ============================================================================
// SpellSlotConfig.h
// Developer: Marcus Daley
// Date: December 28, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Designer-configurable spell slot data structure. This struct allows designers
// to add unlimited spell types without touching C++ code. Simply add new entries
// to the SpellSlotConfigs array in WBP_PlayerHUD and UI updates automatically.
//
// Designer Workflow:
// 1. Open WBP_PlayerHUD Blueprint
// 2. Find SpellSlotConfigs array in Details panel
// 3. Add new element with SpellTypeName, SlotIndex, and icon textures
// 4. Compile Blueprint - new spell slot appears automatically
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "SpellSlotConfig.generated.h"

// Forward declaration avoids pulling in heavy texture header
class UTexture2D;

// ============================================================================
// SPELL SLOT CONFIGURATION STRUCT
// Each entry defines one spell slot's visual properties and identification
// ============================================================================

USTRUCT(BlueprintType)
struct FSpellSlotConfig
{
    GENERATED_BODY()

    // ========================================================================
    // SPELL IDENTIFICATION
    // ========================================================================

    // How matching works:
    // 1. Player touches BP_SpellCollectible_Flame
    // 2. Collectible broadcasts SpellTypeName = "Flame"
    // 3. SpellCollectionComponent adds "Flame" to TSet
    // 4. Component broadcasts OnSpellAdded("Flame", TotalCount)
    // 5. HUD searches SpellSlotConfigs for entry with SpellTypeName == "Flame"
    // 6. HUD swaps texture from LockedIcon to UnlockedIcon
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Identity")
    FName SpellTypeName;

    // ========================================================================
    // ICON TEXTURES
    // ========================================================================

    // Icon displayed when spell is UNLOCKED (player has collected it)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Icons")
    UTexture2D* UnlockedIcon;

    // Icon displayed when spell is LOCKED (not yet collected)
    // This should be a dimmed, grayed, or silhouette version of the icon
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Icons")
    UTexture2D* LockedIcon;

    // ========================================================================
    // COLOR TINTING (Used if icons are nullptr or for additional effects)
    // ========================================================================

    // Color tint when spell is UNLOCKED - multiplied with icon texture
    // Default white means no tinting (show icon as-is)
    // Can be used for:
    //   - Team-based coloring (multiply icon by team color)
    //   - Highlight effects (slight yellow tint when hovering)
    //   - Element matching (ensure icon matches in-world collectible color)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Colors")
    FLinearColor UnlockedColor;

    // Color tint when spell is LOCKED - multiplied with LockedIcon
    // Default dark gray dims the icon further
    // If LockedIcon is nullptr, this tints UnlockedIcon instead
    // Useful for:
    //   - Making all locked spells uniformly dark
    //   - Per-element locked colors (dark blue for ice, dark red for fire)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Colors")
    FLinearColor LockedColor;

    // ========================================================================
    // UI LAYOUT CONTROL
    // ========================================================================

    // Display position in spell bar (0 = leftmost, increases rightward)
    // Allows designer to control display order independently of array order
    // Example: 0=Flame, 1=Ice, 2=Lightning, 3=Arcane
    //
    // Benefits of explicit indexing:
    // - Reorder spells without moving array elements
    // - Leave gaps for expansion (0, 1, 2, 5, 6 = slots 3, 4 reserved)
    // - Clear visual of which slot gets which spell
    //
    // The HUD uses this index to find SpellSlot_X widget (SpellSlot_0, SpellSlot_1, etc.)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spell Layout")
    int32 SlotIndex;

    // ========================================================================
    // CONSTRUCTOR - Provides sensible defaults
    // Uses initialization list per Nick Penney's coding standards
    // ========================================================================

    FSpellSlotConfig()
        : SpellTypeName(NAME_None)
        , UnlockedIcon(nullptr)
        , LockedIcon(nullptr)
        , UnlockedColor(FLinearColor::White)
        , LockedColor(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f))
        , SlotIndex(0)
    {
        // Empty body - all initialization via list above
        // This runs when designer clicks "+" to add array element
    }

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================

    // Returns true if this config has been properly configured by designer
    // Used by HUD to skip unconfigured entries in the array
    bool IsValid() const
    {
        return SpellTypeName != NAME_None;
    }

    // Get the appropriate icon based on unlock state
    // Falls back to UnlockedIcon if LockedIcon is not set
    // Returns nullptr if no icons are configured
    UTexture2D* GetIcon(bool bIsUnlocked) const
    {
        if (bIsUnlocked)
        {
            return UnlockedIcon;
        }

        // Use LockedIcon if available, otherwise fall back to UnlockedIcon
        // The color tinting will differentiate locked appearance
        return LockedIcon ? LockedIcon : UnlockedIcon;
    }

    // Get the appropriate color tint based on unlock state
    FLinearColor GetColor(bool bIsUnlocked) const
    {
        return bIsUnlocked ? UnlockedColor : LockedColor;
    }
};