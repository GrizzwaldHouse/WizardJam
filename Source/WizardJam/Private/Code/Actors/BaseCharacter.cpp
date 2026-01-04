// BaseCharacter.cpp
// Developer: Marcus Daley
// Date: December 26, 2025
// Purpose: Implementation of base character class with modular spell collection
//
// Key Implementation Details:
// - Spell collection uses TSet<FName> for O(1) lookup and automatic duplicate prevention
// - All spell operations broadcast delegates for Observer pattern compliance
// - Channel system shared between teleport and spell requirements (single unified unlock system)
// - Constructor initializes all defaults - no hardcoded values scattered through code
//
// Modification Guide:
// To add a new character capability:
// 1. Add UPROPERTY in header with EditDefaultsOnly for class-level config
// 2. Add accessor function with BlueprintPure or BlueprintCallable
// 3. Add delegate if other systems need to react to changes
// 4. Initialize default in constructor initialization list

#include "Code/Actors/BaseCharacter.h"
#include "Code/Utility/AC_HealthComponent.h"
#include "Code/Utility/AC_StaminaComponent.h"

DEFINE_LOG_CATEGORY(LogBaseCharacter);

// ============================================================================
// CONSTRUCTOR
// All defaults initialized here - never scatter hardcoded values through code
// ============================================================================

ABaseCharacter::ABaseCharacter()
    : bCanCollectSpells(true)    // Default: most characters can collect spells
    , TeamID(0)                   // Default: player team
{
    PrimaryActorTick.bCanEverTick = true;

    // Create health component - manages hit points and damage
    HealthComponent = CreateDefaultSubobject<UAC_HealthComponent>(TEXT("HealthComponent"));

    // Create stamina component - manages sprint and special abilities
    StaminaComponent = CreateDefaultSubobject<UAC_StaminaComponent>(TEXT("StaminaComponent"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Log initial configuration for debugging
    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] BeginPlay - TeamID: %d | CanCollectSpells: %s | Channels: %d"),
        *GetName(),
        TeamID,
        bCanCollectSpells ? TEXT("YES") : TEXT("NO"),
        AllowedTeleportChannels.Num());
}

void ABaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // No tick logic by default - child classes can override
}

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Input binding handled in child classes (BasePlayer)
}

// ============================================================================
// COMPONENT ACCESSORS
// ============================================================================

UAC_HealthComponent* ABaseCharacter::GetHealthComponent() const
{
    return HealthComponent;
}

UAC_StaminaComponent* ABaseCharacter::GetStaminaComponent() const
{
    return StaminaComponent;
}

// ============================================================================
// TELEPORT INTERFACE IMPLEMENTATION
// ============================================================================

bool ABaseCharacter::CanTeleport(FName Channel)
{
    // If no channels specified, allow all teleports
    // This provides sensible default behavior
    if (AllowedTeleportChannels.Num() == 0)
    {
        return true;
    }

    // Check if channel is in allowed list
    bool bIsAllowed = AllowedTeleportChannels.Contains(Channel);

    UE_LOG(LogBaseCharacter, Verbose, TEXT("[%s] Teleport check for channel '%s': %s"),
        *GetName(),
        *Channel.ToString(),
        bIsAllowed ? TEXT("ALLOWED") : TEXT("DENIED"));

    return bIsAllowed;
}

void ABaseCharacter::OnTeleportExecuted(const FVector& TargetLocation, const FRotator& TargetRotation)
{
    // Override in child classes for custom behavior (effects, sounds, etc.)
    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Teleporting to location %s"),
        *GetName(), *TargetLocation.ToString());
}

FOnTeleportStart& ABaseCharacter::GetOnTeleportStart()
{
    return OnTeleportStartDelegate;
}

FOnTeleportComplete& ABaseCharacter::GetOnTeleportComplete()
{
    return OnTeleportCompleteDelegate;
}

// ============================================================================
// TELEPORT CHANNEL MANAGEMENT
// These channels also gate spell pickup requirements (unified unlock system)
// ============================================================================

void ABaseCharacter::AddTeleportChannel(const FName& Channel)
{
    // Validate input - NAME_None would pollute the array
    if (Channel == NAME_None)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] Attempted to add invalid channel NAME_None"),
            *GetName());
        return;
    }

    // AddUnique prevents duplicates
    if (AllowedTeleportChannels.Contains(Channel))
    {
        UE_LOG(LogBaseCharacter, Verbose, TEXT("[%s] Already has channel '%s'"),
            *GetName(), *Channel.ToString());
        return;
    }

    AllowedTeleportChannels.AddUnique(Channel);
    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Added channel: '%s' (Total: %d)"),
        *GetName(), *Channel.ToString(), AllowedTeleportChannels.Num());
}

void ABaseCharacter::RemoveTeleportChannel(const FName& Channel)
{
    int32 Removed = AllowedTeleportChannels.Remove(Channel);
    if (Removed > 0)
    {
        UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Removed channel: '%s'"),
            *GetName(), *Channel.ToString());
    }
}

bool ABaseCharacter::HasTeleportChannel(const FName& Channel) const
{
    bool bHasChannel = AllowedTeleportChannels.Contains(Channel);
    UE_LOG(LogBaseCharacter, Verbose, TEXT("[%s] Has channel '%s': %s"),
        *GetName(), *Channel.ToString(), bHasChannel ? TEXT("YES") : TEXT("NO"));
    return bHasChannel;
}

const TArray<FName>& ABaseCharacter::GetTeleportChannels() const
{
    return AllowedTeleportChannels;
}

void ABaseCharacter::ClearTeleportChannels()
{
    AllowedTeleportChannels.Empty();
    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Cleared all teleport channels"),
        *GetName());
}

