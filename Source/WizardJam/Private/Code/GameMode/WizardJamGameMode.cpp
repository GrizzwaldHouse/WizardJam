// ============================================================================
// WizardJamGameMode.cpp
// Developer: Marcus Daley
// Date: January 4, 2026
// ============================================================================
// Purpose:
// Implementation of WizardJam game mode with spell collection and Quidditch
// goal scoring using Observer pattern with STATIC and instance delegates.
//
// Key Implementation Details:
// - BeginPlay binds to SpellCollectible, Component, and QuidditchGoal delegates
// - ProcessNewSpell broadcasts on BOTH static and instance delegates
// - QuidditchGoal scoring tracks player vs AI scores
// - Match ends when either team reaches WinningScore or time expires
//
// Observer Pattern Flow:
// Spell Collection:
//   SpellCollectible -> GameMode -> OnSpellCollectedGlobal -> HUD
//
// Quidditch Scoring:
//   QuidditchGoal -> GameMode -> OnPlayerScored/OnAIScored -> HUD
// ============================================================================

#include "Code/GameMode/WizardJamGameMode.h"
#include "Code/Actors/SpellCollectible.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Code/Actors/QuidditchGoal.h"
#include "GenericTeamAgentInterface.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogWizardJamGameMode);

// ============================================================================
// STATIC DELEGATE DEFINITION
// This line creates the actual storage for the static delegate.
// Without this, you get linker errors about unresolved external symbol.
// ============================================================================

FOnSpellCollectedGlobal AWizardJamGameMode::OnSpellCollectedGlobal;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AWizardJamGameMode::AWizardJamGameMode()
    : WinningScore(50)
    , MatchTimeLimit(0.0f)
    , PlayerScore(0)
    , AIScore(0)
{
    // Constructor initialization list sets all values
    // No hardcoded values in function body per Nick Penney standards
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AWizardJamGameMode::BeginPlay()
{
    Super::BeginPlay();

    // ========================================================================
    // BIND TO SPELL SYSTEM STATIC DELEGATES
    // ========================================================================

    ASpellCollectible::OnAnySpellPickedUp.AddUObject(
        this, &AWizardJamGameMode::HandleSpellCollectiblePickedUp);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Bound to ASpellCollectible::OnAnySpellPickedUp"));

    UAC_SpellCollectionComponent::OnAnySpellCollected.AddUObject(
        this, &AWizardJamGameMode::HandleComponentSpellCollected);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Bound to UAC_SpellCollectionComponent::OnAnySpellCollected"));

    // ========================================================================
    // BIND TO QUIDDITCH GOALS
    // Find all goals in level and bind to their scoring delegates
    // ========================================================================

    for (TActorIterator<AQuidditchGoal> It(GetWorld()); It; ++It)
    {
        AQuidditchGoal* Goal = *It;
        if (Goal)
        {
            Goal->OnGoalScored.AddDynamic(this, &AWizardJamGameMode::OnGoalScored);
            UE_LOG(LogWizardJamGameMode, Display,
                TEXT("[GameMode] Bound to goal: %s (Element: %s, Team: %d)"),
                *Goal->GetName(), *Goal->GoalElement.ToString(), Goal->TeamID);
        }
    }

    // ========================================================================
    // START MATCH TIMER (if time limit set)
    // ========================================================================

    if (MatchTimeLimit > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            MatchTimerHandle,
            this,
            &AWizardJamGameMode::OnMatchTimeExpired,
            MatchTimeLimit,
            false
        );
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("[GameMode] Match timer started: %.0f seconds"), MatchTimeLimit);
    }

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=== WIZARDJAM MATCH STARTED ==="));
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("Winning Score: %d | Time Limit: %.0f"), WinningScore, MatchTimeLimit);
}

void AWizardJamGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // ========================================================================
    // DELEGATE CLEANUP
    // Static delegates MUST be unbound in EndPlay or we get crashes
    // ========================================================================

    ASpellCollectible::OnAnySpellPickedUp.RemoveAll(this);
    UAC_SpellCollectionComponent::OnAnySpellCollected.RemoveAll(this);

    // Clear match timer
    GetWorldTimerManager().ClearTimer(MatchTimerHandle);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Unbound from all delegates"));

    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// SPELL QUERY FUNCTIONS
// ============================================================================

bool AWizardJamGameMode::HasCollectedSpell(FName SpellTypeName) const
{
    return CollectedSpells.Contains(SpellTypeName);
}

TArray<FName> AWizardJamGameMode::GetCollectedSpells() const
{
    return CollectedSpells.Array();
}

bool AWizardJamGameMode::HasAllSpells(const TArray<FName>& RequiredSpells) const
{
    for (const FName& Spell : RequiredSpells)
    {
        if (Spell != NAME_None && !CollectedSpells.Contains(Spell))
        {
            return false;
        }
    }
    return true;
}

bool AWizardJamGameMode::HasAnySpell(const TArray<FName>& CheckSpells) const
{
    for (const FName& Spell : CheckSpells)
    {
        if (Spell != NAME_None && CollectedSpells.Contains(Spell))
        {
            return true;
        }
    }
    return false;
}

// ============================================================================
// SPELL DELEGATE HANDLERS
// ============================================================================

