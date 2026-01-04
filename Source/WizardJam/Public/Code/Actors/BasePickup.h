// BasePickup.h
// Base class for all pickup items in WizardJam
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam
// Source: Adapted from Island Escape project
//
// PURPOSE:
// Abstract base for pickups. Override HandlePickup() in children:
// - HealthPickup: Restores health
// - SpellCollectible: Grants spell channels (Day 2)
// - DamagePickup: Environmental hazard
//
// USAGE:
// 1. Create child class (e.g., ASpellCollectible)
// 2. Override HandlePickup() for specific behavior
// 3. Override PostPickup() if item shouldn't be destroyed

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BasePickup.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;

UCLASS(Abstract)
class WIZARDJAM_API ABasePickup : public AActor
{
    GENERATED_BODY()

public:
    ABasePickup();

    UFUNCTION(BlueprintPure, Category = "Components")
    UBoxComponent* GetCollisionBox() const;

    UFUNCTION(BlueprintPure, Category = "Components")
    UStaticMeshComponent* GetMeshComponent() const;

protected:
    virtual void BeginPlay() override;

    // Override in child classes to check pickup validity
    UFUNCTION(BlueprintPure, Category = "Pickup")
    virtual bool CanBePickedUp(AActor* OtherActor) const;

    // Override in child classes for specific pickup behavior
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    virtual void HandlePickup(AActor* OtherActor);

    // Override to prevent destruction or add effects
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    virtual void PostPickup();

    // Visual configuration
    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    bool bUseMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    UMaterialInterface* PickupMaterial;

private:
    UPROPERTY(EditInstanceOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UBoxComponent* CollisionBox;

    UPROPERTY(VisibleDefaultsOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* MeshComponent;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    void ApplyMaterialToAllSlots();
};