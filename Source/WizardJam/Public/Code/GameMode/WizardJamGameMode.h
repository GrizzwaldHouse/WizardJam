// ============================================================================
// WizardJamGameMode.h
// Developer: Marcus Daley
// Date: January 4, 2026
// ============================================================================
// Purpose:
// Central game mode for WizardJam that tracks spell collection, wave progression,
// Quidditch-style goal scoring, and win conditions using pure Observer pattern.
//
// Architecture:
// - STATIC delegate OnSpellCollectedGlobal allows ANY class to bind without
//   needing a GameMode reference.
// - Instance delegates for Blueprint binding.
// - Binds to QuidditchGoal delegates for scoring events.
// - Tracks scores, win conditions, and match state.
//
// Observer Pattern Flow:
// 1. QuidditchGoal broadcasts via OnGoalScored delegate
// 2. GameMode receives via bound handler
// 3. GameMode broadcasts via OnPlayerScored/OnAIScored
// 4. HUD receives and updates scoreboard
//
// Designer Usage:
// - Set WizardJamGameMode as GameMode Override in World Settings
// - Place QuidditchGoal actors in level with proper TeamID
// - Configure WinningScore (default 50)
// - System works automatically via delegates
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WizardJamGameMode.generated.h"

// Forward declarations
class ASpellCollectible;
class UAC_SpellCollectionComponent;
class AQuidditchGoal;

DECLARE_LOG_CATEGORY_EXTERN(LogWizardJamGameMode, Log, All);

// ============================================================================
// STATIC DELEGATE (For C++ Listeners like HUD)
// HUD binds to this WITHOUT needing a GameMode reference
// ============================================================================

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnSpellCollectedGlobal,
    FName,      // SpellTypeName
    int32       // TotalSpellsCollected
);

// ============================================================================
// INSTANCE DELEGATES (For Blueprint Listeners)
// ============================================================================

// Spell collection delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSpellTypeCollected,
    FName, SpellTypeName,
    int32, TotalSpellsCollected
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAnySpellEvent,
    FName, SpellTypeName,
    AActor*, CollectingActor
);

// Quidditch scoring delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnPlayerScored,
    int32, NewScore,
    int32, PointsAdded
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAIScored,
    int32, NewScore,
    int32, PointsAdded
);

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
    // ========================================================================

    static FOnSpellCollectedGlobal OnSpellCollectedGlobal;

    // ========================================================================
    // SPELL COLLECTION EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnSpellTypeCollected OnSpellTypeCollected;

    UPROPERTY(BlueprintAssignable, Category = "Spells|Events")
    FOnAnySpellEvent OnAnySpellEvent;

    // ========================================================================
    // QUIDDITCH SCORING EVENTS
    // HUD binds to these for scoreboard updates
    // ========================================================================

    // Broadcast when player scores (Team 0)
    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnPlayerScored OnPlayerScored;

    // Broadcast when AI scores (Team 1)
    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnAIScored OnAIScored;

    // Broadcast when match ends (win or lose)
    UPROPERTY(BlueprintAssignable, Category = "Quidditch|Events")
    FOnMatchEnded OnMatchEnded;

    // ========================================================================
    // SPELL QUERIES
    // ========================================================================

    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasCollectedSpell(FName SpellTypeName) const;

    UFUNCTION(BlueprintPure, Category = "Spells")
    TArray<FName> GetCollectedSpells() const;

    UFUNCTION(BlueprintPure, Category = "Spells")
    int32 GetTotalSpellsCollected() const { return CollectedSpells.Num(); }

    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasAllSpells(const TArray<FName>& RequiredSpells) const;

    UFUNCTION(BlueprintPure, Category = "Spells")
    bool HasAnySpell(const TArray<FName>& CheckSpells) const;

    // ========================================================================
    // QUIDDITCH SCORE QUERIES
    // ========================================================================

    UFUNCTION(BlueprintPure, Category = "Quidditch|Score")
    int32 GetPlayerScore() const { return PlayerScore; }

    UFUNCTION(BlueprintPure, Category = "Quidditch|Score")
    int32 GetAIScore() const { return AIScore; }

    UFUNCTION(BlueprintPure, Category = "Quidditch|Score")
    int32 GetWinningScore() const { return WinningScore; }

    // ========================================================================
    // QUIDDITCH CONFIGURATION
    // Designer sets these in Blueprint child
    // ========================================================================

    // Score needed to win the match
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch|Rules",
        meta = (DisplayName = "Winning Score", ClampMin = "10"))
    int32 WinningScore;

    // Time limit for match in seconds (0 = no limit)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch|Rules",
        meta = (DisplayName = "Match Time Limit", ClampMin = "0.0"))
    float MatchTimeLimit;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // SPELL RUNTIME STATE
    // ========================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spells")
    TSet<FName> CollectedSpells;

    // ========================================================================
    // QUIDDITCH RUNTIME STATE
    // ========================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quidditch|Score")
    int32 PlayerScore;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quidditch|Score")
    int32 AIScore;

private:
    // ========================================================================
    // SPELL DELEGATE HANDLERS
    // ========================================================================

    void HandleSpellCollectiblePickedUp(
        FName SpellTypeName,
        AActor* CollectingActor,
        ASpellCollectible* Collectible);

    void HandleComponentSpellCollected(
        FName SpellTypeName,
        AActor* OwningActor,
        UAC_SpellCollectionComponent* Component);

    void ProcessNewSpell(FName SpellTypeName, AActor* CollectingActor);

    // ========================================================================
    // QUIDDITCH DELEGATE HANDLERS
    // ========================================================================

    // Called when any QuidditchGoal is scored on
    UFUNCTION()
    void OnGoalScored(AActor* ScoringActor, FName Element, int32 PointsAwarded, bool bWasCorrectElement);

    // Check if match should end
    void CheckMatchEnd();

    // Get team ID of actor (0 = player, 1 = AI)
    int32 GetActorTeamID(AActor* Actor) const;

    // Called when match time expires
    void OnMatchTimeExpired();

    // Match timer handle
    FTimerHandle MatchTimerHandle;
};