void AWizardJamGameMode::HandleSpellCollectiblePickedUp(
    FName SpellTypeName,
    AActor* CollectingActor,
    ASpellCollectible* Collectible)
{
    FString CollectorName = CollectingActor ? CollectingActor->GetName() : TEXT("Unknown");

    UE_LOG(LogWizardJamGameMode, Log,
        TEXT("[GameMode] SpellCollectible picked up | Spell: '%s' | Collector: '%s'"),
        *SpellTypeName.ToString(), *CollectorName);

    // Broadcast raw event for VFX/SFX (fires even on duplicates)
    OnAnySpellEvent.Broadcast(SpellTypeName, CollectingActor);

    // Process for unique tracking
    ProcessNewSpell(SpellTypeName, CollectingActor);
}

void AWizardJamGameMode::HandleComponentSpellCollected(
    FName SpellTypeName,
    AActor* OwningActor,
    UAC_SpellCollectionComponent* Component)
{
    // Only process if not already tracked (prevents double events)
    if (!CollectedSpells.Contains(SpellTypeName))
    {
        ProcessNewSpell(SpellTypeName, OwningActor);
    }
}

void AWizardJamGameMode::ProcessNewSpell(FName SpellTypeName, AActor* CollectingActor)
{
    // Validate input
    if (SpellTypeName == NAME_None)
    {
        UE_LOG(LogWizardJamGameMode, Warning,
            TEXT("[GameMode] Received spell with NAME_None - ignoring"));
        return;
    }

    // Check if already collected
    if (CollectedSpells.Contains(SpellTypeName))
    {
        UE_LOG(LogWizardJamGameMode, Log,
            TEXT("[GameMode] Spell '%s' already collected - not broadcasting new spell event"),
            *SpellTypeName.ToString());
        return;
    }

    // Add to tracking set
    CollectedSpells.Add(SpellTypeName);
    int32 TotalCount = CollectedSpells.Num();

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=========================================="));
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=== NEW SPELL TYPE COLLECTED ==="));
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("Type: '%s' | Total Unique: %d"),
        *SpellTypeName.ToString(), TotalCount);
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=========================================="));

    // Broadcast on static delegate (C++ listeners like HUD)
    OnSpellCollectedGlobal.Broadcast(SpellTypeName, TotalCount);

    // Broadcast on instance delegate (Blueprint listeners)
    OnSpellTypeCollected.Broadcast(SpellTypeName, TotalCount);
}

// ============================================================================
// QUIDDITCH SCORING HANDLERS
// ============================================================================

void AWizardJamGameMode::OnGoalScored(
    AActor* ScoringActor,
    FName Element,
    int32 PointsAwarded,
    bool bWasCorrectElement)
{
    if (!ScoringActor || PointsAwarded == 0)
    {
        return;
    }

    // Determine which team scored based on actor's team ID
    int32 ScorerTeamID = GetActorTeamID(ScoringActor);

    if (ScorerTeamID == 0)
    {
        // Player team scored
        PlayerScore += PointsAwarded;
        OnPlayerScored.Broadcast(PlayerScore, PointsAwarded);

        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("========================================"));
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("=== PLAYER SCORED ==="));
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("Points: +%d | Total: %d | Element: %s"),
            PointsAwarded, PlayerScore, *Element.ToString());
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("========================================"));
    }
    else if (ScorerTeamID == 1)
    {
        // AI team scored
        AIScore += PointsAwarded;
        OnAIScored.Broadcast(AIScore, PointsAwarded);

        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("========================================"));
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("=== AI SCORED ==="));
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("Points: +%d | Total: %d | Element: %s"),
            PointsAwarded, AIScore, *Element.ToString());
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("========================================"));
    }
    else
    {
        UE_LOG(LogWizardJamGameMode, Warning,
            TEXT("[GameMode] Unknown team ID %d for actor %s"),
            ScorerTeamID, *ScoringActor->GetName());
    }

    // Check if match should end
    CheckMatchEnd();
}

void AWizardJamGameMode::CheckMatchEnd()
{
    // Check if player reached winning score
    if (PlayerScore >= WinningScore)
    {
        OnMatchEnded.Broadcast(true);
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("=== PLAYER WINS! === Final Score: Player %d - AI %d"),
            PlayerScore, AIScore);

        // Stop match timer
        GetWorldTimerManager().ClearTimer(MatchTimerHandle);
    }
    // Check if AI reached winning score
    else if (AIScore >= WinningScore)
    {
        OnMatchEnded.Broadcast(false);
        UE_LOG(LogWizardJamGameMode, Display,
            TEXT("=== AI WINS! === Final Score: Player %d - AI %d"),
            PlayerScore, AIScore);

        // Stop match timer
        GetWorldTimerManager().ClearTimer(MatchTimerHandle);
    }
}

int32 AWizardJamGameMode::GetActorTeamID(AActor* Actor) const
{
    if (!Actor)
    {
        return -1;
    }

    // Check if actor implements IGenericTeamAgentInterface
    IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Actor);
    if (TeamAgent)
    {
        return TeamAgent->GetGenericTeamId().GetId();
    }

    // Default to player team (0) if no team interface
    return 0;
}

void AWizardJamGameMode::OnMatchTimeExpired()
{
    // Determine winner by score when time expires
    bool bPlayerWon = PlayerScore > AIScore;

    OnMatchEnded.Broadcast(bPlayerWon);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=== TIME EXPIRED ==="));
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("Final Score: Player %d - AI %d"), PlayerScore, AIScore);
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("Winner: %s"), bPlayerWon ? TEXT("PLAYER") : TEXT("AI"));
}