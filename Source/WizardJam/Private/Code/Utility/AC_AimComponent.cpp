// ============================================================================
// AC_AimComponent.cpp
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of modular aiming component using Observer pattern.
// Performs camera-based raycasting and broadcasts state changes via delegates.
//
// Key Architecture Decisions:
// - NO POLLING: External systems request updates or bind to delegates
// - Cached data with timestamps for efficient repeated queries
// - Threshold-based location broadcasts prevent delegate spam
// - Auto-update mode is OPT-IN for systems that need periodic updates
// ============================================================================

#include "Code/Utility/AC_AimComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAimComponent);

// ============================================================================
// CONSTRUCTOR
// All values initialized in list per coding standards
// ============================================================================

UAC_AimComponent::UAC_AimComponent()
    : MaxTraceDistance(10000.0f)
    , TraceChannel(ECC_Visibility)
    , CrosshairScreenPosition(0.5f, 0.5f)
    , MinAimDistance(50.0f)
    , LocationUpdateThreshold(10.0f)
    , bAutoUpdateOnTick(false)
    , AutoUpdateInterval(0.05f)
    , PreviousAimLocation(FVector::ZeroVector)
    , bAimIsBlocked(false)
    , AutoUpdateTimer(0.0f)
{
    // Tick is disabled by default - only enabled if bAutoUpdateOnTick is true
    // This follows the Observer pattern: update on request, not continuously
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    UE_LOG(LogAimComponent, Log,
        TEXT("AimComponent constructed | MaxDistance: %.0f | AutoUpdate: %s"),
        MaxTraceDistance, bAutoUpdateOnTick ? TEXT("ON") : TEXT("OFF"));
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAC_AimComponent::BeginPlay()
{
    Super::BeginPlay();

    // Validate owner exists
    if (!GetOwner())
    {
        UE_LOG(LogAimComponent, Error,
            TEXT("AimComponent has no owner - cannot perform aim traces"));
        return;
    }

    // Enable tick only if auto-update is configured
    if (bAutoUpdateOnTick)
    {
        SetComponentTickEnabled(true);
        UE_LOG(LogAimComponent, Display,
            TEXT("[%s] AimComponent auto-update ENABLED | Interval: %.3fs"),
            *GetOwner()->GetName(), AutoUpdateInterval);
    }
    else
    {
        UE_LOG(LogAimComponent, Display,
            TEXT("[%s] AimComponent ready | Auto-update OFF (request-based)"),
            *GetOwner()->GetName());
    }

    // Perform initial trace to populate cache
    PerformAimTrace(CachedAimData);
    PreviousAimLocation = CachedAimData.AimLocation;
    PreviousTargetActor = CachedAimData.HitActor;
}

void UAC_AimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Only tick if auto-update is enabled
    if (!bAutoUpdateOnTick)
    {
        return;
    }

    // Accumulate time for interval-based updates
    AutoUpdateTimer += DeltaTime;
    if (AutoUpdateTimer >= AutoUpdateInterval)
    {
        AutoUpdateTimer = 0.0f;
        RequestAimUpdate();
    }
}

// ============================================================================
// PRIMARY UPDATE FUNCTION
// This is the main entry point - external systems call this when they need
// fresh aim data. The function performs the raycast, updates cache, and
// broadcasts any changes via delegates.
// ============================================================================

FAimTraceData UAC_AimComponent::RequestAimUpdate()
{
    if (!GetOwner() || !GetWorld())
    {
        return CachedAimData;
    }

    // Store old data for comparison
    FAimTraceData OldData = CachedAimData;

    // Perform fresh raycast
    PerformAimTrace(CachedAimData);

    // Check blocked state change
    bool bWasBlocked = bAimIsBlocked;
    bAimIsBlocked = CachedAimData.bDidHit && CachedAimData.HitDistance < MinAimDistance;

    if (bWasBlocked != bAimIsBlocked)
    {
        OnAimBlocked.Broadcast(bAimIsBlocked);

        UE_LOG(LogAimComponent, Verbose,
            TEXT("[%s] Aim %s | Distance: %.1f"),
            *GetOwner()->GetName(),
            bAimIsBlocked ? TEXT("BLOCKED") : TEXT("clear"),
            CachedAimData.HitDistance);
    }

    // Broadcast any changes
    BroadcastChanges(OldData, CachedAimData);

    return CachedAimData;
}

