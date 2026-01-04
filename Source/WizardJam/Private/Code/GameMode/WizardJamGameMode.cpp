// ============================================================================
// WizardJamGameMode.cpp
// Developer: Marcus Daley
// Date: December 25, 2025
// ============================================================================
// Purpose:
// Implementation of WizardJam game mode with STATIC delegate broadcasting.
//
// Key Implementation Details:
// - BeginPlay binds to SpellCollectible and Component static delegates
// - ProcessNewSpell broadcasts on BOTH static and instance delegates
// - Static delegate allows HUD to bind without GameMode reference
// - Instance delegate allows Blueprint binding
//
// Static Delegate Definition:
// The static delegate must be DEFINED in the .cpp file (not just declared).
// This creates the actual storage for the delegate's invocation list.
//
// Broadcast Order:
// 1. OnSpellCollectedGlobal.Broadcast() - C++ listeners (HUD)
// 2. OnSpellTypeCollected.Broadcast() - Blueprint listeners
// 3. OnAnySpellEvent.Broadcast() - VFX/SFX listeners (including duplicates)
// ============================================================================

#include "Code/GameMode/WizardJamGameMode.h"
#include "Code/Actors/SpellCollectible.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Code/Actors/QuidditchGoal.h"
#include "GenericTeamAgentInterface.h"
#include "EngineUtils.h"
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

AWizardJamGameMode::AWizardJamGameMode(): PlayerScore(0)
, AIScore(0)
, WinningScore(50)
, MatchTimeLimit(0.0f)
{
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AWizardJamGameMode::BeginPlay()
{
    Super::BeginPlay();

    // ========================================================================
    // BIND TO SPELL SYSTEM STATIC DELEGATES
    // GameMode is a LISTENER here - it receives events from spell system
    // ========================================================================

    // Bind to SpellCollectible's static delegate
    // Fires when any SpellCollectible in the world is picked up
    ASpellCollectible::OnAnySpellPickedUp.AddUObject(
        this, &AWizardJamGameMode::HandleSpellCollectiblePickedUp);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Bound to ASpellCollectible::OnAnySpellPickedUp"));

    // Bind to SpellCollectionComponent's static delegate
    // Fires when any component adds a spell (even without collectible)
    UAC_SpellCollectionComponent::OnAnySpellCollected.AddUObject(
        this, &AWizardJamGameMode::HandleComponentSpellCollected);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Bound to UAC_SpellCollectionComponent::OnAnySpellCollected"));

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("=== WizardJamGameMode Initialized ==="));
    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("Broadcasting spell events on AWizardJamGameMode::OnSpellCollectedGlobal"));

    // Bind to all Quidditch goals in level
    for (TActorIterator<AQuidditchGoal> It(GetWorld()); It; ++It)
    {
        AQuidditchGoal* Goal = *It;
        if (Goal)
        {
            Goal->OnGoalScored.AddDynamic(this, &AWizardJamGameMode::OnGoalScored);
            UE_LOG(LogWizardJamGameMode, Display, TEXT("[GameMode] Bound to goal: %s (Element: %s, Team: %d)"),
                *Goal->GetName(), *Goal->GoalElement.ToString(), Goal->TeamID);
        }
    }

    // Start match timer if time limit is set
    if (MatchTimeLimit > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            MatchTimerHandle,
            this,
            &AWizardJamGameMode::OnMatchTimeExpired,
            MatchTimeLimit,
            false
        );
        UE_LOG(LogWizardJamGameMode, Display, TEXT("[GameMode] Match timer started: %.0f seconds"), MatchTimeLimit);
    }

    UE_LOG(LogWizardJamGameMode, Display, TEXT("=== QUIDDITCH MATCH STARTED ==="));
    UE_LOG(LogWizardJamGameMode, Display, TEXT("Winning Score: %d | Time Limit: %.0f"), WinningScore, MatchTimeLimit);
}

void AWizardJamGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // ========================================================================
    // DELEGATE CLEANUP
    // Static delegates MUST be unbound in EndPlay or we get crashes.
    // RemoveAll removes all bindings for this object from the delegate.
    // ========================================================================

    ASpellCollectible::OnAnySpellPickedUp.RemoveAll(this);
    UAC_SpellCollectionComponent::OnAnySpellCollected.RemoveAll(this);

    UE_LOG(LogWizardJamGameMode, Display,
        TEXT("[GameMode] Unbound from spell system static delegates"));

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
// DELEGATE HANDLERS
// ============================================================================

