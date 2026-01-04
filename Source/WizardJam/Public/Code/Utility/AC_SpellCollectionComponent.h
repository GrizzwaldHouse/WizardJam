// ============================================================================
// AC_SpellCollectionComponent.h
// Developer: Marcus Daley
// Date: December 25, 2025
// ============================================================================
// Purpose:
// Actor Component that provides spell collection functionality to ANY actor.
// This follows the "composition over inheritance" principle instead of
// baking spell collection into a base class, we create a reusable component
// that can be attached to any actor that needs it.
//
// Why Component Instead of BaseCharacter:
// 1. NO CAST FAILURES - Component lookup works regardless of class hierarchy
// 2. TRULY MODULAR - Add to Player, Companion, Enemy, even non-characters
// 3. EASY TO DISABLE - Just remove component or set bEnabled = false
// 4. REUSABLE - Same component works in any project
// 5. TESTABLE - Can unit test component in isolation
//
// Architecture:
// - SpellCollectible looks for ISpellCollector interface on overlapping actor
// - ISpellCollector::GetSpellCollectionComponent() returns this component
// - Component handles all spell tracking, validation, and events
// - Actor class stays clean - just implements interface and holds component
//
// Usage:
// 1. Add this component to your actor (C++ or Blueprint)
// 2. Implement ISpellCollector interface on actor
// 3. Return this component from GetSpellCollectionComponent()
// 4. Configure in Details panel: enable collection, set channels
//
// Example Blueprint Setup:
// 1. Open your Blueprint
// 2. Add Component > search "Spell Collection"
// 3. Configure in Details panel
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_SpellCollectionComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpellCollection, Log, All);

// ============================================================================
// DELEGATES
// Observer pattern - other systems bind to these for updates
// ============================================================================

// Broadcast when a spell is successfully added to collection
// Parameters: SpellType (FName), TotalCount (int32)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSpellAdded,
    FName, SpellType,
    int32, TotalSpellCount
);

// Broadcast when a spell is removed from collection
// Parameters: SpellType (FName), RemainingCount (int32)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSpellRemoved,
    FName, SpellType,
    int32, RemainingSpellCount
);

// Broadcast when all spells are cleared
// Parameters: PreviousCount (int32)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnAllSpellsCleared,
    int32, PreviousCount
);

// Broadcast when a channel is added to this component
// Owner can bind to this to sync with teleport system or other systems
// Parameters: Channel (FName)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnChannelAdded,
    FName, Channel
);

// Broadcast when a channel is removed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnChannelRemoved,
    FName, Channel
);

// Static delegate - GameMode binds once, receives events from ALL components
// This enables global spell tracking without direct references
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FOnAnySpellCollectedGlobal,
    FName,      // SpellType
    AActor*,    // Owning actor
    UAC_SpellCollectionComponent*  // This component
);

// ============================================================================
// SPELL COLLECTION COMPONENT
// ============================================================================