// ============================================================================
// BROADCAST CURRENT STATE
// Useful for systems that bind to delegates after BeginPlay
// ============================================================================

void UAC_AimComponent::BroadcastCurrentState()
{
    // Broadcast target (even if nullptr)
    OnAimTargetChanged.Broadcast(CachedAimData.HitActor, CachedAimData.TraceResult);

    // Broadcast location
    OnAimLocationUpdated.Broadcast(CachedAimData.AimLocation, CachedAimData.AimDirection);

    // Broadcast blocked state
    OnAimBlocked.Broadcast(bAimIsBlocked);

    UE_LOG(LogAimComponent, Verbose,
        TEXT("[%s] Broadcast current state | Target: %s | Location: %s"),
        *GetOwner()->GetName(),
        CachedAimData.HitActor ? *CachedAimData.HitActor->GetName() : TEXT("None"),
        *CachedAimData.AimLocation.ToString());
}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

FVector UAC_AimComponent::GetAimDirectionFromLocation(FVector StartLocation) const
{
    // Calculate direction from arbitrary point to aim location
    // This is the key function for trajectory correction:
    // Pass muzzle location, get direction that hits where crosshair shows

    FVector AimPoint = CachedAimData.AimLocation;
    FVector Direction = (AimPoint - StartLocation).GetSafeNormal();

    // Safety: if aim point is behind start (wall clipping), use owner forward
    if (GetOwner())
    {
        float Dot = FVector::DotProduct(Direction, GetOwner()->GetActorForwardVector());
        if (Direction.IsNearlyZero() || Dot < 0.1f)
        {
            return GetOwner()->GetActorForwardVector();
        }
    }

    return Direction;
}

bool UAC_AimComponent::IsAimingAt(AActor* TestActor) const
{
    return TestActor && CachedAimData.HitActor == TestActor;
}

// ============================================================================
// RAYCAST IMPLEMENTATION
// ============================================================================

void UAC_AimComponent::PerformAimTrace(FAimTraceData& OutData) const
{
    // Initialize to defaults
    OutData = FAimTraceData();
    OutData.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (!GetOwner() || !GetWorld())
    {
        return;
    }

    FVector TraceStart;
    FVector TraceEnd;

    // Try to get player controller for camera-based aiming
    APlayerController* PC = GetOwnerController();

    if (PC)
    {
        // CAMERA-BASED AIMING (Player characters)
        // Raycast from camera through crosshair screen position

        int32 ViewportX, ViewportY;
        PC->GetViewportSize(ViewportX, ViewportY);

        FVector2D ScreenPos;
        ScreenPos.X = ViewportX * CrosshairScreenPosition.X;
        ScreenPos.Y = ViewportY * CrosshairScreenPosition.Y;

        FVector WorldLoc, WorldDir;
        bool bDeprojected = PC->DeprojectScreenPositionToWorld(
            ScreenPos.X, ScreenPos.Y, WorldLoc, WorldDir);

        if (bDeprojected)
        {
            TraceStart = WorldLoc;
            TraceEnd = WorldLoc + (WorldDir * MaxTraceDistance);
        }
        else
        {
            // Fallback: use camera location and forward
            FVector CamLoc;
            FRotator CamRot;
            PC->GetPlayerViewPoint(CamLoc, CamRot);

            TraceStart = CamLoc;
            TraceEnd = CamLoc + (CamRot.Vector() * MaxTraceDistance);

            UE_LOG(LogAimComponent, Warning,
                TEXT("[%s] Deprojection failed, using camera forward"),
                *GetOwner()->GetName());
        }
    }
    else
    {
        // NON-PLAYER AIMING (AI, Companions)
        // Use actor location and forward vector

        TraceStart = GetOwner()->GetActorLocation();
        TraceEnd = TraceStart + (GetOwner()->GetActorForwardVector() * MaxTraceDistance);
    }

    // Configure trace parameters
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.bReturnPhysicalMaterial = true;
    QueryParams.bTraceComplex = false;

    // Ignore attached actors (weapons, equipment, etc.)
    TArray<AActor*> AttachedActors;
    GetOwner()->GetAttachedActors(AttachedActors);
    QueryParams.AddIgnoredActors(AttachedActors);

    // Perform line trace
    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        TraceChannel,
        QueryParams
    );

    // Populate output data
    if (bHit)
    {
        OutData.bDidHit = true;
        OutData.AimLocation = HitResult.ImpactPoint;
        OutData.HitActor = HitResult.GetActor();
        OutData.HitDistance = HitResult.Distance;
        OutData.HitNormal = HitResult.ImpactNormal;
        OutData.TraceResult = ClassifyHitActor(HitResult.GetActor());

        if (HitResult.PhysMaterial.IsValid())
        {
            OutData.PhysicalSurface = HitResult.PhysMaterial->GetFName();
        }
    }
    else
    {
        OutData.bDidHit = false;
        OutData.AimLocation = TraceEnd;
        OutData.HitActor = nullptr;
        OutData.HitDistance = MaxTraceDistance;
        OutData.HitNormal = FVector::UpVector;
        OutData.TraceResult = EAimTraceResult::Nothing;
    }

    // Calculate aim direction from owner to aim point
    OutData.AimDirection = (OutData.AimLocation - GetOwner()->GetActorLocation()).GetSafeNormal();
}

