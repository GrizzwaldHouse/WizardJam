// ============================================================================
// BasePlayer.cpp
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of the wizard player character with modular combat system
// and spell switching functionality.
//
// Spell Switching Implementation:
// - SpellOrder array defines the cycling sequence (matches HUD slots)
// - Mouse wheel calls CycleToNextSpell() / CycleToPreviousSpell()
// - Number keys call SetEquippedSpellByIndex()
// - OnEquippedSpellChanged delegate notifies HUD
// - Only unlocked spells can be equipped
// ============================================================================

#include "Code/Actors/BasePlayer.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Code/Utility/AC_AimComponent.h"
#include "Code/Utility/AC_CombatComponent.h"
#include "Code/Actors/BaseProjectile.h"
#include "Code/Utility/AC_HealthComponent.h"
#include "Code/Utility/AC_StaminaComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogBasePlayer);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ABasePlayer::ABasePlayer()
    : EquippedSpellType(NAME_None)
    , bAutoEquipFirstSpell(true)
    , bSyncSpellChannelsToTeleport(true)
{
    // Create spell collection component
    SpellCollectionComponent = CreateDefaultSubobject<UAC_SpellCollectionComponent>(
        TEXT("SpellCollectionComponent"));

    // Create aim component - handles raycast aiming
    AimComponent = CreateDefaultSubobject<UAC_AimComponent>(TEXT("AimComponent"));

    // Create combat component - handles projectile spawning
    CombatComponent = CreateDefaultSubobject<UAC_CombatComponent>(TEXT("CombatComponent"));

    // Set team ID to 0 for player (friendly)
    SetGenericTeamId(FGenericTeamId(0));

    UE_LOG(LogBasePlayer, Log, TEXT("BasePlayer constructed with combat components"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABasePlayer::BeginPlay()
{
    Super::BeginPlay();

    // Initialize default spell order if not configured in Blueprint
    InitializeDefaultSpellOrder();

    // ========================================================================
    // SPELL COLLECTION BINDINGS
    // ========================================================================

    if (SpellCollectionComponent)
    {
        if (bSyncSpellChannelsToTeleport)
        {
            SpellCollectionComponent->OnChannelAdded.AddDynamic(
                this, &ABasePlayer::OnSpellChannelAdded);
            SpellCollectionComponent->OnChannelRemoved.AddDynamic(
                this, &ABasePlayer::OnSpellChannelRemoved);

            UE_LOG(LogBasePlayer, Display,
                TEXT("[%s] Hybrid bridge ENABLED - spell channels sync to teleport"),
                *GetName());
        }

        SpellCollectionComponent->OnSpellAdded.AddDynamic(
            this, &ABasePlayer::HandleSpellAdded);
    }
    else
    {
        UE_LOG(LogBasePlayer, Error,
            TEXT("[%s] SpellCollectionComponent is null!"),
            *GetName());
    }

    // ========================================================================
    // COMBAT COMPONENT BINDINGS
    // ========================================================================

    if (CombatComponent)
    {
        CombatComponent->OnProjectileFired.AddDynamic(
            this, &ABasePlayer::HandleProjectileFired);
        CombatComponent->OnFireBlocked.AddDynamic(
            this, &ABasePlayer::HandleFireBlocked);

        UE_LOG(LogBasePlayer, Display,
            TEXT("[%s] Bound to CombatComponent"),
            *GetName());
    }
    else
    {
        UE_LOG(LogBasePlayer, Error,
            TEXT("[%s] CombatComponent is null!"),
            *GetName());
    }

    // ========================================================================
    // AIM COMPONENT BINDINGS
    // ========================================================================

    if (AimComponent)
    {
        AimComponent->OnAimTargetChanged.AddDynamic(
            this, &ABasePlayer::HandleAimTargetChanged);

        UE_LOG(LogBasePlayer, Display,
            TEXT("[%s] Bound to AimComponent"),
            *GetName());
    }
    else
    {
        UE_LOG(LogBasePlayer, Error,
            TEXT("[%s] AimComponent is null!"),
            *GetName());
    }

    // ========================================================================
    // INITIAL SPELL EQUIPMENT
    // ========================================================================

    // If player has starting spells, equip the first one
    if (SpellCollectionComponent && bAutoEquipFirstSpell)
    {
        TArray<FName> StartingSpells = SpellCollectionComponent->GetAllSpells();
        if (StartingSpells.Num() > 0)
        {
            // Find first spell that's in our spell order
            for (const FName& SpellName : SpellOrder)
            {
                if (StartingSpells.Contains(SpellName))
                {
                    SetEquippedSpell(SpellName);
                    break;
                }
            }
        }
    }

    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] WizardPlayer ready | TeamID: %d | Equipped: %s | SpellOrder: %d types"),
        *GetName(),
        GetGenericTeamId().GetId(),
        *EquippedSpellType.ToString(),
        SpellOrder.Num());
}

void ABasePlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up delegate bindings to prevent crashes
    if (SpellCollectionComponent)
    {
        SpellCollectionComponent->OnChannelAdded.RemoveAll(this);
        SpellCollectionComponent->OnChannelRemoved.RemoveAll(this);
        SpellCollectionComponent->OnSpellAdded.RemoveAll(this);
    }

    if (CombatComponent)
    {
        CombatComponent->OnProjectileFired.RemoveAll(this);
        CombatComponent->OnFireBlocked.RemoveAll(this);
    }

    if (AimComponent)
    {
        AimComponent->OnAimTargetChanged.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// INPUT HANDLERS
// ============================================================================

void ABasePlayer::HandleFireInput()
{
    UE_LOG(LogBasePlayer, Verbose, 
        TEXT("[%s] === FIRE INPUT RECEIVED === Equipped: '%s'"),
        *GetName(), *EquippedSpellType.ToString());

    if (!CombatComponent)
    {
        UE_LOG(LogBasePlayer, Error,
            TEXT("[%s] Cannot fire - CombatComponent is NULL!"),
            *GetName());
        return;
    }

    // Check if we have a spell equipped
    if (EquippedSpellType == NAME_None)
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] Cannot fire - no spell equipped!"),
            *GetName());
        return;
    }

    // Check if we actually have this spell
    if (!CombatComponent->CanFireType(EquippedSpellType))
    {
       
        return;
    }
    // Fire the spell
    CombatComponent->FireProjectileByType(EquippedSpellType);
}