UCLASS(ClassGroup = (WizardJam), meta = (BlueprintSpawnableComponent),
    DisplayName = "Spell Collection Component")
    class WIZARDJAM_API UAC_SpellCollectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAC_SpellCollectionComponent();

    // ========================================================================
    // STATIC DELEGATE
    // GameMode binds to this ONCE and receives events from ALL components
    // ========================================================================

    static FOnAnySpellCollectedGlobal OnAnySpellCollected;

    // ========================================================================
    // INSTANCE DELEGATES
    // Per-component events - bind for actor-specific reactions
    // ========================================================================

    // Fires when this component's owner collects a new spell
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnSpellAdded OnSpellAdded;

    // Fires when a spell is removed from this component
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnSpellRemoved OnSpellRemoved;

    // Fires when ClearAllSpells() is called
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnAllSpellsCleared OnAllSpellsCleared;

    // ========================================================================
    // CHANNEL DELEGATES (Hybrid Bridge for Teleport Sync)
    // These enable DECOUPLED sync between spell channels and other systems
    //
    // How the hybrid bridge works:
    // 1. Component broadcasts OnChannelAdded when a channel is unlocked
    // 2. Owner (BaseCharacter/BasePlayer) binds to this delegate
    // 3. Owner DECIDES whether to also add to its teleport channels
    // 4. Component has ZERO knowledge of teleport system
    //
    // This is better than direct coupling because:
    // - Player can sync channels to teleporters (both systems unlock)
    // - Companion can ignore teleport sync (only affects spell gates)
    // - Enemy can do something completely different (custom logic)
    // - Future systems can also bind without changing component code
    // ========================================================================

    // Fires when a channel is added to this component
    // Bind in your character to sync with teleport system:
    // SpellComponent->OnChannelAdded.AddDynamic(this, &AMyCharacter::HandleChannelAdded);
    UPROPERTY(BlueprintAssignable, Category = "Spells|Channel Events")
    FOnChannelAdded OnChannelAdded;

    // Fires when a channel is removed from this component
    UPROPERTY(BlueprintAssignable, Category = "Spells|Channel Events")
    FOnChannelRemoved OnChannelRemoved;

    // ========================================================================
    // SPELL MANAGEMENT FUNCTIONS
    // These are the primary API for spell collection
    // ========================================================================

    // Add a spell to the collection
    // Returns true if spell was newly added, false if already had it
    // Broadcasts OnSpellAdded on success
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool AddSpell(FName SpellType);

    // Check if a specific spell has been collected
    // Use for ability checks, UI display, gate validation
    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasSpell(FName SpellType) const;

    // Remove a spell from collection
    // Returns true if spell was removed, false if didn't have it
    // Use for: spell theft, curses, death penalties
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool RemoveSpell(FName SpellType);

    // Get all collected spell types as array
    // Useful for HUD iteration, save/load, spell wheel
    UFUNCTION(BlueprintPure, Category = "Spells")
    TArray<FName> GetAllSpells() const;

    // Get count of unique spells collected
    UFUNCTION(BlueprintPure, Category = "Spells")
    int32 GetSpellCount() const;

    // Remove all spells (game reset, respawn penalty)
    // Broadcasts OnAllSpellsCleared
    UFUNCTION(BlueprintCallable, Category = "Spells")
    void ClearAllSpells();

    // ========================================================================
    // CHANNEL SYSTEM
    // Channels gate spell requirements and unlock progression
    // Same system as teleporters for unified progression
    // ========================================================================

    // Add a channel to this component's owner
    UFUNCTION(BlueprintCallable, Category = "Spells|Channels")
    void AddChannel(FName Channel);

    // Check if owner has a specific channel
    UFUNCTION(BlueprintPure, Category = "Spells|Channels")
    bool HasChannel(FName Channel) const;

    // Remove a channel
    UFUNCTION(BlueprintCallable, Category = "Spells|Channels")
    void RemoveChannel(FName Channel);

    // Get all channels
    UFUNCTION(BlueprintPure, Category = "Spells|Channels")
    TArray<FName> GetAllChannels() const;

    // Clear all channels
    UFUNCTION(BlueprintCallable, Category = "Spells|Channels")
    void ClearAllChannels();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    // Is spell collection enabled for this component?
    UFUNCTION(BlueprintPure, Category = "Spells|Configuration")
    bool IsCollectionEnabled() const;

    // Enable or disable spell collection at runtime
    UFUNCTION(BlueprintCallable, Category = "Spells|Configuration")
    void SetCollectionEnabled(bool bEnabled);

    // ========================================================================
    // DEBUG
    // ========================================================================

    // Print all spells to Output Log
    UFUNCTION(BlueprintCallable, Category = "Spells|Debug")
    void DebugPrintSpells() const;

    // Print all channels to Output Log
    UFUNCTION(BlueprintCallable, Category = "Spells|Debug")
    void DebugPrintChannels() const;

protected:
    virtual void BeginPlay() override;

    // ========================================================================
    // CONFIGURATION PROPERTIES
    // Designer sets these in Blueprint or Level Editor
    // ========================================================================

    // Master switch - is spell collection enabled?
    // If false, AddSpell() will fail and pickups will be denied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spells|Configuration",
        meta = (DisplayName = "Collection Enabled"))
    bool bCollectionEnabled;

    // Starting channels - granted at BeginPlay
    // Use for: tutorial characters that start with access, debug testing
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spells|Configuration",
        meta = (DisplayName = "Starting Channels"))
    TArray<FName> StartingChannels;

    // Starting spells - granted at BeginPlay
    // Use for: characters that begin with abilities, debug testing
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spells|Configuration",
        meta = (DisplayName = "Starting Spells"))
    TArray<FName> StartingSpells;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    // All collected spell types (TSet prevents duplicates)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spells|Runtime")
    TSet<FName> CollectedSpells;

    // All unlocked channels (for requirements and teleporters)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spells|Runtime")
    TSet<FName> UnlockedChannels;
};