// ============================================================================
// TARGET CLASSIFICATION
// ============================================================================

EAimTraceResult UAC_AimComponent::ClassifyHitActor(AActor* HitActor) const
{
    if (!HitActor)
    {
        return EAimTraceResult::World;
    }

    // Check interactable tags (highest priority)
    if (ActorHasAnyTag(HitActor, InteractableActorTags))
    {
        return EAimTraceResult::Interactable;
    }

    // Check friendly tags
    if (ActorHasAnyTag(HitActor, FriendlyActorTags))
    {
        return EAimTraceResult::Friendly;
    }

    // Use team interface for friend/foe detection
    IGenericTeamAgentInterface* OwnerTeam = Cast<IGenericTeamAgentInterface>(GetOwner());
    IGenericTeamAgentInterface* TargetTeam = Cast<IGenericTeamAgentInterface>(HitActor);

    if (OwnerTeam && TargetTeam)
    {
        ETeamAttitude::Type Attitude = OwnerTeam->GetTeamAttitudeTowards(*HitActor);

        switch (Attitude)
        {
        case ETeamAttitude::Friendly:
            return EAimTraceResult::Friendly;
        case ETeamAttitude::Hostile:
            return EAimTraceResult::Enemy;
        default:
            break;
        }
    }

    return EAimTraceResult::World;
}

// ============================================================================
// CHANGE DETECTION AND BROADCASTING
// ============================================================================

void UAC_AimComponent::BroadcastChanges(const FAimTraceData& OldData, const FAimTraceData& NewData)
{
    // Check target change
    AActor* OldTarget = PreviousTargetActor.Get();
    AActor* NewTarget = NewData.HitActor;

    if (OldTarget != NewTarget)
    {
        PreviousTargetActor = NewTarget;
        OnAimTargetChanged.Broadcast(NewTarget, NewData.TraceResult);

        UE_LOG(LogAimComponent, Verbose,
            TEXT("[%s] Target changed: %s -> %s (%s)"),
            *GetOwner()->GetName(),
            OldTarget ? *OldTarget->GetName() : TEXT("None"),
            NewTarget ? *NewTarget->GetName() : TEXT("None"),
            *UEnum::GetValueAsString(NewData.TraceResult));
    }

    // Check location change (threshold-based to prevent spam)
    float LocationDelta = FVector::Dist(PreviousAimLocation, NewData.AimLocation);
    if (LocationDelta > LocationUpdateThreshold)
    {
        PreviousAimLocation = NewData.AimLocation;
        OnAimLocationUpdated.Broadcast(NewData.AimLocation, NewData.AimDirection);

        UE_LOG(LogAimComponent, Verbose,
            TEXT("[%s] Aim location updated | Delta: %.1f | New: %s"),
            *GetOwner()->GetName(),
            LocationDelta,
            *NewData.AimLocation.ToString());
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

APlayerController* UAC_AimComponent::GetOwnerController() const
{
    if (!GetOwner())
    {
        return nullptr;
    }

    // Try pawn first
    if (APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }

    // Try character
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        return Cast<APlayerController>(Character->GetController());
    }

    return nullptr;
}

bool UAC_AimComponent::ActorHasAnyTag(AActor* Actor, const TArray<FName>& Tags) const
{
    if (!Actor || Tags.Num() == 0)
    {
        return false;
    }

    for (const FName& Tag : Tags)
    {
        if (Actor->ActorHasTag(Tag))
        {
            return true;
        }
    }

    return false;
}