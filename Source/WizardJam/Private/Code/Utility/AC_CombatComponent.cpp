// ============================================================================
// AC_CombatComponent.cpp
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of modular combat component. Handles projectile spawning
// with trajectory correction, fire rate management, and event broadcasting.
// ============================================================================

#include "Code/Utility/AC_CombatComponent.h"
#include "Code/Utility/AC_AimComponent.h"
#include "Code/Actors/BaseProjectile.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogCombatComponent);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UAC_CombatComponent::UAC_CombatComponent()
    : MuzzleSocketName(TEXT("MuzzleSocket"))
    , MuzzleOffset(FVector(60.0f, 0.0f, 70.0f))
    , FireCooldown(0.5f)
    , bRespectAimBlocked(true)
    , DefaultProjectileClass(nullptr)
    , MuzzlePointComponent(nullptr)
    , AimComponent(nullptr)
    , LastFireTime(-1000.0f)
    , bWasOnCooldown(false)
{
    // Enable tick for cooldown state broadcasting
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.1f;

    UE_LOG(LogCombatComponent, Log,
        TEXT("CombatComponent constructed | Cooldown: %.2fs | MuzzleOffset: %s"),
        FireCooldown, *MuzzleOffset.ToString());
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAC_CombatComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwner())
    {
        UE_LOG(LogCombatComponent, Error,
            TEXT("CombatComponent has no owner"));
        return;
    }

    // Create muzzle point component as child of owner root
    // This provides a spawn point that works without skeletal mesh
    MuzzlePointComponent = NewObject<USceneComponent>(GetOwner(),
        USceneComponent::StaticClass(), TEXT("MuzzlePoint"));

    if (MuzzlePointComponent)
    {
        MuzzlePointComponent->SetupAttachment(GetOwner()->GetRootComponent());
        MuzzlePointComponent->SetRelativeLocation(MuzzleOffset);
        MuzzlePointComponent->RegisterComponent();

        UE_LOG(LogCombatComponent, Display,
            TEXT("[%s] MuzzlePoint created at offset %s"),
            *GetOwner()->GetName(), *MuzzleOffset.ToString());
    }

    // Find aim component on same owner
    FindAimComponent();

    UE_LOG(LogCombatComponent, Display,
        TEXT("[%s] CombatComponent ready | AimComponent: %s | DefaultProjectile: %s"),
        *GetOwner()->GetName(),
        AimComponent ? TEXT("Found") : TEXT("NOT FOUND"),
        DefaultProjectileClass ? *DefaultProjectileClass->GetName() : TEXT("None"));
}

void UAC_CombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Broadcast cooldown state changes
    bool bIsOnCooldown = GetCooldownRemaining() > 0.0f;

    if (bIsOnCooldown != bWasOnCooldown)
    {
        bWasOnCooldown = bIsOnCooldown;
        OnCooldownStateChanged.Broadcast(bIsOnCooldown, GetCooldownRemaining());

        UE_LOG(LogCombatComponent, Verbose,
            TEXT("[%s] Cooldown %s"),
            *GetOwner()->GetName(),
            bIsOnCooldown ? TEXT("started") : TEXT("ended"));
    }
}

// ============================================================================
// FIRE FUNCTIONS
// ============================================================================

ABaseProjectile* UAC_CombatComponent::FireProjectile()
{
    return FireProjectileClass(DefaultProjectileClass);
}

ABaseProjectile* UAC_CombatComponent::FireProjectileByType(FName TypeName)
{
    // Look up class in map
    TSubclassOf<ABaseProjectile>* FoundClass = ProjectileClassMap.Find(TypeName);

    if (!FoundClass || !(*FoundClass))
    {
        UE_LOG(LogCombatComponent, Warning,
            TEXT("[%s] No projectile class mapped for type '%s'"),
            *GetOwner()->GetName(), *TypeName.ToString());

        BroadcastFireBlocked(EFireBlockedReason::NoProjectileClass, TypeName);
        return nullptr;
    }

    return SpawnProjectileInternal(*FoundClass, TypeName);
}

ABaseProjectile* UAC_CombatComponent::FireProjectileClass(TSubclassOf<ABaseProjectile> ProjectileClass)
{
    return SpawnProjectileInternal(ProjectileClass, NAME_None);
}

// ============================================================================
// INTERNAL SPAWN LOGIC
// ============================================================================

