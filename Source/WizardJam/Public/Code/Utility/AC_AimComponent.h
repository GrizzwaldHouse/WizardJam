// ============================================================================
// AC_AimComponent.h
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Modular aiming component that performs raycasts from camera through screen
// center to determine world-space aim points. Attach to ANY character that
// needs aiming capability - player, companion, or AI.
//
// Architecture Philosophy (95/5 Rule):
// This component is 95% reusable across any project. It knows nothing about
// spells, projectiles, or game-specific concepts. It only answers the question:
// "Where in the world is this character aiming?"
//
// Observer Pattern Implementation:
// This component does NOT poll. Instead:
// 1. External systems call RequestAimUpdate() when they need fresh data
// 2. Component performs raycast and caches result
// 3. If target changed, broadcasts OnAimTargetChanged
// 4. If location significantly changed, broadcasts OnAimLocationUpdated
// 
// The component can also be configured for periodic updates via tick, but
// this is opt-in and broadcasts changes rather than requiring polling.
//
// Trajectory Correction Explained:
// Third-person cameras create parallax - crosshair aim point differs from
// where a projectile would hit if fired straight forward from muzzle.
// Solution: Raycast from camera, get world hit point, then calculate
// direction from muzzle TO that point. Projectile hits where crosshair shows.
//
// Usage For Different Character Types:
// - Player: Raycast from camera through screen center
// - Companion: Can share player aim point or use own forward vector
// - AI: Uses actor forward vector (no camera)
//
// Designer Configuration:
// 1. Add component to character Blueprint
// 2. Set MaxTraceDistance for raycast range
// 3. Set TraceChannel (typically ECC_Visibility)
// 4. Configure CrosshairScreenPosition if not using center
// 5. Optionally enable bAutoUpdateOnTick for periodic broadcasts
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_AimComponent.generated.h"

// Forward declarations keep header lightweight
class APlayerController;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogAimComponent, Log, All);

// ============================================================================
// ENUMS
// ============================================================================

// Classifies what the aim raycast hit - used for crosshair color decisions
UENUM(BlueprintType)
enum class EAimTraceResult : uint8
{
    Nothing      UMETA(DisplayName = "Nothing (Sky/Max Range)"),
    World        UMETA(DisplayName = "World Geometry"),
    Friendly     UMETA(DisplayName = "Friendly Actor"),
    Enemy        UMETA(DisplayName = "Enemy Actor"),
    Interactable UMETA(DisplayName = "Interactable Object"),
    Self         UMETA(DisplayName = "Self (Blocked)")
};

// ============================================================================
// STRUCTS
// ============================================================================

// Complete aim state data - returned by GetAimData()
USTRUCT(BlueprintType)
struct FAimTraceData
{
    GENERATED_BODY()

    // World location where crosshair points (raycast hit or max distance)
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    FVector AimLocation;

    // Normalized direction from owner actor to aim location
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    FVector AimDirection;

    // Actor under crosshair (nullptr if aiming at world/nothing)
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    AActor* HitActor;

    // Classification of what was hit
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    EAimTraceResult TraceResult;

    // Distance from trace start to hit point
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    float HitDistance;

    // Surface normal at hit point (useful for decals/effects)
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    FVector HitNormal;

    // Physical material name for VFX selection
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    FName PhysicalSurface;

    // Whether the trace hit anything
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    bool bDidHit;

    // World time when this data was captured
    UPROPERTY(BlueprintReadOnly, Category = "Aim")
    float Timestamp;

    FAimTraceData()
        : AimLocation(FVector::ZeroVector)
        , AimDirection(FVector::ForwardVector)
        , HitActor(nullptr)
        , TraceResult(EAimTraceResult::Nothing)
        , HitDistance(0.0f)
        , HitNormal(FVector::UpVector)
        , PhysicalSurface(NAME_None)
        , bDidHit(false)
        , Timestamp(0.0f)
    {
    }
};

// ============================================================================
// DELEGATE DECLARATIONS (Observer Pattern)
// ============================================================================

// Broadcast when the actor under crosshair changes
// Listeners: HUD (crosshair color), AI (target acquisition)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAimTargetChanged,
    AActor*, NewTarget,
    EAimTraceResult, TargetType
);

// Broadcast when aim location moves significantly (threshold-based)
// Listeners: Laser sight effects, aim prediction systems
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAimLocationUpdated,
    FVector, NewAimLocation,
    FVector, AimDirection
);

// Broadcast when aim becomes blocked (too close to wall)
// Listeners: Combat systems (disable firing), HUD (blocked indicator)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnAimBlocked,
    bool, bIsBlocked
);

