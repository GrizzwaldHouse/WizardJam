// ============================================================================
// BaseProjectile.h
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// Purpose:
// Base projectile class for all spell projectiles in WizardJam.
// Supports color-based and particle-based visual feedback.
//
// Designer Usage:
// 1. Create Blueprint child (BP_Projectile_Flame, etc.)
// 2. Set SpellElement to match spell name
// 3. Set ElementColor for mesh tint
// 4. Optionally assign TrailNiagaraSystem and ImpactNiagaraSystem
// 5. Configure Damage, InitialSpeed, LifetimeSeconds
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "BaseProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UParticleSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogBaseProjectile, Log, All);

// Delegate for projectile hit events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnProjectileHit,
    ABaseProjectile*, Projectile,
    AActor*, HitActor,
    const FHitResult&, HitResult
);

// Delegate for projectile destruction
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnProjectileDestroyed,
    ABaseProjectile*, Projectile,
    bool, bHitSomething
);

UCLASS()
class WIZARDJAM_API ABaseProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABaseProjectile();

    // Initialize projectile after spawning
    UFUNCTION(BlueprintCallable, Category = "Projectile|Setup")
    void InitializeProjectile(AActor* OwningActor, const FVector& LaunchDirection);

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
    FOnProjectileHit OnProjectileHit;

    UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
    FOnProjectileDestroyed OnProjectileDestroyed;

    // Accessors
    UFUNCTION(BlueprintPure, Category = "Projectile|Config")
    FName GetSpellElement() const { return SpellElement; }

    UFUNCTION(BlueprintPure, Category = "Projectile|Config")
    float GetDamage() const { return Damage; }

    UFUNCTION(BlueprintPure, Category = "Projectile|Config")
    FLinearColor GetElementColor() const { return ElementColor; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProjectileMovementComponent* ProjectileMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* TrailNiagaraComponent;

    // Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Identity")
    FName SpellElement;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
    FLinearColor ElementColor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Combat",
        meta = (ClampMin = "0.0"))
    float Damage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement",
        meta = (ClampMin = "0.0"))
    float InitialSpeed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement",
        meta = (ClampMin = "0.1"))
    float LifetimeSeconds;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Collision",
        meta = (ClampMin = "1.0"))
    float CollisionRadius;

    // Niagara effects
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects|Niagara")
    UNiagaraSystem* TrailNiagaraSystem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects|Niagara")
    UNiagaraSystem* ImpactNiagaraSystem;

    // Cascade fallback
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects|Cascade")
    UParticleSystem* TrailCascadeSystem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects|Cascade")
    UParticleSystem* ImpactCascadeSystem;

    // Hit handling
    UFUNCTION()
    void OnOverlapBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION(BlueprintNativeEvent, Category = "Projectile|Combat")
    void HandleHit(AActor* HitActor, const FHitResult& HitResult);
    virtual void HandleHit_Implementation(AActor* HitActor, const FHitResult& HitResult);

    UFUNCTION(BlueprintNativeEvent, Category = "Projectile|Combat")
    void ApplyDamage(AActor* HitActor, const FHitResult& HitResult);
    virtual void ApplyDamage_Implementation(AActor* HitActor, const FHitResult& HitResult);

    // Effect helpers
    void InitializeTrailEffect();
    void SpawnImpactEffect(const FVector& Location, const FVector& Normal);
    void ApplyMaterialColor();

    // Team checking
    bool IsFriendlyActor(AActor* OtherActor) const;

    // Cached references
    UPROPERTY()
    TWeakObjectPtr<AActor> CachedOwner;

    UPROPERTY()
    TWeakObjectPtr<APawn> CachedInstigator;

    bool bDidHitSomething;

private:
    FTimerHandle LifetimeTimerHandle;
    void OnLifetimeExpired();
};