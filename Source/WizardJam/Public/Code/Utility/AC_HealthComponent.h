// AC_HealthComponent.h
// Component managing actor health, damage, healing, and death
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam
// Source: Adapted from Island Escape project
//
// PURPOSE:
// Centralizes health management with delegate-based event broadcasting.
// All damage goes through ApplyDamage() which broadcasts to HUD, AI, and other systems.
//
// USAGE:
// 1. Add component to any actor that needs health
// 2. Call Initialize(MaxHealth) in BeginPlay or use EditDefaultsOnly MaxHealth
// 3. Bind to OnHealthChanged for HUD updates
// 4. Bind to OnDeath for death handling

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "AC_HealthComponent.generated.h"

class AActor;
class AController;

// Delegate for health changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnHealthChanged,
    AActor*, Owner,
    float, NewHealth,
    float, Delta
);

// Delegate for death broadcasts owner and killer
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnDeath,
    AActor*, Owner,
    AActor*, Killer
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WIZARDJAM_API UAC_HealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAC_HealthComponent();

    // Initialize health component with max health value
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Initialize(float InMaxHealth);

    // Apply damage to this component (positive values reduce health)
    UFUNCTION(BlueprintCallable, Category = "Health")
    float ApplyDamage(float DamageAmount, AActor* DamageCauser = nullptr);

    // Heal this component (positive values restore health)
    UFUNCTION(BlueprintCallable, Category = "Health")
    float Heal(float HealAmount);

    // Check if actor is alive
    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsAlive() const;

    // Getters
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetCurrentHealth() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetHealthPercent() const;

    // Delegates - bind to these for health updates
    UPROPERTY(BlueprintAssignable, Category = "Health|Events")
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health|Events")
    FOnDeath OnDeath;

protected:
    virtual void BeginPlay() override;

    // Handle damage via Actor::TakeDamage system
    UFUNCTION()
    void HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
        const class UDamageType* DamageType, class AController* InstigatedBy,
        AActor* DamageCauser);

    // Max health 
    UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = "0.0"))
    float MaxHealth;

    // Current health - visible but not editable
    UPROPERTY(VisibleAnywhere, Category = "Health")
    float CurrentHealth;

private:
    UPROPERTY()
    AActor* OwnerActor;

    bool bIsInitialized;
};