// ============================================================================
// AIM COMPONENT CLASS
// ============================================================================

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WIZARDJAM_API UAC_AimComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAC_AimComponent();

    // ========================================================================
    // PRIMARY INTERFACE - Query Functions
    // These return cached data. Call RequestAimUpdate() first if you need
    // guaranteed fresh data. Otherwise, cached data is usually sufficient.
    // ========================================================================

    // Get complete aim data structure (cached)
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    FAimTraceData GetAimData() const { return CachedAimData; }

    // Get world location where crosshair points (cached)
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    FVector GetAimHitLocation() const { return CachedAimData.AimLocation; }

    // Get normalized direction from owner to aim point (cached)
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    FVector GetAimDirection() const { return CachedAimData.AimDirection; }

    // Get direction from arbitrary world position to aim point
    // Use this for trajectory correction: pass muzzle location
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    FVector GetAimDirectionFromLocation(FVector StartLocation) const;

    // Get actor currently under crosshair (can be nullptr)
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    AActor* GetTargetActor() const { return CachedAimData.HitActor; }

    // Check if currently aiming at a specific actor
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    bool IsAimingAt(AActor* TestActor) const;

    // Check if aim is blocked (too close to geometry)
    UFUNCTION(BlueprintPure, Category = "Aim|Query")
    bool IsAimBlocked() const { return bAimIsBlocked; }

    // ========================================================================
    // UPDATE CONTROL - Observer Pattern
    // External systems request updates, component broadcasts changes
    // ========================================================================

    // Request fresh aim data - performs raycast and broadcasts if changed
    // Call this from systems that need current aim data
    // Returns the fresh aim data for convenience
    UFUNCTION(BlueprintCallable, Category = "Aim|Update")
    FAimTraceData RequestAimUpdate();

    // Force broadcast of current aim state (useful for late-binding listeners)
    UFUNCTION(BlueprintCallable, Category = "Aim|Update")
    void BroadcastCurrentState();

    // ========================================================================
    // DELEGATES - Bind to these for Observer pattern updates
    // ========================================================================

    // Fires when actor under crosshair changes
    UPROPERTY(BlueprintAssignable, Category = "Aim|Events")
    FOnAimTargetChanged OnAimTargetChanged;

    // Fires when aim location moves beyond threshold
    UPROPERTY(BlueprintAssignable, Category = "Aim|Events")
    FOnAimLocationUpdated OnAimLocationUpdated;

    // Fires when aim blocked state changes
    UPROPERTY(BlueprintAssignable, Category = "Aim|Events")
    FOnAimBlocked OnAimBlocked;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CONFIGURATION - Designer sets in Blueprint
    // ========================================================================

    // Maximum raycast distance (units)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    float MaxTraceDistance;

    // Collision channel for raycast
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    TEnumAsByte<ECollisionChannel> TraceChannel;

    // Screen position for crosshair (0.5, 0.5 = center)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    FVector2D CrosshairScreenPosition;

    // Minimum distance before aim is considered blocked
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    float MinAimDistance;

    // Distance threshold for OnAimLocationUpdated broadcast
    // Prevents spamming broadcasts for tiny movements
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    float LocationUpdateThreshold;

    // Actor tags that identify friendly actors
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    TArray<FName> FriendlyActorTags;

    // Actor tags that identify interactable objects
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|Config")
    TArray<FName> InteractableActorTags;

    // ========================================================================
    // AUTO-UPDATE CONFIGURATION
    // Enable for periodic broadcasts without manual RequestAimUpdate calls
    // ========================================================================

    // If true, component updates aim data periodically and broadcasts changes
    // If false, only updates when RequestAimUpdate() is called
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|AutoUpdate")
    bool bAutoUpdateOnTick;

    // Update interval when bAutoUpdateOnTick is true (seconds)
    // Lower = more responsive, higher = less CPU
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim|AutoUpdate",
        meta = (EditCondition = "bAutoUpdateOnTick", ClampMin = "0.016"))
    float AutoUpdateInterval;

private:
    // ========================================================================
    // CACHED STATE
    // ========================================================================

    // Most recent aim data
    FAimTraceData CachedAimData;

    // Previous target for change detection
    TWeakObjectPtr<AActor> PreviousTargetActor;

    // Previous aim location for threshold-based broadcasting
    FVector PreviousAimLocation;

    // Current blocked state
    bool bAimIsBlocked;

    // Timer for auto-update interval
    float AutoUpdateTimer;

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    // Perform raycast and populate aim data
    void PerformAimTrace(FAimTraceData& OutData) const;

    // Classify what type of actor was hit
    EAimTraceResult ClassifyHitActor(AActor* HitActor) const;

    // Get player controller (returns nullptr for AI characters)
    APlayerController* GetOwnerController() const;

    // Check if actor has any of the specified tags
    bool ActorHasAnyTag(AActor* Actor, const TArray<FName>& Tags) const;

    // Broadcast changes based on comparison with previous state
    void BroadcastChanges(const FAimTraceData& OldData, const FAimTraceData& NewData);
};