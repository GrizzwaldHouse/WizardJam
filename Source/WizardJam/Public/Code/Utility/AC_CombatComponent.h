// ============================================================================
// AC_CombatComponent.h
// Developer: Marcus Daley
// Date: December 29, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Modular combat component handling projectile spawning with trajectory
// correction. Works with AC_AimComponent to fire projectiles toward the
// crosshair aim point. Attach to ANY character that needs ranged combat.
//
// Architecture Philosophy (95/5 Rule):
// This component is 95% reusable:
// - Handles muzzle point positioning
// - Performs trajectory correction math
// - Manages fire rate cooldowns
// - Broadcasts combat events
//
// The 5% project-specific part is the ProjectileClassMap, which maps
// attack types (FName) to projectile classes. The owning character
// configures this mapping.
//
// Separation of Concerns:
// - AC_AimComponent: "Where is the target?" (raycast, world position)
// - AC_CombatComponent: "How do I hit it?" (spawn, trajectory, cooldown)
// - Character: "What do I fire?" (projectile class selection)
//
// MuzzlePoint Design:
// The component creates a child USceneComponent for the muzzle point.
// This works without skeletal mesh. When skeletal mesh is added later,
// configure SocketName and the component auto-detects it.
//
// Observer Pattern:
// - OnProjectileFired: Broadcast when projectile spawns (for VFX, sound)
// - OnFireBlocked: Broadcast when fire attempt fails (for UI feedback)
// - OnCooldownStarted/Ended: For UI cooldown indicators
//
// Usage:
// 1. Add AC_CombatComponent to character
// 2. Add AC_AimComponent to same character
// 3. In character Blueprint, set ProjectileClass or populate ProjectileClassMap
// 4. Call FireProjectile() or FireProjectileByType() from input binding
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_CombatComponent.generated.h"

// Forward declarations
class UAC_AimComponent;
class USceneComponent;
class USkeletalMeshComponent;
class ABaseProjectile;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatComponent, Log, All);

// ============================================================================
// ENUMS
// ============================================================================

// Reason why fire attempt failed
UENUM(BlueprintType)
enum class EFireBlockedReason : uint8
{
    OnCooldown       UMETA(DisplayName = "On Cooldown"),
    NoProjectileClass UMETA(DisplayName = "No Projectile Class"),
    AimBlocked       UMETA(DisplayName = "Aim Blocked (Too Close to Wall)"),
    NoAimComponent   UMETA(DisplayName = "No Aim Component"),
    SpawnFailed      UMETA(DisplayName = "Projectile Spawn Failed"),
    Custom           UMETA(DisplayName = "Custom Reason")
};

// ============================================================================
// DELEGATES (Observer Pattern)
// ============================================================================

// Broadcast when projectile successfully fires
// Listeners: VFX systems, audio, animation, ammo UI
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnProjectileFired,
    ABaseProjectile*, Projectile,
    FName, ProjectileType,
    FVector, FireDirection
);

// Broadcast when fire attempt is blocked
// Listeners: UI feedback, input buffer systems
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnFireBlocked,
    EFireBlockedReason, Reason,
    FName, AttemptedType
);

// Broadcast when cooldown state changes
// Listeners: UI cooldown indicators
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCooldownStateChanged,
    bool, bIsOnCooldown,
    float, RemainingTime
);