void ABaseCharacter::SerializeTeleportChannelsForSave(TArray<FString>& OutChannelNames) const
{
    OutChannelNames.Empty();
    for (const FName& Channel : AllowedTeleportChannels)
    {
        OutChannelNames.Add(Channel.ToString());
    }
}

void ABaseCharacter::DeserializeTeleportChannelsFromSave(const TArray<FString>& InChannelNames)
{
    AllowedTeleportChannels.Empty();
    for (const FString& ChannelName : InChannelNames)
    {
        AllowedTeleportChannels.Add(FName(*ChannelName));
    }
}

// ============================================================================
// SPELL COLLECTION SYSTEM
// Modular design - any character type can collect spells if configured
// ============================================================================

bool ABaseCharacter::AddSpell(FName SpellTypeName)
{
    // Validate input
    if (SpellTypeName == NAME_None)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] Attempted to add invalid spell NAME_None"),
            *GetName());
        return false;
    }

    // Check if character is allowed to collect spells
    if (!bCanCollectSpells)
    {
        UE_LOG(LogBaseCharacter, Log, TEXT("[%s] Cannot collect spells (bCanCollectSpells = false)"),
            *GetName());
        return false;
    }

    // Check if already have this spell
    if (CollectedSpells.Contains(SpellTypeName))
    {
        UE_LOG(LogBaseCharacter, Log, TEXT("[%s] Already has spell '%s' - not adding duplicate"),
            *GetName(), *SpellTypeName.ToString());
        return false;
    }

    // Add the spell to our collection
    CollectedSpells.Add(SpellTypeName);

    int32 TotalSpells = CollectedSpells.Num();

    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] === SPELL ACQUIRED === '%s' | Total: %d"),
        *GetName(), *SpellTypeName.ToString(), TotalSpells);

    // Broadcast to listeners (HUD, ability system, achievements, etc.)
    // This follows Observer pattern - character doesn't know who is listening
    OnSpellCollected.Broadcast(SpellTypeName, TotalSpells);

    return true;
}

bool ABaseCharacter::RemoveSpell(FName SpellTypeName)
{
    // Validate input
    if (SpellTypeName == NAME_None)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] Attempted to remove invalid spell NAME_None"),
            *GetName());
        return false;
    }

    // Check if we have this spell
    if (!CollectedSpells.Contains(SpellTypeName))
    {
        UE_LOG(LogBaseCharacter, Log, TEXT("[%s] Does not have spell '%s' - cannot remove"),
            *GetName(), *SpellTypeName.ToString());
        return false;
    }

    // Remove the spell
    CollectedSpells.Remove(SpellTypeName);

    int32 TotalSpells = CollectedSpells.Num();

    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] === SPELL REMOVED === '%s' | Remaining: %d"),
        *GetName(), *SpellTypeName.ToString(), TotalSpells);

    // Broadcast to listeners
    OnSpellRemoved.Broadcast(SpellTypeName, TotalSpells);

    return true;
}

bool ABaseCharacter::HasSpell(FName SpellTypeName) const
{
    // NAME_None never matches any spell
    if (SpellTypeName == NAME_None)
    {
        return false;
    }

    return CollectedSpells.Contains(SpellTypeName);
}

TArray<FName> ABaseCharacter::GetCollectedSpells() const
{
    // Convert TSet to TArray for Blueprint iteration
    // TSet doesn't iterate well in Blueprints, but TArray does
    return CollectedSpells.Array();
}

int32 ABaseCharacter::GetSpellCount() const
{
    return CollectedSpells.Num();
}

void ABaseCharacter::ClearAllSpells()
{
    int32 PreviousCount = CollectedSpells.Num();

    // Store spell names before clearing for broadcast
    TArray<FName> SpellsToRemove = CollectedSpells.Array();

    CollectedSpells.Empty();

    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Cleared all spells (had %d)"),
        *GetName(), PreviousCount);

    // Broadcast removal for each spell so listeners can react
    // This is important for systems tracking individual spell states
    for (const FName& Spell : SpellsToRemove)
    {
        OnSpellRemoved.Broadcast(Spell, 0);
    }
}

bool ABaseCharacter::CanCollectSpells() const
{
    return bCanCollectSpells;
}

// ============================================================================
// TEAM INTERFACE
// Used by AI perception and friendly fire checks
// ============================================================================

FGenericTeamId ABaseCharacter::GetGenericTeamId() const
{
    return FGenericTeamId(TeamID);
}

void ABaseCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
    TeamID = NewTeamID.GetId();
    UE_LOG(LogBaseCharacter, Display, TEXT("[%s] Team changed to: %d"),
        *GetName(), TeamID);
}

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

void ABaseCharacter::DebugPrintChannels() const
{
    UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] === TELEPORT CHANNELS (%d) ==="),
        *GetName(), AllowedTeleportChannels.Num());

    for (const FName& Channel : AllowedTeleportChannels)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("  - %s"), *Channel.ToString());
    }

    if (AllowedTeleportChannels.Num() == 0)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("  (No channels - allows ALL teleports)"));
    }
}

void ABaseCharacter::DebugPrintSpells() const
{
    UE_LOG(LogBaseCharacter, Warning, TEXT("[%s] === COLLECTED SPELLS (%d) ==="),
        *GetName(), CollectedSpells.Num());

    for (const FName& Spell : CollectedSpells)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("  - %s"), *Spell.ToString());
    }

    if (CollectedSpells.Num() == 0)
    {
        UE_LOG(LogBaseCharacter, Warning, TEXT("  (No spells collected)"));
    }

    UE_LOG(LogBaseCharacter, Warning, TEXT("  Can Collect: %s"),
        bCanCollectSpells ? TEXT("YES") : TEXT("NO"));
}