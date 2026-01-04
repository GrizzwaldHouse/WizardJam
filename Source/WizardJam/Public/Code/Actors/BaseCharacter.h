// BaseCharacter.h
// Developer: Marcus Daley
// Date: December 26, 2025
// Purpose: Base character class providing shared functionality for all character types
//
// Architecture:
// This class serves as the foundation for Player, Enemy, and Companion characters.
// It provides modular systems that any character type can use:
// - Teleport Channel System: Controls which teleporters a character can use
// - Spell Collection System: Tracks which spells a character has collected
// - Team Interface: Identifies friend vs foe for AI perception and combat
//
// Spell Collection Design Philosophy:
// The spell collection is implemented at the BaseCharacter level (not BasePlayer)
// so that enemies and companions can also collect and use spells. This enables:
// - Enemy wizards that have their own spell repertoires
// - Companion that can pick up spells for the player
// - Future multiplayer where each player tracks their own spells
//
// Designer Configuration:
// - bCanCollectSpells: Enable or disable spell collection for this character type
// - AllowedTeleportChannels: Which teleport channels this character can use
// - TeamID: Determines faction (0=Player, 1=Enemy, 2=Companion typically)
//
// Usage Example:
// 1. Create BP child class (BP_WizardEnemy)
// 2. Set bCanCollectSpells = true if enemies can collect spells
// 3. SpellCollectible will check this before allowing pickup
// 4. Character's CollectedSpells set tracks what they've collected

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Code/Actors/InputCharacter.h"
#include "Code/Utility/TeleportInterface.h"
#include "GenericTeamAgentInterface.h"
#include "BaseCharacter.generated.h"

// Forward declarations
class UAC_HealthComponent;
class UAC_StaminaComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogBaseCharacter, Log, All);

// ============================================================================
// SPELL COLLECTION DELEGATES
// These follow the Observer pattern - any system can listen for spell events
// ============================================================================

// Broadcast when THIS character collects a spell
// Use for: Character-specific reactions, inventory updates, ability unlocks
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCharacterSpellCollected,
    FName, SpellTypeName,
    int32, TotalSpellsCollected
);

// Broadcast when a spell is removed from this character
// Use for: Spell theft mechanics, curse effects, debugging
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCharacterSpellRemoved,
    FName, SpellTypeName,
    int32, TotalSpellsRemaining
);

// ============================================================================
// CHARACTER CLASS
// ============================================================================

