// ============================================================================
// BaseProjectile.cpp
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// ============================================================================
// IMPORTANT: This file COMPLETELY REPLACES your existing BaseProjectile.cpp
// DELETE your old file first, then add this one.
// ============================================================================
// Purpose:
// Implementation of the base projectile class for spell combat.
//
// Key Implementation Details:
// - Constructor initializes all components and default values
// - InitializeProjectile() sets up collision ignore for owner
// - Overlap-based collision for gameplay-friendly hit detection
// - Niagara effects with Cascade fallback for compatibility
// - Team-based friendly fire prevention
//
// Collision Ignore Fix:
// IgnoreActorWhenMoving() is called on CollisionSphere (UPrimitiveComponent),
// NOT on ProjectileMovementComponent. The YouTube tutorial confirms this.
// ============================================================================

#include "Code/Actors/BaseProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Engine/DamageEvents.h"

DEFINE_LOG_CATEGORY(LogBaseProjectile);

// ============================================================================
// CONSTRUCTOR
// All default values in initialization list per coding standards
// ============================================================================

ABaseProjectile::ABaseProjectile()
    : SpellElement(TEXT("Flame"))
    , ElementColor(FLinearColor(1.0f, 0.5f, 0.0f, 1.0f))
    , Damage(15.0f)
    , InitialSpeed(3000.0f)
    , LifetimeSeconds(5.0f)
    , CollisionRadius(15.0f)
    , TrailNiagaraSystem(nullptr)
    , ImpactNiagaraSystem(nullptr)
    , TrailCascadeSystem(nullptr)
    , ImpactCascadeSystem(nullptr)
    , bDidHitSomething(false)
{
    PrimaryActorTick.bCanEverTick = false;

    // ========================================================================
    // COLLISION SPHERE
    // Root component - detects overlaps with targets
    // ========================================================================

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    CollisionSphere->InitSphereRadius(CollisionRadius);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionSphere->SetGenerateOverlapEvents(true);
    RootComponent = CollisionSphere;

    // ========================================================================
    // PROJECTILE MESH
    // Visual representation - color applied in BeginPlay
    // ========================================================================

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ========================================================================
    // PROJECTILE MOVEMENT
    // Handles physics-based movement without requiring Tick
    // ========================================================================

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionSphere;
    ProjectileMovement->InitialSpeed = InitialSpeed;
    ProjectileMovement->MaxSpeed = InitialSpeed;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    // ========================================================================
    // NIAGARA TRAIL COMPONENT
    // Created but not activated until BeginPlay
    // ========================================================================

    TrailNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailNiagara"));
    TrailNiagaraComponent->SetupAttachment(RootComponent);
    TrailNiagaraComponent->bAutoActivate = false;

    UE_LOG(LogBaseProjectile, Verbose, TEXT("Projectile constructed"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABaseProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Bind overlap event
    CollisionSphere->OnComponentBeginOverlap.AddDynamic(
        this, &ABaseProjectile::OnOverlapBegin);

    // Apply visuals and start trail
    ApplyMaterialColor();
    InitializeTrailEffect();

    // Lifetime timer - destroy after timeout
    GetWorld()->GetTimerManager().SetTimer(
        LifetimeTimerHandle,
        this,
        &ABaseProjectile::OnLifetimeExpired,
        LifetimeSeconds,
        false);

    UE_LOG(LogBaseProjectile, Verbose,
        TEXT("[%s] BeginPlay | Element: %s | Damage: %.1f | Speed: %.0f"),
        *GetName(), *SpellElement.ToString(), Damage, InitialSpeed);
}

void ABaseProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorld()->GetTimerManager().ClearTimer(LifetimeTimerHandle);
    OnProjectileDestroyed.Broadcast(this, bDidHitSomething);

    if (CollisionSphere)
    {
        CollisionSphere->OnComponentBeginOverlap.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// INITIALIZATION
// Called by CombatComponent after spawning
// ============================================================================

void ABaseProjectile::InitializeProjectile(AActor* OwningActor, const FVector& LaunchDirection)
{
    // ========================================================================
    // CACHE OWNER AND INSTIGATOR
    // ========================================================================

    CachedOwner = OwningActor;
    SetOwner(OwningActor);

    if (OwningActor)
    {
        APawn* OwnerPawn = Cast<APawn>(OwningActor);
        if (OwnerPawn)
        {
            CachedInstigator = OwnerPawn;
            SetInstigator(OwnerPawn);
        }
        else
        {
            CachedInstigator = OwningActor->GetInstigator();
            SetInstigator(CachedInstigator.Get());
        }
    }

    // ========================================================================
    // COLLISION IGNORE SETUP
    // CRITICAL: Call IgnoreActorWhenMoving on the COLLISION COMPONENT
    // NOT on ProjectileMovementComponent (that doesn't have this function)
    // This prevents the projectile from hitting the actor that fired it
    // ========================================================================

    if (OwningActor && CollisionSphere)
    {
        // This is the correct UE5.4 API - called on collision primitive
        CollisionSphere->IgnoreActorWhenMoving(OwningActor, true);

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Added owner '%s' to collision ignore list"),
            *GetName(), *OwningActor->GetName());
    }

    // Also ignore instigator if different from owner
    if (CachedInstigator.IsValid() && CachedInstigator.Get() != OwningActor)
    {
        if (CollisionSphere)
        {
            CollisionSphere->IgnoreActorWhenMoving(CachedInstigator.Get(), true);
        }

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Added instigator '%s' to collision ignore list"),
            *GetName(), *CachedInstigator->GetName());
    }

    // ========================================================================
    // SET VELOCITY
    // Apply launch direction to movement component
    // ========================================================================

    if (ProjectileMovement)
    {
        FVector NormalizedDirection = LaunchDirection.GetSafeNormal();
        ProjectileMovement->Velocity = NormalizedDirection * InitialSpeed;

        UE_LOG(LogBaseProjectile, Display,
            TEXT("[%s] Initialized | Owner: %s | Direction: %s | Speed: %.0f"),
            *GetName(),
            OwningActor ? *OwningActor->GetName() : TEXT("None"),
            *NormalizedDirection.ToString(),
            InitialSpeed);
    }
}

// ============================================================================
// OVERLAP HANDLING
// ============================================================================

void ABaseProjectile::OnOverlapBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // Skip invalid or self
    if (!OtherActor || OtherActor == this)
    {
        return;
    }

    // Skip owner or instigator
    if (OtherActor == CachedOwner.Get() || OtherActor == CachedInstigator.Get())
    {
        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Ignoring overlap with owner/instigator: %s"),
            *GetName(), *OtherActor->GetName());
        return;
    }

    // Skip friendly actors
    if (IsFriendlyActor(OtherActor))
    {
        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Ignoring overlap with friendly: %s"),
            *GetName(), *OtherActor->GetName());
        return;
    }

    // Build hit result
    FHitResult HitResult = SweepResult;
    if (!HitResult.IsValidBlockingHit())
    {
        HitResult.ImpactPoint = OtherComp ? OtherComp->GetComponentLocation() : OtherActor->GetActorLocation();
        HitResult.ImpactNormal = -GetActorForwardVector();
        HitResult.Location = GetActorLocation();
        HitResult.Normal = HitResult.ImpactNormal;
    }

    UE_LOG(LogBaseProjectile, Display,
        TEXT("[%s] Hit: %s at %s"),
        *GetName(), *OtherActor->GetName(), *HitResult.ImpactPoint.ToString());

    bDidHitSomething = true;
    HandleHit(OtherActor, HitResult);
}

