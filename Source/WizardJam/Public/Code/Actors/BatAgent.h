// BatAgent.h - Flying Bat Enemy
//
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
//
// PURPOSE:
// Flying enemy that inherits from BaseAgent and adds:
// - Flying movement mode
// - Projectile attacks from mouth socket
// - Simple chase-and-attack AI (optional - can use behavior tree instead)
//
// ARCHITECTURE:
// This is the BODY - it executes commands without making strategic decisions.
// The AI Controller (brain) decides WHEN to attack, this class executes HOW to attack.
// Health/death handling is inherited from BaseAgent - do NOT duplicate handlers.
//
// USAGE:
// 1. Create Blueprint subclass (BP_BatAgent or BP_WizardBoss)
// 2. Configure mesh, animations, projectile class in Blueprint
// 3. Assign AI Controller (BP_BatAIController or BP_BossAIController)
// 4. Place in level or spawn at runtime via Spawner

#pragma once

#include "CoreMinimal.h"
#include "Code/Actors/BaseAgent.h"
#include "BatAgent.generated.h"

// Forward declarations
class ABaseProjectile;
class USoundBase;
class UParticleSystem;

UCLASS()
class WIZARDJAM_API ABatAgent : public ABaseAgent
{
    GENERATED_BODY()

public:
    ABatAgent();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // ========================================================================
    // IENEMYINTERFACE OVERRIDE (PROJECTILE ATTACK)
    // ========================================================================

    // Attack using projectile spawned from mouth
    // Overrides BaseAgent's melee attack for ranged combat
    virtual bool Attack_Implementation(AActor* Target) override;

protected:
    // ========================================================================
    // PROJECTILE CONFIGURATION
    // ========================================================================

    // Projectile class to spawn (MUST be set in Blueprint)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
    TSubclassOf<ABaseProjectile> ProjectileClass;

    // Speed of spawned projectiles
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "0.0"))
    float ProjectileSpeed;

    // Socket name on skeletal mesh where projectiles spawn (usually mouth)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
    FName MuzzleSocketName;

    // ========================================================================
    // SIMPLE AI CONFIGURATION (OPTIONAL)
    // Disable if using behavior tree for production AI
    // ========================================================================

    // Enable simple tick-based AI? (disable if using behavior tree)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Simple")
    bool bUseSimpleAI;

    // Distance at which bat stops chasing and starts attacking
    float AttackRange;

    // How fast bat flies when chasing player
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Simple", meta = (ClampMin = "0.0"))
    float FlySpeed;

private:
    // ========================================================================
    // SIMPLE AI IMPLEMENTATION
    // ========================================================================

    // Simple tick-based AI for quick testing
    // Can be disabled in favor of behavior tree for production
    void SimpleAI_ChaseAndAttack(float DeltaTime);

    // Find the player in the world
    // Returns: Player character, or nullptr if not found
    AActor* FindPlayer();

    // Check if target is within attack range
    bool IsInAttackRange(AActor* Target) const;

    // ========================================================================
    // PROJECTILE SPAWNING
    // ========================================================================

    // Spawn projectile at mouth socket aimed at target
    // Target - Actor to aim projectile at
    // Returns: Spawned projectile, or nullptr if spawn failed
    ABaseProjectile* SpawnProjectileAtMouth(AActor* Target);

    // Get world location of mouth socket
    // Returns: Socket location, or actor location if socket not found
    FVector GetMouthLocation() const;

    // Get world rotation of mouth socket
    // Returns: Socket rotation, or actor rotation if socket not found
    FRotator GetMouthRotation() const;
};