void AWizardJamGameMode::HandleSpellCollectiblePickedUp(
    FName SpellTypeName,
    AActor* CollectingActor,
    ASpellCollectible* Collectible)
{
    // ========================================================================
    // Handler for SpellCollectible static delegate
    // This is called when ANY SpellCollectible in the world is picked up
    // ========================================================================

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
    // ========================================================================
    // Handler for SpellCollectionComponent static delegate
    // This catches spells added by ANY method (collectible, debug, script)
    // ========================================================================

    // Only process if not already tracked (prevents double events)
    if (!CollectedSpells.Contains(SpellTypeName))
    {
        ProcessNewSpell(SpellTypeName, OwningActor);
    }
}

// ============================================================================
// SHARED SPELL PROCESSING
// ============================================================================

void AWizardJamGameMode::ProcessNewSpell(FName SpellTypeName, AActor* CollectingActor)
{
    // ========================================================================
    // Central spell processing logic
    // Adds to tracking and broadcasts on BOTH static and instance delegates
    // ========================================================================

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

    // ========================================================================
    // BROADCAST ON STATIC DELEGATE (C++ Listeners)
    // HUD binds to this without needing a GameMode reference.
    // This is the pure Observer pattern in action.
    // ========================================================================

    OnSpellCollectedGlobal.Broadcast(SpellTypeName, TotalCount);

    UE_LOG(LogWizardJamGameMode, Verbose,
        TEXT("[GameMode] Broadcast on OnSpellCollectedGlobal"));

    // ========================================================================
    // BROADCAST ON INSTANCE DELEGATE (Blueprint Listeners)
    // Blueprint widgets that have a GameMode reference bind to this.
    // ========================================================================

    OnSpellTypeCollected.Broadcast(SpellTypeName, TotalCount);

    UE_LOG(LogWizardJamGameMode, Verbose,
        TEXT("[GameMode] Broadcast on OnSpellTypeCollected (instance)"));
}

void AWizardJamGameMode::OnGoalScored(AActor* ScoringActor, FName Element, int32 PointsAwarded, bool bWasCorrectElement)
{
    if (!ScoringActor || PointsAwarded == 0)
    {
        return;
    }

    // Determine which team scored
    int32 ScorerTeamID = GetActorTeamID(ScoringActor);

    if (ScorerTeamID == 0)
    {
        // Player scored
        PlayerScore += PointsAwarded;
        OnPlayerScored.Broadcast(PlayerScore, PointsAwarded);

        UE_LOG(LogWizardJamGameMode, Display, TEXT("========================================"));
        UE_LOG(LogWizardJamGameMode, Display, TEXT("=== PLAYER SCORED ==="));
        UE_LOG(LogWizardJamGameMode, Display, TEXT("Points: +%d | Total: %d | Element: %s"),
            PointsAwarded, PlayerScore, *Element.ToString());
        UE_LOG(LogWizardJamGameMode, Display, TEXT("========================================"));
    }
    else if (ScorerTeamID == 1)
    {
        // AI scored
        AIScore += PointsAwarded;
        OnAIScored.Broadcast(AIScore, PointsAwarded);

        UE_LOG(LogWizardJamGameMode, Display, TEXT("========================================"));
        UE_LOG(LogWizardJamGameMode, Display, TEXT("=== AI SCORED ==="));
        UE_LOG(LogWizardJamGameMode, Display, TEXT("Points: +%d | Total: %d | Element: %s"),
            PointsAwarded, AIScore, *Element.ToString());
        UE_LOG(LogWizardJamGameMode, Display, TEXT("========================================"));
    }

    // Check if match has ended
    CheckMatchEnd();
}

void AWizardJamGameMode::CheckMatchEnd()
{// Check if either team reached winning score
    if (PlayerScore >= WinningScore)
    {
        OnMatchEnded.Broadcast(true);
        UE_LOG(LogWizardJamGameMode, Display, TEXT("=== PLAYER WINS! === Final Score: Player %d - AI %d"),
            PlayerScore, AIScore);

        // Stop match timer
        GetWorldTimerManager().ClearTimer(MatchTimerHandle);
    }
    else if (AIScore >= WinningScore)
    {
        OnMatchEnded.Broadcast(false);
        UE_LOG(LogWizardJamGameMode, Display, TEXT("=== AI WINS! === Final Score: Player %d - AI %d"),
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

    // Default to player team
    return 0;
}

void AWizardJamGameMode::OnMatchTimeExpired()
{    // Determine winner by score
    bool bPlayerWon = PlayerScore > AIScore;

    OnMatchEnded.Broadcast(bPlayerWon);

    UE_LOG(LogWizardJamGameMode, Display, TEXT("=== TIME EXPIRED ==="));
    UE_LOG(LogWizardJamGameMode, Display, TEXT("Final Score: Player %d - AI %d"), PlayerScore, AIScore);
    UE_LOG(LogWizardJamGameMode, Display, TEXT("Winner: %s"), bPlayerWon ? TEXT("PLAYER") : TEXT("AI"));
}