UCLASS()
class WIZARDJAM_API ABaseCharacter : public AInputCharacter, public ITeleportInterface,
    public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    ABaseCharacter();

    // ========================================================================
    // COMPONENT ACCESSORS
    // ========================================================================

    UFUNCTION(BlueprintPure, Category = "Health")
    UAC_HealthComponent* GetHealthComponent() const;

    UFUNCTION(BlueprintPure, Category = "Stamina")
    UAC_StaminaComponent* GetStaminaComponent() const;

    // ========================================================================
    // TELEPORT INTERFACE IMPLEMENTATION
    // ========================================================================

    virtual bool CanTeleport(FName Channel) override;
    virtual void OnTeleportExecuted(const FVector& TargetLocation, const FRotator& TargetRotation) override;
    virtual FOnTeleportStart& GetOnTeleportStart() override;
    virtual FOnTeleportComplete& GetOnTeleportComplete() override;

    UPROPERTY(BlueprintAssignable, Category = "Teleport|Events")
    FOnTeleportStart OnTeleportStartDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Teleport|Events")
    FOnTeleportComplete OnTeleportCompleteDelegate;

    // ========================================================================
    // TELEPORT CHANNEL MANAGEMENT
    // Channels gate which teleporters this character can use
    // Same system used by spells for unlock requirements
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Teleport|Channels")
    void AddTeleportChannel(const FName& Channel);

    UFUNCTION(BlueprintCallable, Category = "Teleport|Channels")
    void RemoveTeleportChannel(const FName& Channel);

    UFUNCTION(BlueprintPure, Category = "Teleport|Channels")
    bool HasTeleportChannel(const FName& Channel) const;

    UFUNCTION(BlueprintPure, Category = "Teleport|Channels")
    const TArray<FName>& GetTeleportChannels() const;

    UFUNCTION(BlueprintCallable, Category = "Teleport|Channels")
    void ClearTeleportChannels();

    void SerializeTeleportChannelsForSave(TArray<FString>& OutChannelNames) const;
    void DeserializeTeleportChannelsFromSave(const TArray<FString>& InChannelNames);

    // ========================================================================
    // SPELL COLLECTION SYSTEM
    // Tracks which spells THIS character has collected
    // Modular design - works for Player, Enemy, Companion
    // ========================================================================

    // Add a spell to this character's collection
    // Returns true if spell was newly added, false if already had it
    // Broadcasts OnSpellCollected delegate on success
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool AddSpell(FName SpellTypeName);

    // Remove a spell from this character's collection
    // Returns true if spell was removed, false if didn't have it
    // Broadcasts OnSpellRemoved delegate on success
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool RemoveSpell(FName SpellTypeName);

    // Check if this character has a specific spell
    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasSpell(FName SpellTypeName) const;

    // Get all spells this character has collected
    // Returns TArray for Blueprint iteration (TSet doesn't iterate well in BP)
    UFUNCTION(BlueprintPure, Category = "Spells")
    TArray<FName> GetCollectedSpells() const;

    // Get total number of unique spells collected
    UFUNCTION(BlueprintPure, Category = "Spells")
    int32 GetSpellCount() const;

    // Clear all collected spells (use with caution - for respawn or curse effects)
    UFUNCTION(BlueprintCallable, Category = "Spells")
    void ClearAllSpells();

    // Check if this character is allowed to collect spells at all
    // Designer sets this in Blueprint - enables spell collection per character type
    UFUNCTION(BlueprintPure, Category = "Spells|Configuration")
    bool CanCollectSpells() const;

    // ========================================================================
    // SPELL COLLECTION DELEGATES
    // Blueprint systems can bind to these for UI updates, ability unlocks, etc.
    // ========================================================================

    // Broadcast when this character collects a new spell
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnCharacterSpellCollected OnSpellCollected;

    // Broadcast when a spell is removed from this character
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnCharacterSpellRemoved OnSpellRemoved;

    // ========================================================================
    // TEAM INTERFACE (IGenericTeamAgentInterface)
    // Used by AI perception and friendly fire checks
    // ========================================================================

    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;

    // ========================================================================
    // DEBUG FUNCTIONS
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Teleport|Debug")
    void DebugPrintChannels() const;

    UFUNCTION(BlueprintCallable, Category = "Spells|Debug")
    void DebugPrintSpells() const;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
    virtual void BeginPlay() override;

    // ========================================================================
    // COMPONENTS
    // ========================================================================

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
    UAC_HealthComponent* HealthComponent;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
    UAC_StaminaComponent* StaminaComponent;

    // ========================================================================
    // TELEPORT CONFIGURATION
    // ========================================================================

    // Channels this character can teleport on (empty = all channels allowed)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
    TArray<FName> AllowedTeleportChannels;

    // ========================================================================
    // SPELL COLLECTION CONFIGURATION
    // ========================================================================

    // Enable or disable spell collection for this character type
    // Designer sets this to control which characters can pick up spells:
    // - Player: Usually true
    // - Enemy Wizard: Could be true (they collect spells from fallen players)
    // - Companion Dog: Could be true (fetches spells for player)
    // - Basic Enemy Grunt: Usually false (doesn't use magic)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spells|Configuration",
        meta = (DisplayName = "Can Collect Spells"))
    bool bCanCollectSpells;

    // Spells this character has collected (private - use accessor functions)
    // TSet ensures no duplicates and fast lookup
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spells|Runtime")
    TSet<FName> CollectedSpells;

    // ========================================================================
    // TEAM CONFIGURATION
    // ========================================================================

    // Team ID for AI perception and friendly fire
    // Common values:
    // 0 = Player team
    // 1 = Enemy team
    // 2 = Companion team (neutral to player, hostile to enemies)
    // 255 = No team (attacks everyone)
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Team")
    uint8 TeamID;
};