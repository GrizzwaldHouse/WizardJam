// BaseAgent.cpp - AI Enemy Agent Implementation
//
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
//
// IMPLEMENTATION NOTES:
// - Melee attack by default (face target, apply damage)
// - Faction color applied to ALL material slots (not just slot 0)
// - Blackboard updates for health ratio enable AI decision making
// - Observer pattern: delegates broadcast completion, brain listens

#include "Code/Actors/BaseAgent.h"
#include "Code/Utility/AC_HealthComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

// Define log category
DEFINE_LOG_CATEGORY(LogBaseAgent);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ABaseAgent::ABaseAgent()
    : AttackDamage(20.0f)
    , AttackRange(150.0f)
    , AttackCooldown(1.0f)
    , PatrolSpeed(200.0f)
    , ChaseSpeed(400.0f)
    , AgentColor(FLinearColor::Red)
    , MaterialParameterName(TEXT("Tint"))
    , PlacedAgentFactionID(1)
    , PlacedAgentFactionColor(FLinearColor::Red)
    , EnemyTypeName(TEXT("Enemy"))
    , CachedAIController(nullptr)
    , AttackCooldownRemaining(0.0f)
{
    // Enable tick for cooldown management
    PrimaryActorTick.bCanEverTick = true;

    // Configure AI possession
    // PlacedInWorldOrSpawned ensures AI works for both placed and spawned agents
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    UE_LOG(LogBaseAgent, Log, TEXT("BaseAgent constructor | AttackDamage: %.1f | AttackRange: %.0f"),
        AttackDamage, AttackRange);
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABaseAgent::BeginPlay()
{
    Super::BeginPlay();

    // Cache AI controller reference for efficient access
    CachedAIController = Cast<AAIController>(GetController());
    if (CachedAIController)
    {
        UE_LOG(LogBaseAgent, Display, TEXT("[%s] AI Controller: %s"),
            *GetName(), *CachedAIController->GetName());
    }
    else
    {
        UE_LOG(LogBaseAgent, Warning, TEXT("[%s] No AI Controller! Agent will not have AI behavior."),
            *GetName());
    }

    // Setup faction for level-placed agents (spawned agents get faction from spawner)
    if (GetOwner() == nullptr)
    {
        // No owner means placed in level, not spawned
        UE_LOG(LogBaseAgent, Log, TEXT("[%s] Level-placed agent - applying faction ID %d"),
            *GetName(), PlacedAgentFactionID);

        OnFactionAssigned(PlacedAgentFactionID, PlacedAgentFactionColor);
    }
    else
    {
        UE_LOG(LogBaseAgent, Log, TEXT("[%s] Spawned agent - faction assigned by spawner"),
            *GetName());
    }

    // Setup dynamic materials for color changes
    SetupAgentAppearance();

    // Bind to health component for damage/death events
    // FIX: Delegate signatures must match exactly
    // FOnHealthChanged expects: (AActor* Owner, float NewHealth, float Delta)
    // FOnDeath expects: (AActor* Owner, AActor* Killer)
    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddDynamic(this, &ABaseAgent::HandleDamageTaken);
        HealthComponent->OnDeath.AddDynamic(this, &ABaseAgent::HandleDeath);

        // Initialize blackboard with starting health
        UpdateBlackboardHealth(1.0f);

        UE_LOG(LogBaseAgent, Display, TEXT("[%s] Bound to HealthComponent"),
            *GetName());
    }

    // Configure collision for overlap events (damage detection, etc.)
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
        GetCapsuleComponent()->SetGenerateOverlapEvents(true);
    }

    UE_LOG(LogBaseAgent, Display, TEXT("[%s] BaseAgent BeginPlay complete | TeamID: %d | Color: (%.2f, %.2f, %.2f)"),
        *GetName(), TeamID, AgentColor.R, AgentColor.G, AgentColor.B);
}

void ABaseAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update attack cooldown
    if (AttackCooldownRemaining > 0.0f)
    {
        AttackCooldownRemaining -= DeltaTime;
    }
}

// ============================================================================
// IENEMYINTERFACE IMPLEMENTATION
// ============================================================================