// ============================================================================
// COMBAT COMPONENT CLASS
// ============================================================================

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WIZARDJAM_API UAC_CombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAC_CombatComponent();

    // ========================================================================
    // PRIMARY INTERFACE - Fire Functions
    // ========================================================================

    // Fire using default projectile class
    // Returns spawned projectile or nullptr if failed
    UFUNCTION(BlueprintCallable, Category = "Combat|Fire")
    ABaseProjectile* FireProjectile();

    // Fire specific projectile type from map
    // TypeName maps to ProjectileClassMap entry
    UFUNCTION(BlueprintCallable, Category = "Combat|Fire")
    ABaseProjectile* FireProjectileByType(FName TypeName);

    // Fire specific projectile class directly
    // Bypasses map lookup - use for one-off special attacks
    UFUNCTION(BlueprintCallable, Category = "Combat|Fire")
    ABaseProjectile* FireProjectileClass(TSubclassOf<ABaseProjectile> ProjectileClass);

    // ========================================================================
    // QUERY FUNCTIONS
    // ========================================================================

    // Check if component can fire (cooldown, aim, etc.)
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    bool CanFire() const;

    // Check if specific type can fire
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    bool CanFireType(FName TypeName) const;

    // Get remaining cooldown time (0 if ready)
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    float GetCooldownRemaining() const;

    // Get cooldown progress (0.0 = just fired, 1.0 = ready)
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    float GetCooldownProgress() const;

    // Get muzzle world location
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    FVector GetMuzzleLocation() const;

    // Get muzzle world rotation
    UFUNCTION(BlueprintPure, Category = "Combat|Query")
    FRotator GetMuzzleRotation() const;

    // ========================================================================
    // CONFIGURATION - Runtime Setters
    // ========================================================================

    // Set the aim component reference (auto-detected in BeginPlay if not set)
    UFUNCTION(BlueprintCallable, Category = "Combat|Config")
    void SetAimComponent(UAC_AimComponent* InAimComponent);

    // Set default projectile class
    UFUNCTION(BlueprintCallable, Category = "Combat|Config")
    void SetDefaultProjectileClass(TSubclassOf<ABaseProjectile> InClass);

    // Add or update projectile class mapping
    UFUNCTION(BlueprintCallable, Category = "Combat|Config")
    void SetProjectileClassForType(FName TypeName, TSubclassOf<ABaseProjectile> InClass);

    // ========================================================================
    // DELEGATES - Bind for Observer pattern
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnProjectileFired OnProjectileFired;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnFireBlocked OnFireBlocked;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCooldownStateChanged OnCooldownStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // MUZZLE POINT CONFIGURATION
    // ========================================================================

    // Socket name on skeletal mesh (used if mesh exists and has socket)
    // If socket doesn't exist, falls back to MuzzlePointComponent
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Muzzle")
    FName MuzzleSocketName;

    // Offset for MuzzlePointComponent when no socket exists
    // X = Forward, Y = Right, Z = Up (relative to owner)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Muzzle")
    FVector MuzzleOffset;

    // ========================================================================
    // FIRE RATE CONFIGURATION
    // ========================================================================

    // Minimum time between shots (seconds)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|FireRate",
        meta = (ClampMin = "0.05"))
    float FireCooldown;

    // If true, prevents firing when AimComponent reports blocked
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|FireRate")
    bool bRespectAimBlocked;

    // ========================================================================
    // PROJECTILE CONFIGURATION
    // ========================================================================

    // Default projectile class (used by FireProjectile())
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
    TSubclassOf<ABaseProjectile> DefaultProjectileClass;

    // Map of type names to projectile classes
    // Designer populates: "Flame" -> BP_Projectile_Flame, etc.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
    TMap<FName, TSubclassOf<ABaseProjectile>> ProjectileClassMap;

private:
    // ========================================================================
    // COMPONENT REFERENCES
    // ========================================================================

    // Child component for muzzle position (created in constructor)
    UPROPERTY()
    USceneComponent* MuzzlePointComponent;

    // Reference to aim component on same actor
    UPROPERTY()
    UAC_AimComponent* AimComponent;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    // Timestamp of last fire
    float LastFireTime;

    // Was on cooldown last frame (for state change detection)
    bool bWasOnCooldown;

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    // Find aim component on owner
    void FindAimComponent();

    // Get the actual muzzle location (socket or component)
    FVector CalculateMuzzleLocation() const;

    // Spawn projectile with trajectory correction
    ABaseProjectile* SpawnProjectileInternal(TSubclassOf<ABaseProjectile> ProjectileClass,
        FName TypeName);

    // Broadcast fire blocked with reason
    void BroadcastFireBlocked(EFireBlockedReason Reason, FName TypeName);
};