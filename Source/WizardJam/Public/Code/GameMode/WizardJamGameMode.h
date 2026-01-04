// ============================================================================
// WizardJamGameMode.h
// Developer: Marcus Daley
// Date: December 25, 2025
// ============================================================================
// Purpose:
// Central game mode for WizardJam that tracks spell collection, wave progression,
// and win conditions using pure Observer pattern with STATIC delegates.
//
// Architecture:
// - STATIC delegate OnSpellCollectedGlobal allows ANY class to bind without
//   needing a GameMode reference. HUD binds by class name, not instance.
// - Instance delegate OnSpellTypeCollected kept for Blueprint binding.
// - Binds to SpellCollectible and SpellCollectionComponent static delegates.
// - Tracks unique spell types in TSet<FName> - fully modular.
//
// Observer Pattern Flow:
// 1. SpellCollectible broadcasts via ASpellCollectible::OnAnySpellPickedUp
// 2. GameMode receives via bound handler
// 3. GameMode broadcasts via AWizardJamGameMode::OnSpellCollectedGlobal (STATIC)
// 4. HUD receives without needing GameMode reference
//
// Why Static Delegate:
// - HUD binds with: AWizardJamGameMode::OnSpellCollectedGlobal.AddUObject(...)
// - HUD does NOT need: GetWorld()->GetAuthGameMode<>()
// - Pure decoupling - HUD doesn't know WHO broadcasts, just WHERE to listen
//
// Designer Usage:
// - Set WizardJamGameMode as GameMode Override in World Settings
// - Place spell collectibles in level
// - System works automatically via delegates
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WizardJamGameMode.generated.h"

// Forward declarations
class ASpellCollectible;
class UAC_SpellCollectionComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogWizardJamGameMode, Log, All);

// ============================================================================
// STATIC DELEGATE (For C++ Listeners like HUD)
// HUD binds to this WITHOUT needing a GameMode reference
// Usage: AWizardJamGameMode::OnSpellCollectedGlobal.AddUObject(this, &MyClass::Handler)
// ============================================================================

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnSpellCollectedGlobal,
    FName,      // SpellTypeName
    int32       // TotalSpellsCollected
);

// ============================================================================
// INSTANCE DELEGATE (For Blueprint Listeners)
// Blueprint binds to this via the instance reference
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSpellTypeCollected,
    FName, SpellTypeName,
    int32, TotalSpellsCollected
);

// Delegate for any spell event including duplicates (for VFX/SFX)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAnySpellEvent,
    FName, SpellTypeName,
    AActor*, CollectingActor
);

// ============================================================================
// QUIDDITCH SCORING DELEGATES (Declared at file scope, BEFORE class)
// ============================================================================

// Broadcast when player scores
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnPlayerScored,
    int32, NewScore,
    int32, PointsAdded
);

// Broadcast when AI scores
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAIScored,
    int32, NewScore,
    int32, PointsAdded
);

// Broadcast when match ends
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnMatchEnded,
    bool, bPlayerWon
);

UCLASS()
class WIZARDJAM_API AWizardJamGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AWizardJamGameMode();

    // ========================================================================
    // STATIC DELEGATE
    // C++ classes bind to this without needing a GameMode reference.
    // This is the pure Observer pattern - listener doesn't know broadcaster.
    //
    // Example binding in HUD:
    //   AWizardJamGameMode::OnSpellCollectedGlobal.AddUObject(
    //       this, &UWizardJamHUDWidget::HandleSpellCollected);
    // ========================================================================

    static FOnSpellCollectedGlobal OnSpellCollectedGlobal;

    // ========================================================================
    // INSTANCE DELEGATES (For Blueprint)
    // ========================================================================

    // Broadcast when a NEW spell type is collected (fires once per type)
    // Blueprint binds via GameMode reference in Event Graph
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnSpellTypeCollected OnSpellTypeCollected;

    // Broadcast on ANY spell event including duplicates
    // Useful for VFX/SFX that should play every pickup
    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnAnySpellEvent OnAnySpellEvent;

    // ========================================================================
    // QUIDDITCH SCORING DELEGATES (Instance properties)
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnPlayerScored OnPlayerScored;

    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnAIScored OnAIScored;

    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnMatchEnded OnMatchEnded;

    // ========================================================================
    // SPELL QUERIES
    // Use these to check spell state from anywhere
    // ========================================================================

    // Check if a specific spell type has been collected
    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasCollectedSpell(FName SpellTypeName) const;

    // Get array of all collected spell type names
    UFUNCTION(BlueprintPure, Category = "Spells")
    TArray<FName> GetCollectedSpells() const;

    // Get count of unique spell types collected
    UFUNCTION(BlueprintPure, Category = "Spells")
    int32 GetTotalSpellsCollected() const { return CollectedSpells.Num(); }

    // Check if player has ALL specified spells
    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasAllSpells(const TArray<FName>& RequiredSpells) const;

    // Check if player has ANY of the specified spells
    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasAnySpell(const TArray<FName>& CheckSpells) const;

    // ========================================================================
    // QUIDDITCH SCORING STATE
    // ========================================================================

    // Current player score
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quidditch|Score")
    int32 PlayerScore;

    // Current AI score
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quidditch|Score")
    int32 AIScore;

    // Score to win the match
    UPROPERTY(EditDefaultsOnly, Category = "Quidditch|Rules")
    int32 WinningScore;

    // Time limit for match (0 = no limit)
    UPROPERTY(EditDefaultsOnly, Category = "Quidditch|Rules")
    float MatchTimeLimit;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    // Tracks all unique spell type names collected
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spells")
    TSet<FName> CollectedSpells;

private:
    // ========================================================================
    // DELEGATE HANDLERS
    // Called by static delegates from spell system
    // ========================================================================

    // Handler for SpellCollectible's static delegate
    void HandleSpellCollectiblePickedUp(
        FName SpellTypeName,
        AActor* CollectingActor,
        ASpellCollectible* Collectible);

    // Handler for SpellCollectionComponent's static delegate
    void HandleComponentSpellCollected(
        FName SpellTypeName,
        AActor* OwningActor,
        UAC_SpellCollectionComponent* Component);

    // Shared logic for processing new spells
    void ProcessNewSpell(FName SpellTypeName, AActor* CollectingActor);

    // ========================================================================
    // QUIDDITCH SCORING HANDLERS
    // ========================================================================

    // Called when any goal is scored
    UFUNCTION()
    void OnGoalScored(AActor* ScoringActor, FName Element, int32 PointsAwarded, bool bWasCorrectElement);

    // Check if match has ended
    void CheckMatchEnd();

    // Get team ID of actor (0 = player, 1 = AI)
    int32 GetActorTeamID(AActor* Actor) const;

    // Match timer
    FTimerHandle MatchTimerHandle;

    // Called when time limit expires
    void OnMatchTimeExpired();
};