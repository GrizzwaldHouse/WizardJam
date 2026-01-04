// ============================================================================
// AC_SpellCollectionComponent.cpp
// Developer: Marcus Daley
// Date: December 25, 2025
// ============================================================================
// Purpose:
// Implementation of the Spell Collection Component. All spell tracking,
// channel management, and event broadcasting happens here.
//
// Key Implementation Details:
// - Uses TSet<FName> for O(1) lookup and automatic duplicate prevention
// - Static delegate allows GameMode to receive events from ALL components
// - Instance delegates allow per-actor reactions (VFX, animations)
// - All functions validate input (NAME_None checks)
// - Verbose logging for easy debugging
//
// Integration with SpellCollectible:
// 1. SpellCollectible checks if actor implements ISpellCollector
// 2. Calls GetSpellCollectionComponent() to get this component
// 3. Validates via IsCollectionEnabled() and channel requirements
// 4. Calls AddSpell() which broadcasts to static and instance delegates
// 5. GameMode receives global event, actor receives instance event
// ============================================================================

#include "Code/Utility/AC_SpellCollectionComponent.h"

// NOTE: No TeleportInterface include here!
// The component is fully decoupled from the teleport system.
// Channel sync happens via delegates - the OWNER decides how to handle it.

DEFINE_LOG_CATEGORY(LogSpellCollection);

// Static delegate definition - GameMode binds to this once
FOnAnySpellCollectedGlobal UAC_SpellCollectionComponent::OnAnySpellCollected;

// ============================================================================
// CONSTRUCTOR
// All defaults initialized here - designer overrides in Blueprint
// ============================================================================

UAC_SpellCollectionComponent::UAC_SpellCollectionComponent()
    : bCollectionEnabled(true)  // Default: collection is enabled
{
    // Component does not need to tick - purely event-driven
    PrimaryComponentTick.bCanEverTick = false;

    // This component can be added in Blueprint
    // The meta tag BlueprintSpawnableComponent in UCLASS handles this
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAC_SpellCollectionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Grant starting channels if configured
    for (const FName& Channel : StartingChannels)
    {
        if (Channel != NAME_None)
        {
            UnlockedChannels.Add(Channel);
        }
    }

    // Grant starting spells if configured
    for (const FName& Spell : StartingSpells)
    {
        if (Spell != NAME_None)
        {
            CollectedSpells.Add(Spell);
        }
    }

    // Log initial state for debugging
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] SpellCollectionComponent initialized | Enabled: %s | Starting Spells: %d | Starting Channels: %d"),
        *OwnerName,
        bCollectionEnabled ? TEXT("YES") : TEXT("NO"),
        CollectedSpells.Num(),
        UnlockedChannels.Num());
}

// ============================================================================
// SPELL MANAGEMENT
// ============================================================================

bool UAC_SpellCollectionComponent::AddSpell(FName SpellType)
{
    // Get owner name for logging
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    // Validate input - NAME_None would pollute the set
    if (SpellType == NAME_None)
    {
        UE_LOG(LogSpellCollection, Warning,
            TEXT("[%s] AddSpell failed: SpellType is NAME_None"),
            *OwnerName);
        return false;
    }

    // Check if collection is enabled
    if (!bCollectionEnabled)
    {
        UE_LOG(LogSpellCollection, Log,
            TEXT("[%s] AddSpell('%s') denied: Collection is disabled"),
            *OwnerName, *SpellType.ToString());
        return false;
    }

    // Check for duplicate
    if (CollectedSpells.Contains(SpellType))
    {
        UE_LOG(LogSpellCollection, Log,
            TEXT("[%s] AddSpell('%s') skipped: Already collected"),
            *OwnerName, *SpellType.ToString());
        return false;
    }

    // Add the spell
    CollectedSpells.Add(SpellType);
    int32 TotalCount = CollectedSpells.Num();

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] === SPELL COLLECTED === '%s' | Total: %d"),
        *OwnerName, *SpellType.ToString(), TotalCount);

    // Broadcast to instance delegate (actor-specific reactions)
    OnSpellAdded.Broadcast(SpellType, TotalCount);

    // Broadcast to static delegate (GameMode global tracking)
    OnAnySpellCollected.Broadcast(SpellType, Owner, this);

    return true;
}

bool UAC_SpellCollectionComponent::HasSpell(FName SpellType) const
{
    // NAME_None never matches
    if (SpellType == NAME_None)
    {
        return false;
    }

    return CollectedSpells.Contains(SpellType);
}

bool UAC_SpellCollectionComponent::RemoveSpell(FName SpellType)
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    // Validate input
    if (SpellType == NAME_None)
    {
        UE_LOG(LogSpellCollection, Warning,
            TEXT("[%s] RemoveSpell failed: SpellType is NAME_None"),
            *OwnerName);
        return false;
    }

    // Check if we have this spell
    if (!CollectedSpells.Contains(SpellType))
    {
        UE_LOG(LogSpellCollection, Log,
            TEXT("[%s] RemoveSpell('%s') failed: Not in collection"),
            *OwnerName, *SpellType.ToString());
        return false;
    }

    // Remove the spell
    CollectedSpells.Remove(SpellType);
    int32 RemainingCount = CollectedSpells.Num();

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] === SPELL REMOVED === '%s' | Remaining: %d"),
        *OwnerName, *SpellType.ToString(), RemainingCount);

    // Broadcast removal
    OnSpellRemoved.Broadcast(SpellType, RemainingCount);

    return true;
}

TArray<FName> UAC_SpellCollectionComponent::GetAllSpells() const
{
    // Convert TSet to TArray for Blueprint iteration
    // TSet doesn't iterate well in Blueprints
    return CollectedSpells.Array();
}