bool ABaseAgent::Attack_Implementation(AActor* Target)
{
    // Validate target
    if (!Target)
    {
        UE_LOG(LogBaseAgent, Warning, TEXT("[%s] Attack failed - null target"),
            *GetName());
        return false;
    }

    // Check cooldown
    if (AttackCooldownRemaining > 0.0f)
    {
        UE_LOG(LogBaseAgent, Verbose, TEXT("[%s] Attack blocked - cooldown remaining: %.2f"),
            *GetName(), AttackCooldownRemaining);
        return false;
    }

    // Face target
    FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FRotator LookRotation = Direction.Rotation();
    SetActorRotation(LookRotation);

    // Apply damage (melee attack)
    UGameplayStatics::ApplyDamage(
        Target,
        AttackDamage,
        GetController(),
        this,
        nullptr // Damage type
    );

    // Start cooldown
    AttackCooldownRemaining = AttackCooldown;

    // Play effects
    PlayAttackEffects(Target);

    UE_LOG(LogBaseAgent, Display, TEXT("[%s] Melee attack on %s for %.1f damage"),
        *GetName(), *Target->GetName(), AttackDamage);

    // Notify brain that attack completed
    NotifyAttackComplete_Implementation();

    return true;
}

void ABaseAgent::Reload_Implementation()
{
    // Base agent (melee) doesn't use ammo, but we implement for interface compliance
    UE_LOG(LogBaseAgent, Log, TEXT("[%s] Reload called (melee agent - no-op)"),
        *GetName());

    // Immediately notify complete since there's nothing to reload
    NotifyReloadComplete_Implementation();
}

bool ABaseAgent::CanAttack_Implementation() const
{
    // Can't attack if on cooldown
    if (AttackCooldownRemaining > 0.0f)
    {
        return false;
    }

    // Can't attack if dead
    // FIX: Use IsAlive() instead of non-existent IsDead()
    // AC_HealthComponent exposes IsAlive(), not IsDead()
    if (HealthComponent && !HealthComponent->IsAlive())
    {
        return false;
    }

    return true;
}

bool ABaseAgent::NeedsReload_Implementation() const
{
    // Melee agent never needs reload
    return false;
}

float ABaseAgent::GetAttackRange_Implementation() const
{
    return AttackRange;
}

void ABaseAgent::NotifyAttackComplete_Implementation()
{
    // Broadcast to listeners (AI controller, behavior tree, etc.)
    OnAttackComplete.Broadcast();

    // Update blackboard if controller exists
    if (CachedAIController)
    {
        UBlackboardComponent* BB = CachedAIController->GetBlackboardComponent();
        if (BB)
        {
            BB->SetValueAsBool(TEXT("ActionFinished"), true);
        }
    }

    UE_LOG(LogBaseAgent, Log, TEXT("[%s] Attack complete broadcast"),
        *GetName());
}

void ABaseAgent::NotifyReloadComplete_Implementation()
{
    // Broadcast to listeners
    OnReloadComplete.Broadcast();

    UE_LOG(LogBaseAgent, Log, TEXT("[%s] Reload complete broadcast"),
        *GetName());
}

// ============================================================================
// FACTION SYSTEM
// ============================================================================

void ABaseAgent::OnFactionAssigned(int32 FactionID, const FLinearColor& FactionColor)
{
    UE_LOG(LogBaseAgent, Display, TEXT("[%s] Faction assigned: ID=%d, Color=(%.2f, %.2f, %.2f)"),
        *GetName(), FactionID, FactionColor.R, FactionColor.G, FactionColor.B);

    // Update team ID for AI perception
    TeamID = static_cast<uint8>(FactionID);

    // Update visual appearance
    SetAgentColor(FactionColor);

    // Update AI controller's team if exists
    if (CachedAIController)
    {
        // Controller should also implement team interface
        IGenericTeamAgentInterface* TeamInterface = Cast<IGenericTeamAgentInterface>(CachedAIController);
        if (TeamInterface)
        {
            TeamInterface->SetGenericTeamId(FGenericTeamId(TeamID));
            UE_LOG(LogBaseAgent, Log, TEXT("[%s] Controller team updated to %d"),
                *GetName(), TeamID);
        }

        // Update blackboard with faction data
        UBlackboardComponent* BB = CachedAIController->GetBlackboardComponent();
        if (BB)
        {
            BB->SetValueAsInt(TEXT("FactionID"), FactionID);
            BB->SetValueAsVector(TEXT("FactionColor"),
                FVector(FactionColor.R, FactionColor.G, FactionColor.B));
        }
    }
}