// ============================================================================
// HIT HANDLING
// ============================================================================

void ABaseProjectile::HandleHit_Implementation(AActor* HitActor, const FHitResult& HitResult)
{
    // Apply damage to actors
    if (HitActor)
    {
        ApplyDamage(HitActor, HitResult);
    }

    // Spawn impact effect
    FVector ImpactLocation = HitResult.ImpactPoint;
    FVector ImpactNormal = HitResult.ImpactNormal;
    SpawnImpactEffect(ImpactLocation, ImpactNormal);

    // Broadcast hit event
    OnProjectileHit.Broadcast(this, HitActor, HitResult);

    // Destroy projectile
    Destroy();
}

void ABaseProjectile::ApplyDamage_Implementation(AActor* HitActor, const FHitResult& HitResult)
{
    if (!HitActor || Damage <= 0.0f)
    {
        return;
    }

    // Create damage event
    FPointDamageEvent DamageEvent;
    DamageEvent.Damage = Damage;
    DamageEvent.HitInfo = HitResult;
    DamageEvent.ShotDirection = GetActorForwardVector();

    // Get instigator controller
    AController* InstigatorController = nullptr;
    if (CachedInstigator.IsValid())
    {
        InstigatorController = CachedInstigator->GetController();
    }

    // Apply damage
    HitActor->TakeDamage(
        Damage,
        DamageEvent,
        InstigatorController,
        CachedOwner.Get()
    );

    UE_LOG(LogBaseProjectile, Display,
        TEXT("[%s] Applied %.1f damage to %s"),
        *GetName(), Damage, *HitActor->GetName());
}