ABaseProjectile* UAC_CombatComponent::SpawnProjectileInternal(
    TSubclassOf<ABaseProjectile> ProjectileClass, FName TypeName)
{
    // Validate world
    if (!GetWorld() || !GetOwner())
    {
        BroadcastFireBlocked(EFireBlockedReason::SpawnFailed, TypeName);
        return nullptr;
    }

    // Check projectile class
    if (!ProjectileClass)
    {
        UE_LOG(LogCombatComponent, Warning,
            TEXT("[%s] FireProjectile: No projectile class specified"),
            *GetOwner()->GetName());

        BroadcastFireBlocked(EFireBlockedReason::NoProjectileClass, TypeName);
        return nullptr;
    }

    // Check cooldown
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastFireTime < FireCooldown)
    {
        UE_LOG(LogCombatComponent, Verbose,
            TEXT("[%s] Fire blocked: On cooldown (%.2fs remaining)"),
            *GetOwner()->GetName(), FireCooldown - (CurrentTime - LastFireTime));

        BroadcastFireBlocked(EFireBlockedReason::OnCooldown, TypeName);
        return nullptr;
    }

    // Check aim blocked
    if (bRespectAimBlocked && AimComponent && AimComponent->IsAimBlocked())
    {
        UE_LOG(LogCombatComponent, Verbose,
            TEXT("[%s] Fire blocked: Aim blocked (too close to wall)"),
            *GetOwner()->GetName());

        BroadcastFireBlocked(EFireBlockedReason::AimBlocked, TypeName);
        return nullptr;
    }

    // Get spawn location
    FVector SpawnLocation = CalculateMuzzleLocation();

    // Get aim point and calculate trajectory-corrected direction
    FVector AimPoint;
    FVector FireDirection;

    if (AimComponent)
    {
        // Request fresh aim data
        AimComponent->RequestAimUpdate();

        // Get aim point from component
        AimPoint = AimComponent->GetAimHitLocation();

        // TRAJECTORY CORRECTION
        // Calculate direction from muzzle to where crosshair actually points
        FireDirection = AimComponent->GetAimDirectionFromLocation(SpawnLocation);
    }
    else
    {
        // Fallback: fire straight forward from owner
        UE_LOG(LogCombatComponent, Warning,
            TEXT("[%s] No AimComponent - firing straight forward"),
            *GetOwner()->GetName());

        AimPoint = SpawnLocation + (GetOwner()->GetActorForwardVector() * 10000.0f);
        FireDirection = GetOwner()->GetActorForwardVector();
    }

    // Convert direction to rotation
    FRotator SpawnRotation = FireDirection.Rotation();

    // Configure spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = Cast<APawn>(GetOwner());
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Spawn the projectile
    ABaseProjectile* Projectile = GetWorld()->SpawnActor<ABaseProjectile>(
        ProjectileClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (!Projectile)
    {
        UE_LOG(LogCombatComponent, Error,
            TEXT("[%s] SpawnActor failed for projectile"),
            *GetOwner()->GetName());

        BroadcastFireBlocked(EFireBlockedReason::SpawnFailed, TypeName);
        return nullptr;
    }
    Projectile->InitializeProjectile(GetOwner(), FireDirection);

    // Set projectile velocity
    UProjectileMovementComponent* MoveComp =
        Projectile->FindComponentByClass<UProjectileMovementComponent>();

    if (MoveComp)
    {
        MoveComp->Velocity = FireDirection * MoveComp->InitialSpeed;
    }

    // Update cooldown
    LastFireTime = CurrentTime;
    bWasOnCooldown = true;
    OnCooldownStateChanged.Broadcast(true, FireCooldown);

    // Broadcast success
    OnProjectileFired.Broadcast(Projectile, TypeName, FireDirection);

    UE_LOG(LogCombatComponent, Log,
        TEXT("[%s] Fired projectile | Type: %s | Location: %s | Direction: %s"),
        *GetOwner()->GetName(),
        *TypeName.ToString(),
        *SpawnLocation.ToString(),
        *FireDirection.ToString());

    return Projectile;
}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

bool UAC_CombatComponent::CanFire() const
{
    if (!GetWorld())
    {
        return false;
    }

    // Check cooldown
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastFireTime < FireCooldown)
    {
        return false;
    }

    // Check aim blocked
    if (bRespectAimBlocked && AimComponent && AimComponent->IsAimBlocked())
    {
        return false;
    }

    // Check default projectile exists
    if (!DefaultProjectileClass)
    {
        return false;
    }

    return true;
}

