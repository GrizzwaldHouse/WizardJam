// ============================================================================
// CollectiblePickup.h
// Developer: Marcus Daley
// Date: December 31, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Base class for collectible items that grant spell channels when picked up.
// Uses component-based architecture to work with any actor that has
// AC_SpellCollectionComponent (player, companion, etc.).
//
// Designer Usage:
// 1. Create Blueprint child (e.g., BP_SpellCollectible_Flame)
// 2. Set ItemName to descriptive name (e.g., "Fire Spell Crystal")
// 3. Add spell channels to GrantsSpellChannels array (e.g., "Flame")
// 4. Configure which actors can collect (bPlayerCanCollect, bEnemyCanCollect, etc.)
// 5. Place in level - automatically grants channels on overlap
//
// Delegate Usage:
// GameMode or other systems can subscribe to OnPickedUp delegate to track
// collectible acquisition and update UI/progression systems.
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Code/Actors/BasePickup.h"
#include "CollectiblePickup.generated.h"

class AActor;

// Delegate broadcast when collectible is picked up
// Listeners can track what was collected and by whom
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCollectiblePickedUp,
    AActor*, PickingActor,
    ACollectiblePickup*, PickedItem
);

// Base class for collectible items that grant spell channels
// Child classes can specialize for specific item types (health, spells, power-ups)
UCLASS()
class WIZARDJAM_API ACollectiblePickup : public ABasePickup
{
    GENERATED_BODY()

public:
    ACollectiblePickup();

    // Broadcast when this collectible is picked up
    // GameMode subscribes to update UI and track progression
    UPROPERTY(BlueprintAssignable, Category = "Collectible|Events")
    FOnCollectiblePickedUp OnPickedUp;

    // Get the item's configured name
    UFUNCTION(BlueprintPure, Category = "Collectible")
    FName GetItemName() const { return ItemName; }

    // Get list of spell channels this item grants
    UFUNCTION(BlueprintPure, Category = "Collectible")
    TArray<FName> GetGrantedChannels() const { return GrantsSpellChannels; }

protected:
    virtual bool CanBePickedUp(AActor* OtherActor) const override;
    virtual void HandlePickup(AActor* OtherActor) override;
    virtual void PostPickup() override;

    // Descriptive name for this collectible (shown in logs and UI)
    // Designer sets this to identify the item (e.g., "Fire Spell", "Ice Crystal")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collectible|Identity")
    FName ItemName;

    // Spell channels granted when collected
    // Designer adds channel names that unlock spell types
    // Example: Add "Flame" to unlock fire spells, "Ice" for ice spells
    // Multiple channels can be granted by one item
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Collectible|Spell System",
        meta = (DisplayName = "Grants Spell Channels"))
    TArray<FName> GrantsSpellChannels;

    // Show detailed logging when channels are granted (useful for debugging)
    UPROPERTY(EditDefaultsOnly, Category = "Collectible|Debug")
    bool bShowChannelGrantLog;

    // Can the player character collect this item?
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collectible|Permissions")
    bool bPlayerCanCollect;

    // Can enemy characters collect this item? (usually false for spell collectibles)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collectible|Permissions")
    bool bEnemyCanCollect;

    // Can companion characters collect this item? (e.g., dog companion)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collectible|Permissions")
    bool bCompanionCanCollect;

private:
    // Grant spell channels to collecting actor via AC_SpellCollectionComponent
    // Only works if actor has the component - silently fails otherwise
    void GrantChannels(AActor* OtherActor);

    // Check if actor has permission to collect based on team ID
    // Uses IGenericTeamAgentInterface for team identification
    bool HasCollectionPermission(AActor* OtherActor) const;
};