// ============================================================================
// BasePlayer.h
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Player-controlled wizard character with integrated spell system, combat,
// and spell switching functionality. Inherits from BaseCharacter.
// Implements ISpellCollector interface for spell pickup participation.
//
// Key Features:
// - ISpellCollector interface enables spell pickup via interface (no cast)
// - SpellCollectionComponent tracks collected spells and channels
// - AimComponent handles raycast aiming and target classification
// - CombatComponent handles projectile spawning and trajectory correction
// - Spell switching via mouse wheel or number keys
// - Observer pattern delegates for HUD updates
//
// Spell Switching:
// - Mouse wheel cycles through unlocked spells
// - Number keys 1-4 select specific spell slots
// - OnEquippedSpellChanged delegate notifies HUD of changes
// - Only unlocked spells can be equipped
//
// Designer Usage:
// 1. Create BP_WizardPlayer Blueprint child
// 2. Configure SpellCollectionComponent for starting spells
// 3. Configure CombatComponent's ProjectileClassMap
// 4. Create Input Actions: IA_Fire, IA_CycleSpell, IA_SelectSpell_1 to 4
// 5. Assign all Input Actions in Blueprint Details panel
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Code/Actors/BaseCharacter.h"
#include "Code/Utility/ISpellCollector.h"
#include "BasePlayer.generated.h"

class UAC_SpellCollectionComponent;
class UAC_AimComponent;
class UAC_CombatComponent;
class ABaseProjectile;

// Forward declare enums from other headers
enum class EAimTraceResult : uint8;
enum class EFireBlockedReason : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogBasePlayer, Log, All);

// ============================================================================
// DELEGATES
// ============================================================================

// Broadcast when player collects a new spell type
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnPlayerSpellCollected,
    FName, SpellType
);

// Broadcast when equipped spell changes (for HUD indicator)
// Parameters: NewSpellType, SlotIndex (0-3 for UI positioning)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnEquippedSpellChanged,
    FName, NewSpellType,
    int32, SlotIndex
);

// ============================================================================
// BASEPLAYER CLASS
// ============================================================================

UCLASS()
class WIZARDJAM_API ABasePlayer : public ABaseCharacter, public ISpellCollector
{
    GENERATED_BODY()

public:
    ABasePlayer();

    // ========================================================================
    // ISpellCollector Interface
    // ========================================================================

    virtual UAC_SpellCollectionComponent* GetSpellCollectionComponent_Implementation() const override;
    virtual int32 GetCollectorTeamID_Implementation() const override;
    virtual void OnSpellCollected_Implementation(FName SpellType) override;
    virtual void OnSpellCollectionDenied_Implementation(FName SpellType, const FString& Reason) override;

    // ========================================================================
    // Component Accessors
    // ========================================================================

    UFUNCTION(BlueprintPure, Category = "Spells")
    UAC_SpellCollectionComponent* GetSpellCollection() const { return SpellCollectionComponent; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UAC_AimComponent* GetAimComponent() const { return AimComponent; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    UAC_CombatComponent* GetCombatComponent() const { return CombatComponent; }

    // ========================================================================
    // Spell Query Functions
    // ========================================================================

    // Get currently equipped spell type
    UFUNCTION(BlueprintPure, Category = "Spells")
    FName GetEquippedSpellType() const { return EquippedSpellType; }

    // Get index of equipped spell in the spell order array
    UFUNCTION(BlueprintPure, Category = "Spells")
    int32 GetEquippedSpellIndex() const;

    // Check if a specific spell type is unlocked
    bool HasSpell(FName SpellType) const;

    // ========================================================================
    // Spell Control Functions
    // ========================================================================

    // Set equipped spell by type name
    // Returns true if spell was equipped, false if not unlocked
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool SetEquippedSpell(FName SpellType);

    // Set equipped spell by index in SpellOrder array
    // Returns true if valid index and spell is unlocked
    UFUNCTION(BlueprintCallable, Category = "Spells")
    bool SetEquippedSpellByIndex(int32 SlotIndex);

    // Cycle to next unlocked spell
    UFUNCTION(BlueprintCallable, Category = "Spells")
    void CycleToNextSpell();

    // Cycle to previous unlocked spell
    UFUNCTION(BlueprintCallable, Category = "Spells")
    void CycleToPreviousSpell();

    // ========================================================================
    // Events
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnPlayerSpellCollected OnPlayerSpellCollected;

    // Broadcast when equipped spell changes - HUD binds to this
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnEquippedSpellChanged OnEquippedSpellChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // Input Overrides from InputCharacter
    // ========================================================================

    virtual void HandleFireInput() override;
    virtual void HandleCycleSpellInput(const FInputActionValue& Value) override;
    virtual void HandleSelectSpellSlot(int32 SlotIndex) override;

    // ========================================================================
    // Components
    // ========================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAC_SpellCollectionComponent* SpellCollectionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAC_AimComponent* AimComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAC_CombatComponent* CombatComponent;

    // ========================================================================
    // Spell Configuration
    // ========================================================================

    // Currently equipped spell type - fires this when pressing fire button
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WizardJam|Spells")
    FName EquippedSpellType;

    // Order of spells for cycling (matches HUD slot order)
    // Must match the SpellSlotConfigs order in HUD widget
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WizardJam|Spells")
    TArray<FName> SpellOrder;

    // If true, automatically equip first spell when collected
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WizardJam|Spells")
    bool bAutoEquipFirstSpell;

    // Enable teleport channel sync with spell collection
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spells|Teleport Sync")
    bool bSyncSpellChannelsToTeleport;

    // ========================================================================
    // Hybrid Bridge Handlers (Spell -> Teleport Channel Sync)
    // ========================================================================

    UFUNCTION()
    void OnSpellChannelAdded(FName Channel);

    UFUNCTION()
    void OnSpellChannelRemoved(FName Channel);

    // ========================================================================
    // Component Event Handlers
    // ========================================================================

    UFUNCTION()
    void HandleSpellAdded(FName SpellType, int32 TotalCount);

    UFUNCTION()
    void HandleProjectileFired(ABaseProjectile* Projectile, FName ProjectileType, FVector FireDirection);

    UFUNCTION()
    void HandleFireBlocked(EFireBlockedReason Reason, FName AttemptedType);

    UFUNCTION()
    void HandleAimTargetChanged(AActor* NewTarget, EAimTraceResult TargetType);

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================

    // Find index of spell in SpellOrder array
    int32 FindSpellIndex(FName SpellType) const;

    // Find next unlocked spell starting from given index
    int32 FindNextUnlockedSpellIndex(int32 StartIndex, bool bForward) const;

    // Initialize default spell order if not configured
    void InitializeDefaultSpellOrder();

public:
    // ========================================================================
    // Debug Commands (Console)
    // ========================================================================

    UFUNCTION(Exec)
    void Debug_TakeDamage(float Amount);

    UFUNCTION(Exec)
    void Debug_Heal(float Amount);

    UFUNCTION(Exec)
    void Debug_DrainStamina(float Amount);

    UFUNCTION(Exec)
    void Debug_RestoreStamina(float Amount);

    UFUNCTION(Exec)
    void Debug_AddSpell(FName SpellType);

    UFUNCTION(Exec)
    void Debug_SwitchSpell(FName SpellType);

    UFUNCTION(Exec)
    void Debug_ListSpells();
};