void ABasePlayer::HandleCycleSpellInput(const FInputActionValue& Value)
{
    float ScrollValue = Value.Get<float>();

    UE_LOG(LogBasePlayer, Verbose,
        TEXT("[%s] Cycle spell input | Value: %.2f"),
        *GetName(), ScrollValue);

    if (ScrollValue > 0.0f)
    {
        CycleToNextSpell();
    }
    else if (ScrollValue < 0.0f)
    {
        CycleToPreviousSpell();
    }
}

void ABasePlayer::HandleSelectSpellSlot(int32 SlotIndex)
{
    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] Select spell slot %d"),
        *GetName(), SlotIndex);

    SetEquippedSpellByIndex(SlotIndex);
}

// ============================================================================
// SPELL CONTROL FUNCTIONS
// ============================================================================

bool ABasePlayer::SetEquippedSpell(FName SpellType)
{
    // Don't allow equipping spells we don't have
    if (!HasSpell(SpellType))
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] Cannot equip '%s' - not unlocked"),
            *GetName(), *SpellType.ToString());
        return false;
    }

    // Skip if already equipped
    if (EquippedSpellType == SpellType)
    {
        return true;
    }

    FName OldSpell = EquippedSpellType;
    EquippedSpellType = SpellType;

    int32 SlotIndex = FindSpellIndex(SpellType);

    // Broadcast change for HUD
    OnEquippedSpellChanged.Broadcast(EquippedSpellType, SlotIndex);

    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] === SPELL EQUIPPED === '%s' (Slot %d) | Previous: '%s'"),
        *GetName(), *SpellType.ToString(), SlotIndex, *OldSpell.ToString());

    return true;
}

bool ABasePlayer::SetEquippedSpellByIndex(int32 SlotIndex)
{
    // Validate index
    if (!SpellOrder.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] Invalid spell slot index: %d (valid: 0-%d)"),
            *GetName(), SlotIndex, SpellOrder.Num() - 1);
        return false;
    }

    FName SpellType = SpellOrder[SlotIndex];
    return SetEquippedSpell(SpellType);
}

void ABasePlayer::CycleToNextSpell()
{
    if (SpellOrder.Num() == 0)
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] Cannot cycle - SpellOrder is empty"),
            *GetName());
        return;
    }

    int32 CurrentIndex = FindSpellIndex(EquippedSpellType);
    int32 NextIndex = FindNextUnlockedSpellIndex(CurrentIndex, true);

    if (NextIndex != INDEX_NONE)
    {
        SetEquippedSpellByIndex(NextIndex);
    }
    else
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] No unlocked spells to cycle to"),
            *GetName());
    }
}

void ABasePlayer::CycleToPreviousSpell()
{
    if (SpellOrder.Num() == 0)
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] Cannot cycle - SpellOrder is empty"),
            *GetName());
        return;
    }

    int32 CurrentIndex = FindSpellIndex(EquippedSpellType);
    int32 PrevIndex = FindNextUnlockedSpellIndex(CurrentIndex, false);

    if (PrevIndex != INDEX_NONE)
    {
        SetEquippedSpellByIndex(PrevIndex);
    }
    else
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[%s] No unlocked spells to cycle to"),
            *GetName());
    }
}

