// BaseAgent.h - AI Enemy Agent Base Class
//
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
//
// PURPOSE:
// Base class for all AI-controlled enemy characters. Provides:
// - IEnemyInterface implementation for brain-body communication
// - Faction system with dynamic material color assignment
// - AI Controller caching for efficient access
// - Blackboard integration for behavior tree decisions
//
// ARCHITECTURE:
// BaseAgent is the BODY - it executes commands from the BRAIN (AI Controller).
// The body does not make strategic decisions. It only:
// - Executes attack commands (melee by default, override for ranged)
// - Reports state (can attack, needs reload, health ratio)
// - Broadcasts action completion via delegates
//
// INHERITANCE:
// ABaseCharacter (health, stamina, team, spells)
//   -> ABaseAgent (AI behavior, faction colors)
//         -> ABatAgent (flying, projectile attack)
//         -> Future: AMageEnemy, ABossWizard, etc.
//
// USAGE:
// 1. Create Blueprint child (BP_GruntEnemy, BP_WizardMinion)
// 2. Assign AI Controller class in Blueprint
// 3. Configure faction color via Spawner or placed-in-level settings
// 4. AI Controller handles behavior tree and perception
//
// DESIGNER CONFIGURATION:
// - AttackDamage: Base damage for melee attacks
// - AttackRange: Distance at which attack can occur
// - PatrolSpeed/ChaseSpeed: Movement speeds for different states
// - MaterialParameterName: Name of color parameter in materials (default: "Tint")
// - PlacedAgentFactionID/Color: For level-placed agents without spawner

#pragma once

#include "CoreMinimal.h"
#include "Code/Actors/BaseCharacter.h"
#include "Code/Utility/EnemyInterface.h"
#include "BaseAgent.generated.h"

// Forward declarations
class UMaterialInstanceDynamic;
class AAIC_BaseAgentAIController;
class UBehaviorTree;

DECLARE_LOG_CATEGORY_EXTERN(LogBaseAgent, Log, All);

// ============================================================================
// DELEGATES
// ============================================================================

// Broadcast when attack animation/action completes
// Brain listens to update cooldowns and blackboard
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentAttackComplete);

// Broadcast when reload completes
// Brain listens to resume combat behavior
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentReloadComplete);

// ============================================================================
// BASE AGENT CLASS
// ============================================================================

UCLASS()
class WIZARDJAM_API ABaseAgent : public ABaseCharacter, public IEnemyInterface
{
    GENERATED_BODY()

public:
    ABaseAgent();

    // ========================================================================
    // IENEMYINTERFACE IMPLEMENTATION
    // Override in child classes for specialized behavior
    // ========================================================================

    // Execute melee attack against target
    // Default: Face target, apply damage, notify brain
    // Override in BatAgent for projectile attack
    virtual bool Attack_Implementation(AActor* Target) override;

    // Execute reload sequence
    // Default: Reset cooldowns, notify brain
    virtual void Reload_Implementation() override;

    // Check if agent can currently attack
    // Returns false if: on cooldown, stunned, dead
    virtual bool CanAttack_Implementation() const override;

    // Check if agent needs reload
    // Default: Returns false (melee doesn't need ammo)
    // Override in ranged enemies
    virtual bool NeedsReload_Implementation() const override;

    // Get attack range for AI positioning
    virtual float GetAttackRange_Implementation() const override;

    // Notification that attack finished
    // Called by animation notify or timer
    virtual void NotifyAttackComplete_Implementation() override;

    // Notification that reload finished
    virtual void NotifyReloadComplete_Implementation() override;

    // ========================================================================
    // FACTION SYSTEM
    // Spawners call OnFactionAssigned to set team and color
    // ========================================================================

    // Called by Spawner when this agent is spawned with faction data
    // Sets team ID and applies faction color to all materials
    UFUNCTION(BlueprintCallable, Category = "Faction")
    void OnFactionAssigned(int32 FactionID, const FLinearColor& FactionColor);

    // Manually set agent color (can be called anytime)
    UFUNCTION(BlueprintCallable, Category = "Faction")
    void SetAgentColor(const FLinearColor& NewColor);

    // Get current faction color
    UFUNCTION(BlueprintPure, Category = "Faction")
    FLinearColor GetAgentColor() const { return AgentColor; }

    // ========================================================================
    // DELEGATES
    // Brain binds to these for state updates
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnAgentAttackComplete OnAttackComplete;

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnAgentReloadComplete OnReloadComplete;

    // ========================================================================
    // AI CONTROLLER ACCESS
    // ========================================================================

    // Get cached AI controller (faster than repeated casts)
    UFUNCTION(BlueprintPure, Category = "AI")
    AAIController* GetAgentAIController() const { return CachedAIController; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ========================================================================
    // COMBAT CONFIGURATION
    // ========================================================================

    // Base damage for attacks
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float AttackDamage;

    // Distance at which agent can attack
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float AttackRange;

    // Cooldown between attacks (seconds)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float AttackCooldown;

    // ========================================================================
    // MOVEMENT CONFIGURATION
    // ========================================================================

    // Speed during patrol state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float PatrolSpeed;

    // Speed during chase state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
    float ChaseSpeed;

    // ========================================================================
    // APPEARANCE CONFIGURATION
    // ========================================================================

    // Current faction color (set by spawner or level designer)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
    FLinearColor AgentColor;

    // Material parameter name for color tinting
    // Must match the parameter name in your materials
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
    FName MaterialParameterName;

    // ========================================================================
    // LEVEL-PLACED AGENT CONFIGURATION
    // For agents placed directly in level (not spawned)
    // ========================================================================

    // Faction ID for level-placed agents
    UPROPERTY(EditInstanceOnly, Category = "Faction|Level Placed", meta = (ClampMin = "0", ClampMax = "255"))
    int32 PlacedAgentFactionID;

    // Faction color for level-placed agents
    UPROPERTY(EditInstanceOnly, Category = "Faction|Level Placed")
    FLinearColor PlacedAgentFactionColor;

    // ========================================================================
    // AI CONFIGURATION
    // ========================================================================

    // Display name for this enemy type (for HUD, damage numbers, etc.)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    FString EnemyTypeName;

    // ========================================================================
    // INTERNAL STATE
    // ========================================================================

    // Cached AI controller reference
    UPROPERTY()
    AAIController* CachedAIController;

    // Dynamic materials for color changes
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DynamicMaterials;

    // Attack cooldown timer
    float AttackCooldownRemaining;

    // ========================================================================
    // INTERNAL FUNCTIONS
    // ========================================================================

    // Setup dynamic materials for faction color changes
    void SetupAgentAppearance();

    // Update blackboard with current health ratio
    void UpdateBlackboardHealth(float HealthRatio);

    // Called when health component reports damage
    // Signature matches FOnHealthChanged: (AActor* Owner, float NewHealth, float Delta)
    UFUNCTION()
    void HandleDamageTaken(AActor* HitOwner, float NewHealth, float Delta);

    // Called when health component reports death
    // Signature matches FOnDeath: (AActor* Owner, AActor* Killer)
    UFUNCTION()
    void HandleDeath(AActor* HitOwner, AActor* Killer);

    // Play attack effects (sound, particles)
    void PlayAttackEffects(AActor* Target);
};