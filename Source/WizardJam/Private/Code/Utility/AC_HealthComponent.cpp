// AC_HealthComponent.cpp
// Health component implementation with delegate broadcasting
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam

#include "Code/Utility/AC_HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHealthComponent, Log, All);

UAC_HealthComponent::UAC_HealthComponent()
    : MaxHealth(100.0f)
    , CurrentHealth(0.0f)
    , OwnerActor(nullptr)
    , bIsInitialized(false)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UAC_HealthComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        UE_LOG(LogHealthComponent, Error, TEXT("HealthComponent has no owner actor!"));
        return;
    }

    // Bind to owner's TakeDamage event
    OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UAC_HealthComponent::HandleTakeAnyDamage);

    // Auto-initialize if not already initialized
    if (!bIsInitialized)
    {
        Initialize(MaxHealth);
    }

    UE_LOG(LogHealthComponent, Display, TEXT("[%s] HealthComponent initialized: %.0f/%.0f HP"),
        *OwnerActor->GetName(), CurrentHealth, MaxHealth);
}

void UAC_HealthComponent::Initialize(float InMaxHealth)
{
    if (InMaxHealth <= 0.0f)
    {
        UE_LOG(LogHealthComponent, Warning, TEXT("Initialize called with invalid MaxHealth: %.0f, using default"),
            InMaxHealth);
        InMaxHealth = 100.0f;
    }

    MaxHealth = InMaxHealth;
    CurrentHealth = MaxHealth;
    bIsInitialized = true;

    if (OwnerActor && OnHealthChanged.IsBound())
    {
        OnHealthChanged.Broadcast(OwnerActor, CurrentHealth, 0.0f);
    }
}

float UAC_HealthComponent::ApplyDamage(float DamageAmount, AActor* DamageCauser)
{
    if (DamageAmount <= 0.0f || !IsAlive())
    {
        return 0.0f;
    }

    float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
    float ActualDamage = OldHealth - CurrentHealth;

    if (OwnerActor && OnHealthChanged.IsBound())
    {
        OnHealthChanged.Broadcast(OwnerActor, CurrentHealth, -ActualDamage);
    }

    UE_LOG(LogHealthComponent, Display, TEXT("[%s] Took %.1f damage | HP: %.1f/%.1f"),
        *GetNameSafe(OwnerActor), ActualDamage, CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.0f)
    {
        UE_LOG(LogHealthComponent, Warning, TEXT("[%s] has died!"), *GetNameSafe(OwnerActor));
        if (OnDeath.IsBound())
        {
            OnDeath.Broadcast(OwnerActor, DamageCauser);
        }
    }

    return ActualDamage;
}

float UAC_HealthComponent::Heal(float HealAmount)
{
    if (HealAmount <= 0.0f || !IsAlive())
    {
        return 0.0f;
    }

    float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, MaxHealth);
    float ActualHealing = CurrentHealth - OldHealth;

    if (OwnerActor && OnHealthChanged.IsBound())
    {
        OnHealthChanged.Broadcast(OwnerActor, CurrentHealth, ActualHealing);
    }

    UE_LOG(LogHealthComponent, Display, TEXT("[%s] Healed %.1f HP | HP: %.1f/%.1f"),
        *GetNameSafe(OwnerActor), ActualHealing, CurrentHealth, MaxHealth);

    return ActualHealing;
}

void UAC_HealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
    const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    if (Damage == 0.0f) return;

    if (Damage > 0.0f)
    {
        ApplyDamage(Damage, DamageCauser);
    }
    else
    {
        Heal(FMath::Abs(Damage));
    }
}

bool UAC_HealthComponent::IsAlive() const
{
    return CurrentHealth > 0.0f;
}

float UAC_HealthComponent::GetCurrentHealth() const
{
    return CurrentHealth;
}

float UAC_HealthComponent::GetMaxHealth() const
{
    return MaxHealth;
}

float UAC_HealthComponent::GetHealthPercent() const
{
    return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}