// ============================================================================
// SPELL QUERY FUNCTIONS
// ============================================================================

int32 ABasePlayer::GetEquippedSpellIndex() const
{
    return FindSpellIndex(EquippedSpellType);
}

bool ABasePlayer::HasSpell(FName SpellType) const
{
    if (!SpellCollectionComponent)
    {
        return false;
    }
    return SpellCollectionComponent->HasSpell(SpellType);
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

int32 ABasePlayer::FindSpellIndex(FName SpellType) const
{
    return SpellOrder.IndexOfByKey(SpellType);
}

int32 ABasePlayer::FindNextUnlockedSpellIndex(int32 StartIndex, bool bForward) const
{
    if (SpellOrder.Num() == 0)
    {
        return INDEX_NONE;
    }

    // Handle invalid start index
    if (StartIndex == INDEX_NONE)
    {
        StartIndex = bForward ? -1 : SpellOrder.Num();
    }

    int32 CheckedCount = 0;
    int32 CurrentIndex = StartIndex;

    while (CheckedCount < SpellOrder.Num())
    {
        // Move to next/previous index with wrapping
        if (bForward)
        {
            CurrentIndex = (CurrentIndex + 1) % SpellOrder.Num();
        }
        else
        {
            CurrentIndex = (CurrentIndex - 1 + SpellOrder.Num()) % SpellOrder.Num();
        }

        // Check if this spell is unlocked
        FName SpellType = SpellOrder[CurrentIndex];
        if (HasSpell(SpellType))
        {
            return CurrentIndex;
        }

        CheckedCount++;
    }

    return INDEX_NONE;
}

void ABasePlayer::InitializeDefaultSpellOrder()
{
    // Only initialize if designer didn't configure
    if (SpellOrder.Num() == 0)
    {
        SpellOrder.Add(FName("Flame"));
        SpellOrder.Add(FName("Ice"));
        SpellOrder.Add(FName("Lightning"));
        SpellOrder.Add(FName("Arcane"));

        UE_LOG(LogBasePlayer, Display,
            TEXT("[%s] Initialized default SpellOrder: Flame, Ice, Lightning, Arcane"),
            *GetName());
    }
}

// ============================================================================
// ISpellCollector Interface
// ============================================================================

UAC_SpellCollectionComponent* ABasePlayer::GetSpellCollectionComponent_Implementation() const
{
    return SpellCollectionComponent;
}

int32 ABasePlayer::GetCollectorTeamID_Implementation() const
{
    return 0;
}

void ABasePlayer::OnSpellCollected_Implementation(FName SpellType)
{
    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] SPELL COLLECTED: '%s'"),
        *GetName(), *SpellType.ToString());

    // Auto-equip if no spell currently equipped
    if (bAutoEquipFirstSpell && EquippedSpellType == NAME_None)
    {
        SetEquippedSpell(SpellType);
    }
}

void ABasePlayer::OnSpellCollectionDenied_Implementation(FName SpellType, const FString& Reason)
{
    UE_LOG(LogBasePlayer, Warning,
        TEXT("[%s] Spell DENIED | Type: '%s' | Reason: %s"),
        *GetName(), *SpellType.ToString(), *Reason);
}

// ============================================================================
// HYBRID BRIDGE HANDLERS
// ============================================================================

void ABasePlayer::OnSpellChannelAdded(FName Channel)
{
    AddTeleportChannel(Channel);

    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] HYBRID SYNC: Channel '%s' -> spell + teleport"),
        *GetName(), *Channel.ToString());
}

void ABasePlayer::OnSpellChannelRemoved(FName Channel)
{
    RemoveTeleportChannel(Channel);

    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] HYBRID SYNC: Channel '%s' removed"),
        *GetName(), *Channel.ToString());
}

// ============================================================================
// COMPONENT EVENT HANDLERS
// ============================================================================

void ABasePlayer::HandleSpellAdded(FName SpellType, int32 TotalCount)
{
    OnPlayerSpellCollected.Broadcast(SpellType);

    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] Total spells: %d"),
        *GetName(), TotalCount);
}

void ABasePlayer::HandleProjectileFired(ABaseProjectile* Projectile, FName ProjectileType, FVector FireDirection)
{
    UE_LOG(LogBasePlayer, Display,
        TEXT("[%s] === PROJECTILE FIRED === Spell: '%s' | Direction: %s"),
        *GetName(), *ProjectileType.ToString(), *FireDirection.ToString());
}

void ABasePlayer::HandleFireBlocked(EFireBlockedReason Reason, FName AttemptedType)
{
    UE_LOG(LogBasePlayer, Verbose, 
        TEXT("[%s] Fire blocked | Reason: %d | Attempted: '%s'"),
        *GetName(), static_cast<int32>(Reason), *AttemptedType.ToString());
}

