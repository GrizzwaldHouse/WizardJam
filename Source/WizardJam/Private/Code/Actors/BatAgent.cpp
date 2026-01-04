// BatAgent.cpp - Flying Bat Enemy Implementation
//
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
//
// IMPLEMENTATION NOTES:
// - Flying enemy that attacks with projectiles from mouth socket
// - Inherits health/death handling from BaseAgent (do NOT duplicate handlers here)
// - Optional simple AI for quick testing, disable for behavior tree in production

#include "Code/Actors/BatAgent.h"

// Engine includes
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

// Project includes
#include "Code/Actors/BaseProjectile.h"
#include "Code/Utility/AC_HealthComponent.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogBatAgent, Log, All);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ABatAgent::ABatAgent()
    : ProjectileSpeed(1200.0f)
    , MuzzleSocketName(TEXT("MuzzleSocket"))
    , bUseSimpleAI(false)
    , AttackRange(800.0f)
    , FlySpeed(450.0f)
{
    // Configure flying movement mode
    // Without this, bat falls through floor like ground character
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Flying);
        Movement->MaxFlySpeed = FlySpeed;
        Movement->BrakingDecelerationFlying = 1000.0f;

        UE_LOG(LogBatAgent, Log, TEXT("Flying movement configured: MaxFlySpeed=%.0f"), FlySpeed);
    }

    // Adjust capsule size for flying creature (smaller than ground enemies)
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCapsuleSize(30.0f, 30.0f);
    }

    // Bat-specific combat defaults (less damage than melee enemies)
    AttackDamage = 10.0f;
    EnemyTypeName = TEXT("Evil Bat");
    PatrolSpeed = 300.0f;
    ChaseSpeed = FlySpeed;

    // Enable tick for simple AI (only used if bUseSimpleAI is true)
    PrimaryActorTick.bCanEverTick = true;

    UE_LOG(LogBatAgent, Log, TEXT("BatAgent constructor complete"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABatAgent::BeginPlay()
{
    // CRITICAL: Call parent BeginPlay first
    // BaseAgent::BeginPlay binds to HealthComponent delegates
    // Do NOT re-bind or duplicate handler functions here
    Super::BeginPlay();

    // Validate projectile setup for designer feedback
    if (!ProjectileClass)
    {
        UE_LOG(LogBatAgent, Error, TEXT("[%s] DESIGNER: ProjectileClass not set! Bat cannot attack."),
            *GetName());
    }

    // Validate socket exists on mesh
    if (GetMesh() && !GetMesh()->DoesSocketExist(MuzzleSocketName))
    {
        UE_LOG(LogBatAgent, Warning, TEXT("[%s] DESIGNER: Socket '%s' not found on skeletal mesh! Projectiles will spawn from actor center."),
            *GetName(), *MuzzleSocketName.ToString());
    }

    // Log AI mode for debugging
    if (bUseSimpleAI)
    {
        UE_LOG(LogBatAgent, Display, TEXT("[%s] Using SIMPLE AI (tick-based chase/attack)"),
            *GetName());
    }
    else
    {
        UE_LOG(LogBatAgent, Display, TEXT("[%s] Using BEHAVIOR TREE AI (requires AI Controller with BehaviorTree asset)"),
            *GetName());
    }

    UE_LOG(LogBatAgent, Display, TEXT("[%s] Evil Bat spawned - Flying mode active, AttackRange: %.0f"),
        *GetName(), AttackRange);
}

void ABatAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Only run simple AI if enabled (disabled by default - use behavior tree)
    if (bUseSimpleAI)
    {
        SimpleAI_ChaseAndAttack(DeltaTime);
    }
}

// ============================================================================
// IENEMYINTERFACE OVERRIDE - PROJECTILE ATTACK
// ============================================================================

bool ABatAgent::Attack_Implementation(AActor* Target)
{
    // PROJECTILE ATTACK:
    // Instead of melee damage like BaseAgent, bat fires projectile from mouth.
    // 
    // FLOW:
    // 1. Spawn projectile at mouth socket
    // 2. Aim it at target
    // 3. Set initial velocity
    // 4. Notify brain that attack completed (for cooldown/ammo tracking)

    if (!Target)
    {
        UE_LOG(LogBatAgent, Warning, TEXT("[%s] Attack command received but Target is null"),
            *GetName());
        return false;
    }

    // Check cooldown using parent logic
    if (AttackCooldownRemaining > 0.0f)
    {
        UE_LOG(LogBatAgent, Verbose, TEXT("[%s] Attack blocked - cooldown remaining: %.2f"),
            *GetName(), AttackCooldownRemaining);
        return false;
    }

    // Spawn projectile aimed at target
    ABaseProjectile* Projectile = SpawnProjectileAtMouth(Target);

    if (!Projectile)
    {
        UE_LOG(LogBatAgent, Error, TEXT("[%s] Failed to spawn projectile!"), *GetName());
        return false;
    }

    // Start attack cooldown
    AttackCooldownRemaining = AttackCooldown;

    UE_LOG(LogBatAgent, Display, TEXT("[%s] Fired projectile at %s"),
        *GetName(), *Target->GetName());

    // Play attack effects (sound/particles) - implemented in parent
    PlayAttackEffects(Target);

    // CRITICAL: Notify brain that attack completed
    // Brain updates ammo count and cooldown timer
    NotifyAttackComplete_Implementation();

    return true;
}

// ============================================================================
// SIMPLE AI IMPLEMENTATION
// ============================================================================

void ABatAgent::SimpleAI_ChaseAndAttack(float DeltaTime)
{
    // SIMPLE TICK-BASED AI:
    // This is a basic AI for quick testing. It's not as sophisticated
    // as a behavior tree, but it works and is easy to understand.
    // 
    // Logic:
    // 1. Find player
    // 2. If too far: Fly toward player
    // 3. If in range: Attack if cooldown ready
    // 
    // For production, disable bUseSimpleAI and use a behavior tree instead.

    // Can't attack if dead
    if (HealthComponent && !HealthComponent->IsAlive())
    {
        return;
    }

    AActor* Player = FindPlayer();
    if (!Player)
    {
        return;
    }

    float DistanceToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());

    if (DistanceToPlayer > AttackRange)
    {
        // TOO FAR: Chase player
        FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        AddMovementInput(Direction, 1.0f);

        // Face player while flying (smooth rotation)
        FRotator LookRotation = Direction.Rotation();
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRotation, DeltaTime, 5.0f));
    }
    else
    {
        // IN RANGE: Attack if cooldown allows
        if (CanAttack_Implementation())
        {
            Attack_Implementation(Player);
        }
    }
}

