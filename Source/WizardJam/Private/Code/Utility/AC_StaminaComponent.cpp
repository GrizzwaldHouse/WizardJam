// AC_StaminaComponent.cpp (UPDATED)
// Stamina component with delegate broadcasting for Observer pattern
//
// Developer: Marcus Daley
// Date: December 30, 2025 (Updated from December 21)
// Project: WizardJam

#include "Code/Utility/AC_StaminaComponent.h"
#include "GameFramework/Actor.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogStaminaComponent, Log, All);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UAC_StaminaComponent::UAC_StaminaComponent()
    : MaxStamina(100.0f)
    , CurrentStamina(100.0f)
    , StaminaRegenRate(15.0f)
    , StaminaDrainRate(20.0f)
    , RegenDelay(1.0f)
    , bIsSprinting(false)
    , RegenDelayTimer(0.0f)
    , OwnerActor(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAC_StaminaComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache owner
    OwnerActor = GetOwner();

    // Ensure valid starting stamina
    if (CurrentStamina <= 0.0f)
    {
        CurrentStamina = MaxStamina;
    }

    // Initial broadcast so HUD can initialize
    if (OwnerActor && OnStaminaChanged.IsBound())
    {
        OnStaminaChanged.Broadcast(OwnerActor, CurrentStamina, 0.0f);
    }

    UE_LOG(LogStaminaComponent, Display, TEXT("[%s] StaminaComponent ready: %.0f/%.0f | RegenRate: %.0f | DrainRate: %.0f"),
        *GetNameSafe(OwnerActor), CurrentStamina, MaxStamina, StaminaRegenRate, StaminaDrainRate);
}

void UAC_StaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsSprinting)
    {
        // Drain stamina while sprinting
        float DrainAmount = StaminaDrainRate * DeltaTime;
        ApplyStaminaChange(-DrainAmount);

        // Auto-stop sprinting when depleted
        if (CurrentStamina <= 0.0f)
        {
            bIsSprinting = false;
            UE_LOG(LogStaminaComponent, Display, TEXT("[%s] Sprint stopped - stamina depleted"),
                *GetNameSafe(OwnerActor));
        }

        // Reset regen delay timer
        RegenDelayTimer = RegenDelay;
    }
    else
    {
        // Count down regen delay
        if (RegenDelayTimer > 0.0f)
        {
            RegenDelayTimer -= DeltaTime;
        }
        else if (CurrentStamina < MaxStamina)
        {
            // Regenerate stamina after delay
            float RegenAmount = StaminaRegenRate * DeltaTime;
            ApplyStaminaChange(RegenAmount);
        }
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void UAC_StaminaComponent::Initialize(float InMaxStamina)
{
    if (InMaxStamina <= 0.0f)
    {
        UE_LOG(LogStaminaComponent, Warning, TEXT("[%s] Initialize called with invalid value %.0f, using default 100"),
            *GetNameSafe(OwnerActor), InMaxStamina);
        InMaxStamina = 100.0f;
    }

    MaxStamina = InMaxStamina;
    CurrentStamina = MaxStamina;
    RegenDelayTimer = 0.0f;

    // Broadcast initial state
    if (OwnerActor && OnStaminaChanged.IsBound())
    {
        OnStaminaChanged.Broadcast(OwnerActor, CurrentStamina, 0.0f);
    }

    UE_LOG(LogStaminaComponent, Display, TEXT("[%s] Stamina initialized: %.0f/%.0f"),
        *GetNameSafe(OwnerActor), CurrentStamina, MaxStamina);
}

void UAC_StaminaComponent::SetSprinting(bool bNewSprinting)
{
    // Can only sprint if we have stamina
    if (bNewSprinting && !CanSprint())
    {
        bNewSprinting = false;
    }

    if (bIsSprinting != bNewSprinting)
    {
        bIsSprinting = bNewSprinting;

        if (bIsSprinting)
        {
            UE_LOG(LogStaminaComponent, Verbose, TEXT("[%s] Sprint STARTED"),
                *GetNameSafe(OwnerActor));
        }
        else
        {
            UE_LOG(LogStaminaComponent, Verbose, TEXT("[%s] Sprint STOPPED"),
                *GetNameSafe(OwnerActor));
        }
    }
}

bool UAC_StaminaComponent::CanSprint() const
{
    return CurrentStamina > 0.0f;
}

bool UAC_StaminaComponent::ConsumeStamina(float Amount)
{
    if (Amount <= 0.0f)
    {
        return true; // Nothing to consume
    }

    if (CurrentStamina < Amount)
    {
        UE_LOG(LogStaminaComponent, Verbose, TEXT("[%s] Cannot consume %.0f stamina - only %.0f available"),
            *GetNameSafe(OwnerActor), Amount, CurrentStamina);
        return false;
    }

    ApplyStaminaChange(-Amount);
    RegenDelayTimer = RegenDelay; // Reset regen delay

    UE_LOG(LogStaminaComponent, Display, TEXT("[%s] Consumed %.0f stamina | Remaining: %.0f/%.0f"),
        *GetNameSafe(OwnerActor), Amount, CurrentStamina, MaxStamina);

    return true;
}

float UAC_StaminaComponent::RestoreStamina(float Amount)
{
    if (Amount <= 0.0f)
    {
        return 0.0f;
    }

    float OldStamina = CurrentStamina;
    ApplyStaminaChange(Amount);
    float ActualRestored = CurrentStamina - OldStamina;

    if (ActualRestored > 0.0f)
    {
        UE_LOG(LogStaminaComponent, Display, TEXT("[%s] Restored %.0f stamina | Current: %.0f/%.0f"),
            *GetNameSafe(OwnerActor), ActualRestored, CurrentStamina, MaxStamina);
    }

    return ActualRestored;
}

float UAC_StaminaComponent::GetStaminaPercent() const
{
    return (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UAC_StaminaComponent::ApplyStaminaChange(float Delta)
{
    if (FMath::IsNearlyZero(Delta))
    {
        return;
    }

    float OldStamina = CurrentStamina;
    CurrentStamina = FMath::Clamp(CurrentStamina + Delta, 0.0f, MaxStamina);
    float ActualDelta = CurrentStamina - OldStamina;

    // Only broadcast if there was actual change
    if (!FMath::IsNearlyZero(ActualDelta))
    {
        // Broadcast to HUD and other listeners
        if (OwnerActor && OnStaminaChanged.IsBound())
        {
            OnStaminaChanged.Broadcast(OwnerActor, CurrentStamina, ActualDelta);
        }

        // Check threshold events
        CheckThresholds(OldStamina, CurrentStamina);
    }
}

void UAC_StaminaComponent::CheckThresholds(float OldStamina, float NewStamina)
{
    // Check for depletion
    if (OldStamina > 0.0f && NewStamina <= 0.0f)
    {
        if (OwnerActor && OnStaminaDepleted.IsBound())
        {
            OnStaminaDepleted.Broadcast(OwnerActor);
        }
        UE_LOG(LogStaminaComponent, Warning, TEXT("[%s] Stamina DEPLETED!"),
            *GetNameSafe(OwnerActor));
    }

    // Check for full restoration
    if (OldStamina < MaxStamina && NewStamina >= MaxStamina)
    {
        if (OwnerActor && OnStaminaRestored.IsBound())
        {
            OnStaminaRestored.Broadcast(OwnerActor);
        }
        UE_LOG(LogStaminaComponent, Display, TEXT("[%s] Stamina FULL!"),
            *GetNameSafe(OwnerActor));
    }
}