bool UAC_CombatComponent::CanFireType(FName TypeName) const
{
    // Check base conditions
    if (!GetWorld())
    {
        return false;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastFireTime < FireCooldown)
    {
        return false;
    }

    if (bRespectAimBlocked && AimComponent && AimComponent->IsAimBlocked())
    {
        return false;
    }

    // Check type exists in map
    const TSubclassOf<ABaseProjectile>* FoundClass = ProjectileClassMap.Find(TypeName);
    return FoundClass && (*FoundClass);
}

float UAC_CombatComponent::GetCooldownRemaining() const
{
    if (!GetWorld())
    {
        return 0.0f;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float Elapsed = CurrentTime - LastFireTime;
    float Remaining = FireCooldown - Elapsed;

    return FMath::Max(0.0f, Remaining);
}

float UAC_CombatComponent::GetCooldownProgress() const
{
    if (FireCooldown <= 0.0f)
    {
        return 1.0f;
    }

    float Remaining = GetCooldownRemaining();
    return 1.0f - (Remaining / FireCooldown);
}

FVector UAC_CombatComponent::GetMuzzleLocation() const
{
    return CalculateMuzzleLocation();
}

FRotator UAC_CombatComponent::GetMuzzleRotation() const
{
    // Get direction from aim component or owner forward
    if (AimComponent)
    {
        return AimComponent->GetAimDirection().Rotation();
    }

    return GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

void UAC_CombatComponent::SetAimComponent(UAC_AimComponent* InAimComponent)
{
    AimComponent = InAimComponent;

    UE_LOG(LogCombatComponent, Display,
        TEXT("[%s] AimComponent set: %s"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
        AimComponent ? TEXT("Valid") : TEXT("Null"));
}

void UAC_CombatComponent::SetDefaultProjectileClass(TSubclassOf<ABaseProjectile> InClass)
{
    DefaultProjectileClass = InClass;

    UE_LOG(LogCombatComponent, Display,
        TEXT("[%s] DefaultProjectileClass set: %s"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
        InClass ? *InClass->GetName() : TEXT("None"));
}

void UAC_CombatComponent::SetProjectileClassForType(FName TypeName, TSubclassOf<ABaseProjectile> InClass)
{
    ProjectileClassMap.Add(TypeName, InClass);

    UE_LOG(LogCombatComponent, Display,
        TEXT("[%s] ProjectileClassMap updated: '%s' -> %s"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
        *TypeName.ToString(),
        InClass ? *InClass->GetName() : TEXT("None"));
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

void UAC_CombatComponent::FindAimComponent()
{
    if (!GetOwner())
    {
        return;
    }

    // Look for aim component on owner
    AimComponent = GetOwner()->FindComponentByClass<UAC_AimComponent>();

    if (!AimComponent)
    {
        UE_LOG(LogCombatComponent, Warning,
            TEXT("[%s] No AC_AimComponent found on owner - trajectory correction disabled"),
            *GetOwner()->GetName());
    }
}

FVector UAC_CombatComponent::CalculateMuzzleLocation() const
{
    if (!GetOwner())
    {
        return FVector::ZeroVector;
    }

    // Priority 1: Skeletal mesh socket
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
        if (Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
        {
            return Mesh->GetSocketLocation(MuzzleSocketName);
        }
    }

    // Priority 2: MuzzlePointComponent
    if (MuzzlePointComponent)
    {
        return MuzzlePointComponent->GetComponentLocation();
    }

    // Priority 3: Calculated fallback
    UE_LOG(LogCombatComponent, Warning,
        TEXT("[%s] No muzzle socket or component - using calculated fallback"),
        *GetOwner()->GetName());

    return GetOwner()->GetActorLocation()
        + (GetOwner()->GetActorForwardVector() * MuzzleOffset.X)
        + (GetOwner()->GetActorRightVector() * MuzzleOffset.Y)
        + (FVector::UpVector * MuzzleOffset.Z);
}

void UAC_CombatComponent::BroadcastFireBlocked(EFireBlockedReason Reason, FName TypeName)
{
    OnFireBlocked.Broadcast(Reason, TypeName);
}