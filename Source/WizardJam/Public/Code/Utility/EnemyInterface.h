// EnemyInterface.h - Interface for enemy behavior
// 
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
//
// PURPOSE:
// Defines the contract that all enemy AI agents must fulfill.
// This interface ensures the Brain (AI Controller) can command
// the Body (Agent Actor) without knowing concrete implementation.
//
// ARCHITECTURE:
// The AI Controller (brain) decides WHEN to act based on perception,
// blackboard state, and behavior trees. This interface defines
// the commands the brain can issue to the body.
//
// The Actor (body) implements HOW to execute each command:
// - BaseAgent: Melee attack (look at target, play animation, apply damage)
// - BatAgent: Ranged attack (spawn projectile at mouth, aim at target)
// - Future: MageEnemy, BossWizard, etc.
//
// USAGE:
// 1. Enemy actors inherit from IEnemyInterface
// 2. AI Controller calls interface functions via Cast
// 3. Each actor implementation handles the specifics
//
// EXAMPLE:
// In AI Controller:
//   if (IEnemyInterface* Enemy = Cast<IEnemyInterface>(GetPawn()))
//   {
//       Enemy->Execute_Attack(GetPawn(), Target);
//   }

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UEnemyInterface : public UInterface
{
    GENERATED_BODY()
};

// ============================================================================
// ENEMY INTERFACE
// Commands that AI Controllers can issue to enemy actors
// ============================================================================

class WIZARDJAM_API IEnemyInterface
{
    GENERATED_BODY()

public:
    // ========================================================================
    // COMBAT COMMANDS
    // ========================================================================

    // Execute an attack against the target
    // Brain calls this when attack decision is made
    // Body decides HOW to attack (melee, ranged, spell, etc.)
    // Returns true if attack was executed successfully
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Combat")
    bool Attack(AActor* Target);

    // Execute a reload or ability cooldown reset
    // Brain calls this when ammo is low or ability needs recharge
    // Body handles the actual reload animation and timing
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Combat")
    void Reload();

    // ========================================================================
    // STATE QUERIES
    // Brain needs to know body state for decision making
    // ========================================================================

    // Check if the enemy can currently attack
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|State")
    bool CanAttack() const;

    // Check if enemy needs to reload or recharge
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|State")
    bool NeedsReload() const;

    // Get the enemy's attack range
    // Brain uses this for positioning decisions
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|State")
    float GetAttackRange() const;

 

    // Called when an attack finishes (animation complete, projectile spawned)
    // Body broadcasts this so brain can update cooldowns and state
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Notify")
    void NotifyAttackComplete();

    // Called when reload finishes
    // Body broadcasts this so brain can resume combat behavior
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Enemy|Notify")
    void NotifyReloadComplete();
};