AActor* ABatAgent::FindPlayer()
{
    // Simple player finder - just gets player 0
    // For multiplayer, you'd need more sophisticated logic
    return UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
}

bool ABatAgent::IsInAttackRange(AActor* Target) const
{
    if (!Target)
    {
        return false;
    }

    float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    return Distance <= AttackRange;
}

// ============================================================================
// PROJECTILE SPAWNING
// ============================================================================

ABaseProjectile* ABatAgent::SpawnProjectileAtMouth(AActor* Target)
{
    if (!ProjectileClass || !Target)
    {
        return nullptr;
    }

    // Get spawn location/rotation from mouth socket
    FVector SpawnLocation = GetMouthLocation();
    FRotator SpawnRotation = GetMouthRotation();

    // Calculate direction to target
    FVector TargetLocation = Target->GetActorLocation();
    FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal();
    SpawnRotation = Direction.Rotation();

    // Configure spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Spawn the projectile
    ABaseProjectile* Projectile = GetWorld()->SpawnActor<ABaseProjectile>(
        ProjectileClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (Projectile)
    {
        // Set projectile velocity using ProjectileMovementComponent
        if (UProjectileMovementComponent* ProjectileMovement = Projectile->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjectileMovement->Velocity = Direction * ProjectileSpeed;
        }

        UE_LOG(LogBatAgent, Verbose, TEXT("[%s] Spawned projectile at %s, velocity: %s"),
            *GetName(), *SpawnLocation.ToString(), *(Direction * ProjectileSpeed).ToString());
    }

    return Projectile;
}

FVector ABatAgent::GetMouthLocation() const
{
    if (GetMesh() && GetMesh()->DoesSocketExist(MuzzleSocketName))
    {
        return GetMesh()->GetSocketLocation(MuzzleSocketName);
    }

    // Fallback: Use actor location if socket not found
    UE_LOG(LogBatAgent, Warning, TEXT("[%s] MuzzleSocket not found, using actor location"),
        *GetName());
    return GetActorLocation();
}

FRotator ABatAgent::GetMouthRotation() const
{
    if (GetMesh() && GetMesh()->DoesSocketExist(MuzzleSocketName))
    {
        return GetMesh()->GetSocketRotation(MuzzleSocketName);
    }

    // Fallback: Use actor rotation if socket not found
    return GetActorRotation();
}