void ABasePlayer::HandleAimTargetChanged(AActor* NewTarget, EAimTraceResult TargetType)
{
    UE_LOG(LogBasePlayer, Verbose,
        TEXT("[%s] Aim target: %s | Type: %d"),
        *GetName(),
        NewTarget ? *NewTarget->GetName() : TEXT("None"),
        static_cast<int32>(TargetType));
}

// ============================================================================
// DEBUG CONSOLE COMMANDS
// ============================================================================

void ABasePlayer::Debug_TakeDamage(float Amount)
{
    UAC_HealthComponent* HC = GetHealthComponent();
    if (!HC)
    {
        UE_LOG(LogBasePlayer, Warning, TEXT("[DEBUG] No HealthComponent"));
        return;
    }

    HC->ApplyDamage(Amount, nullptr);
    UE_LOG(LogBasePlayer, Display,
        TEXT("[DEBUG] Damage: %.0f | Health: %.0f / %.0f"),
        Amount, HC->GetCurrentHealth(), HC->GetMaxHealth());
}

void ABasePlayer::Debug_Heal(float Amount)
{
    UAC_HealthComponent* HC = GetHealthComponent();
    if (!HC)
    {
        UE_LOG(LogBasePlayer, Warning, TEXT("[DEBUG] No HealthComponent"));
        return;
    }

    HC->Heal(Amount);
    UE_LOG(LogBasePlayer, Display,
        TEXT("[DEBUG] Healed: %.0f | Health: %.0f / %.0f"),
        Amount, HC->GetCurrentHealth(), HC->GetMaxHealth());
}

void ABasePlayer::Debug_DrainStamina(float Amount)
{
    UAC_StaminaComponent* SC = GetStaminaComponent();
    if (!SC)
    {
        UE_LOG(LogBasePlayer, Warning, TEXT("[DEBUG] No StaminaComponent"));
        return;
    }

    SC->ConsumeStamina(Amount);
    UE_LOG(LogBasePlayer, Display,
        TEXT("[DEBUG] Drained: %.0f | Stamina: %.0f / %.0f"),
        Amount, SC->GetCurrentStamina(), SC->GetMaxStamina());
}

void ABasePlayer::Debug_RestoreStamina(float Amount)
{
    UAC_StaminaComponent* SC = GetStaminaComponent();
    if (!SC)
    {
        UE_LOG(LogBasePlayer, Warning, TEXT("[DEBUG] No StaminaComponent"));
        return;
    }

    SC->RestoreStamina(Amount);
    UE_LOG(LogBasePlayer, Display,
        TEXT("[DEBUG] Restored: %.0f | Stamina: %.0f / %.0f"),
        Amount, SC->GetCurrentStamina(), SC->GetMaxStamina());
}

void ABasePlayer::Debug_AddSpell(FName SpellType)
{
    if (!SpellCollectionComponent)
    {
        UE_LOG(LogBasePlayer, Warning, TEXT("[DEBUG] No SpellCollectionComponent"));
        return;
    }

    SpellCollectionComponent->AddSpell(SpellType);
    UE_LOG(LogBasePlayer, Display,
        TEXT("[DEBUG] Added spell: %s"),
        *SpellType.ToString());
}

void ABasePlayer::Debug_SwitchSpell(FName SpellType)
{
    if (SetEquippedSpell(SpellType))
    {
        UE_LOG(LogBasePlayer, Display,
            TEXT("[DEBUG] Switched to: %s"),
            *SpellType.ToString());
    }
    else
    {
        UE_LOG(LogBasePlayer, Warning,
            TEXT("[DEBUG] Failed to switch to: %s (not unlocked)"),
            *SpellType.ToString());
    }
}

void ABasePlayer::Debug_ListSpells()
{
    UE_LOG(LogBasePlayer, Warning, TEXT("========== SPELL STATUS =========="));
    UE_LOG(LogBasePlayer, Warning, TEXT("Equipped: %s"), *EquippedSpellType.ToString());
    UE_LOG(LogBasePlayer, Warning, TEXT("SpellOrder:"));
    
    for (int32 i = 0; i < SpellOrder.Num(); i++)
    {
        bool bUnlocked = HasSpell(SpellOrder[i]);
        bool bEquipped = (SpellOrder[i] == EquippedSpellType);
        
        UE_LOG(LogBasePlayer, Warning,
            TEXT("  [%d] %s - %s%s"),
            i,
            *SpellOrder[i].ToString(),
            bUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"),
            bEquipped ? TEXT(" <-- EQUIPPED") : TEXT(""));
    }
    UE_LOG(LogBasePlayer, Warning, TEXT("==================================="));
}