void ABaseAgent::SetAgentColor(const FLinearColor& NewColor)
{
    AgentColor = NewColor;

    // Apply color to all dynamic materials
    if (DynamicMaterials.Num() == 0)
    {
        UE_LOG(LogBaseAgent, Warning, TEXT("[%s] No dynamic materials - SetupAgentAppearance may not have run"),
            *GetName());
        return;
    }

    for (int32 i = 0; i < DynamicMaterials.Num(); i++)
    {
        if (DynamicMaterials[i])
        {
            DynamicMaterials[i]->SetVectorParameterValue(MaterialParameterName, NewColor);
        }
    }

    UE_LOG(LogBaseAgent, Log, TEXT("[%s] Applied color (%.2f, %.2f, %.2f) to %d materials"),
        *GetName(), NewColor.R, NewColor.G, NewColor.B, DynamicMaterials.Num());
}

// ============================================================================
// APPEARANCE SETUP
// ============================================================================

void ABaseAgent::SetupAgentAppearance()
{
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp)
    {
        UE_LOG(LogBaseAgent, Error, TEXT("[%s] No skeletal mesh component!"),
            *GetName());
        return;
    }

    int32 NumMaterials = MeshComp->GetNumMaterials();
    if (NumMaterials == 0)
    {
        UE_LOG(LogBaseAgent, Warning, TEXT("[%s] Mesh has no materials"),
            *GetName());
        return;
    }

    // Clear existing dynamic materials
    DynamicMaterials.Empty();

    // Create dynamic material for EVERY material slot
    // This ensures faction color applies to entire character, not just one slot
    for (int32 i = 0; i < NumMaterials; i++)
    {
        UMaterialInterface* BaseMaterial = MeshComp->GetMaterial(i);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            if (DynMat)
            {
                MeshComp->SetMaterial(i, DynMat);
                DynMat->SetVectorParameterValue(MaterialParameterName, AgentColor);
                DynamicMaterials.Add(DynMat);

                UE_LOG(LogBaseAgent, Verbose, TEXT("[%s] Created dynamic material for slot %d"),
                    *GetName(), i);
            }
        }
    }

    UE_LOG(LogBaseAgent, Display, TEXT("[%s] Setup %d dynamic materials for faction coloring"),
        *GetName(), DynamicMaterials.Num());
}

// ============================================================================
// BLACKBOARD UPDATES
// ============================================================================

void ABaseAgent::UpdateBlackboardHealth(float HealthRatio)
{
    if (!CachedAIController)
    {
        return;
    }

    UBlackboardComponent* BB = CachedAIController->GetBlackboardComponent();
    if (!BB)
    {
        return;
    }

    BB->SetValueAsFloat(TEXT("HealthRatio"), HealthRatio);

    UE_LOG(LogBaseAgent, Verbose, TEXT("[%s] Blackboard HealthRatio: %.2f"),
        *GetName(), HealthRatio);
}

// ============================================================================
// HEALTH EVENT HANDLERS
// FIX: Signatures updated to match AC_HealthComponent delegate declarations
// ============================================================================

void ABaseAgent::HandleDamageTaken(AActor* HitOwner, float NewHealth, float Delta)
{
    // Calculate health ratio for AI decision making
    // NewHealth is current health, need max health from component
    float MaxHealth = HealthComponent ? HealthComponent->GetMaxHealth() : 100.0f;

    if (MaxHealth <= 0.0f)
    {
        return;
    }

    float HealthRatio = NewHealth / MaxHealth;
    UpdateBlackboardHealth(HealthRatio);

    UE_LOG(LogBaseAgent, Log, TEXT("[%s] Damage taken - Health: %.0f/%.0f (%.0f%%) | Delta: %.1f"),
        *GetName(), NewHealth, MaxHealth, HealthRatio * 100.0f, Delta);
}

void ABaseAgent::HandleDeath(AActor* HitOwner, AActor* Killer)
{
    UE_LOG(LogBaseAgent, Warning, TEXT("[%s] Death triggered | Killed by: %s"),
        *GetName(), Killer ? *Killer->GetName() : TEXT("Unknown"));

    // Update blackboard
    UpdateBlackboardHealth(0.0f);

    // Agent death handling (animation, ragdoll, etc.) would go here
    // For now, just destroy after delay
    SetLifeSpan(3.0f);
}

// ============================================================================
// EFFECTS
// ============================================================================

void ABaseAgent::PlayAttackEffects(AActor* Target)
{
    // Override in child classes for specific effects
    // Base implementation does nothing

    // Future: Could add melee hit sound, particle effect at target location, etc.
}