int32 UAC_SpellCollectionComponent::GetSpellCount() const
{
    return CollectedSpells.Num();
}

void UAC_SpellCollectionComponent::ClearAllSpells()
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    int32 PreviousCount = CollectedSpells.Num();

    // Store names before clearing for individual removal broadcasts
    TArray<FName> SpellsToRemove = CollectedSpells.Array();

    CollectedSpells.Empty();

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] === ALL SPELLS CLEARED === Previous count: %d"),
        *OwnerName, PreviousCount);

    // Broadcast individual removals so listeners can react per-spell
    for (const FName& Spell : SpellsToRemove)
    {
        OnSpellRemoved.Broadcast(Spell, 0);
    }

    // Broadcast the clear event
    OnAllSpellsCleared.Broadcast(PreviousCount);
}

// ============================================================================
// CHANNEL MANAGEMENT
// Channels gate spell requirements and teleporter access
// ============================================================================

void UAC_SpellCollectionComponent::AddChannel(FName Channel)
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    if (Channel == NAME_None)
    {
        UE_LOG(LogSpellCollection, Warning,
            TEXT("[%s] AddChannel failed: Channel is NAME_None"),
            *OwnerName);
        return;
    }

    if (UnlockedChannels.Contains(Channel))
    {
        UE_LOG(LogSpellCollection, Verbose,
            TEXT("[%s] AddChannel('%s') skipped: Already unlocked"),
            *OwnerName, *Channel.ToString());
        return;
    }

    UnlockedChannels.Add(Channel);

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] Channel unlocked: '%s' | Total channels: %d"),
        *OwnerName, *Channel.ToString(), UnlockedChannels.Num());

    // ========================================================================
    // HYBRID BRIDGE: Broadcast channel addition via delegate
    // 
    // This is the DECOUPLED approach:
    // - Component broadcasts the event
    // - Component has NO knowledge of teleport system
    // - Owner decides IF and HOW to handle this
    //
    // Example: In BasePlayer::BeginPlay():
    //   SpellCollectionComponent->OnChannelAdded.AddDynamic(
    //       this, &ABasePlayer::OnSpellChannelAdded);
    //
    // Then in ABasePlayer::OnSpellChannelAdded(FName Channel):
    //   AllowedTeleportChannels.AddUnique(Channel);
    //
    // Companion might not bind at all, so channels only affect spells
    // Enemy might bind to do something completely different
    // ========================================================================
    OnChannelAdded.Broadcast(Channel);
}

bool UAC_SpellCollectionComponent::HasChannel(FName Channel) const
{
    if (Channel == NAME_None)
    {
        return false;
    }

    return UnlockedChannels.Contains(Channel);
}

void UAC_SpellCollectionComponent::RemoveChannel(FName Channel)
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    if (Channel == NAME_None)
    {
        return;
    }

    if (UnlockedChannels.Remove(Channel) > 0)
    {
        UE_LOG(LogSpellCollection, Display,
            TEXT("[%s] Channel removed: '%s'"),
            *OwnerName, *Channel.ToString());

        // Broadcast so owner can also remove from teleport channels if desired
        OnChannelRemoved.Broadcast(Channel);
    }
}

TArray<FName> UAC_SpellCollectionComponent::GetAllChannels() const
{
    return UnlockedChannels.Array();
}

void UAC_SpellCollectionComponent::ClearAllChannels()
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    int32 PreviousCount = UnlockedChannels.Num();
    UnlockedChannels.Empty();

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] All channels cleared (had %d)"),
        *OwnerName, PreviousCount);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool UAC_SpellCollectionComponent::IsCollectionEnabled() const
{
    return bCollectionEnabled;
}

void UAC_SpellCollectionComponent::SetCollectionEnabled(bool bEnabled)
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    bCollectionEnabled = bEnabled;

    UE_LOG(LogSpellCollection, Display,
        TEXT("[%s] Spell collection %s"),
        *OwnerName, bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

// ============================================================================
// DEBUG
// ============================================================================

void UAC_SpellCollectionComponent::DebugPrintSpells() const
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    UE_LOG(LogSpellCollection, Warning,
        TEXT("========== [%s] COLLECTED SPELLS (%d) =========="),
        *OwnerName, CollectedSpells.Num());

    for (const FName& Spell : CollectedSpells)
    {
        UE_LOG(LogSpellCollection, Warning, TEXT("  - %s"), *Spell.ToString());
    }

    if (CollectedSpells.Num() == 0)
    {
        UE_LOG(LogSpellCollection, Warning, TEXT("  (No spells collected)"));
    }

    UE_LOG(LogSpellCollection, Warning,
        TEXT("  Collection Enabled: %s"),
        bCollectionEnabled ? TEXT("YES") : TEXT("NO"));

    UE_LOG(LogSpellCollection, Warning, TEXT("=========================================="));
}

void UAC_SpellCollectionComponent::DebugPrintChannels() const
{
    AActor* Owner = GetOwner();
    FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");

    UE_LOG(LogSpellCollection, Warning,
        TEXT("========== [%s] UNLOCKED CHANNELS (%d) =========="),
        *OwnerName, UnlockedChannels.Num());

    for (const FName& Channel : UnlockedChannels)
    {
        UE_LOG(LogSpellCollection, Warning, TEXT("  - %s"), *Channel.ToString());
    }

    if (UnlockedChannels.Num() == 0)
    {
        UE_LOG(LogSpellCollection, Warning, TEXT("  (No channels unlocked)"));
    }

    UE_LOG(LogSpellCollection, Warning, TEXT("=========================================="));
}