// ============================================================================
// EFFECT HELPERS
// ============================================================================

void ABaseProjectile::InitializeTrailEffect()
{
    // Priority: Niagara first, then Cascade fallback
    if (TrailNiagaraSystem && TrailNiagaraComponent)
    {
        TrailNiagaraComponent->SetAsset(TrailNiagaraSystem);
        TrailNiagaraComponent->SetColorParameter(FName("ElementColor"), ElementColor);
        TrailNiagaraComponent->Activate();

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Niagara trail activated"),
            *GetName());
    }
    else if (TrailCascadeSystem)
    {
        UGameplayStatics::SpawnEmitterAttached(
            TrailCascadeSystem,
            RootComponent,
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true
        );

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Cascade trail spawned"),
            *GetName());
    }
}

void ABaseProjectile::SpawnImpactEffect(const FVector& Location, const FVector& Normal)
{
    // Priority: Niagara first, then Cascade fallback
    if (ImpactNiagaraSystem)
    {
        UNiagaraComponent* ImpactNiagara = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            ImpactNiagaraSystem,
            Location,
            Normal.Rotation(),
            FVector::OneVector,
            true,
            true,
            ENCPoolMethod::AutoRelease
        );

        if (ImpactNiagara)
        {
            ImpactNiagara->SetColorParameter(FName("ElementColor"), ElementColor);
        }

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Niagara impact spawned at %s"),
            *GetName(), *Location.ToString());
    }
    else if (ImpactCascadeSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            ImpactCascadeSystem,
            Location,
            Normal.Rotation()
        );

        UE_LOG(LogBaseProjectile, Verbose,
            TEXT("[%s] Cascade impact spawned at %s"),
            *GetName(), *Location.ToString());
    }
}

void ABaseProjectile::ApplyMaterialColor()
{
    if (!ProjectileMesh)
    {
        return;
    }

    int32 NumMaterials = ProjectileMesh->GetNumMaterials();

    for (int32 i = 0; i < NumMaterials; i++)
    {
        UMaterialInterface* BaseMaterial = ProjectileMesh->GetMaterial(i);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* DynMaterial =
                ProjectileMesh->CreateAndSetMaterialInstanceDynamic(i);

            if (DynMaterial)
            {
                DynMaterial->SetVectorParameterValue(FName("Color"), ElementColor);
                DynMaterial->SetVectorParameterValue(FName("BaseColor"), ElementColor);
                DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), ElementColor);
            }
        }
    }

    UE_LOG(LogBaseProjectile, Verbose,
        TEXT("[%s] Applied color to %d materials"),
        *GetName(), NumMaterials);
}

// ============================================================================
// TEAM CHECKING
// ============================================================================

bool ABaseProjectile::IsFriendlyActor(AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    // Get owner's team
    FGenericTeamId OwnerTeam = FGenericTeamId::NoTeam;
    if (CachedOwner.IsValid())
    {
        IGenericTeamAgentInterface* OwnerTeamAgent =
            Cast<IGenericTeamAgentInterface>(CachedOwner.Get());
        if (OwnerTeamAgent)
        {
            OwnerTeam = OwnerTeamAgent->GetGenericTeamId();
        }
    }

    // Get other actor's team
    IGenericTeamAgentInterface* OtherTeamAgent =
        Cast<IGenericTeamAgentInterface>(OtherActor);
    if (!OtherTeamAgent)
    {
        return false;
    }

    FGenericTeamId OtherTeam = OtherTeamAgent->GetGenericTeamId();

    // Same team = friendly
    if (OwnerTeam == FGenericTeamId::NoTeam || OtherTeam == FGenericTeamId::NoTeam)
    {
        return false;
    }

    return OwnerTeam == OtherTeam;
}

// ============================================================================
// LIFETIME HANDLING
// ============================================================================

void ABaseProjectile::OnLifetimeExpired()
{
    UE_LOG(LogBaseProjectile, Verbose,
        TEXT("[%s] Lifetime expired after %.1fs"),
        *GetName(), LifetimeSeconds);

    Destroy();
}