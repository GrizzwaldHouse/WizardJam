// AC_BroomComponent.h
// Broom flight system - spawn-on-demand with full HUD integration
//
// Developer: Marcus Daley
// Date: January 1, 2026
// Project: WizardJam
//
// FULL HUD INTEGRATION VERSION:
// - All delegates for WizardJamHUDWidget compatibility
// - OnStaminaVisualUpdate (color changes)
// - OnForcedDismount (depletion feedback)
// - OnBoostStateChanged (boost indicator)
// - GetFlightStaminaPercent() (stamina display)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "AC_BroomComponent.generated.h"

// Forward declarations
class UAC_StaminaComponent;
class UCharacterMovementComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogBroomComponent, Log, All);

// Broadcast when flight state changes (for UI: show/hide broom icon)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlightStateChanged, bool, bIsFlying);

// Broadcast when stamina bar color should change (cyan flying, orange boost, red depleted)
// NOTE: This is separate from WizardJamHUDWidget's version (different use case)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBroomStaminaVisualUpdate, FLinearColor, NewColor);

// Broadcast when forced to dismount due to stamina depletion
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForcedDismount);

// Broadcast when boost state changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoostStateChanged, bool, bIsBoosting);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WIZARDJAM_API UAC_BroomComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAC_BroomComponent();

    // ========================================================================
    // PUBLIC API
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Broom")
    void SetFlightEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Broom")
    bool IsFlying() const { return bIsFlying; }

    UFUNCTION(BlueprintPure, Category = "Broom")
    bool IsBoosting() const { return bIsBoosting; }

    // Get stamina as percentage (0.0 - 1.0) for HUD display
    UFUNCTION(BlueprintPure, Category = "Broom")
    float GetFlightStaminaPercent() const;

    // ========================================================================
    // DELEGATES
    // ========================================================================

    // Broadcast when flight state changes
    UPROPERTY(BlueprintAssignable, Category = "Broom|Events")
    FOnFlightStateChanged OnFlightStateChanged;

    // Broadcast when stamina bar color should change
    UPROPERTY(BlueprintAssignable, Category = "Broom|Events")
    FOnBroomStaminaVisualUpdate OnStaminaVisualUpdate;

    // Broadcast when forced to dismount
    UPROPERTY(BlueprintAssignable, Category = "Broom|Events")
    FOnForcedDismount OnForcedDismount;

    // Broadcast when boost state changes
    UPROPERTY(BlueprintAssignable, Category = "Broom|Events")
    FOnBoostStateChanged OnBoostStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // BROOM VISUAL SPAWNING
    // ========================================================================

    void SpawnBroomVisual();
    void DestroyBroomVisual();

    // ========================================================================
    // INPUT BINDING
    // ========================================================================

    void SetupFlightInputBindings(UEnhancedInputComponent* EnhancedInput);

    // ========================================================================
    // INPUT HANDLERS
    // ========================================================================

    UFUNCTION()
    void HandleToggleInput(const FInputActionValue& Value);

    UFUNCTION()
    void HandleAscendInput(const FInputActionValue& Value);

    UFUNCTION()
    void HandleDescendInput(const FInputActionValue& Value);

    UFUNCTION()
    void HandleBoostInput(const FInputActionValue& Value);

    // ========================================================================
    // FLIGHT MECHANICS
    // ========================================================================

    void ApplyVerticalMovement(float DeltaTime);
    void SetMovementMode(bool bFlying);
    void UpdateInputContext(bool bAddContext);
    bool HasSufficientStamina() const;
    void ForceDismount();

    // ========================================================================
    // STAMINA INTEGRATION
    // ========================================================================

    void DrainStamina(float DeltaTime);

    // Callback when stamina changes
    // MUST match AC_StaminaComponent::OnStaminaChanged signature:
    // DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStaminaChanged, AActor*, Owner, float, NewStamina, float, Delta)
    UFUNCTION()
    void OnStaminaChanged(AActor* Owner, float NewStamina, float Delta);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Visuals")
    TSubclassOf<AActor> BroomVisualClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Input")
    UInputMappingContext* FlightMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Input")
    UInputAction* ToggleAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Input")
    UInputAction* AscendAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Input")
    UInputAction* DescendAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Input")
    UInputAction* BoostAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Flight")
    float FlySpeed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Flight")
    float BoostSpeed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Flight")
    float VerticalSpeed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Stamina")
    float StaminaDrainRate;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Stamina")
    float BoostStaminaDrainRate;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Stamina")
    float MinStaminaToFly;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Visuals")
    FName MountSocketName;

    // ========================================================================
    // RUNTIME STATE
    // ========================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|State")
    bool bIsFlying;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|State")
    bool bIsBoosting;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|State")
    float CurrentVerticalVelocity;

    // ========================================================================
    // SPAWNED ACTOR REFERENCE
    // ========================================================================

    UPROPERTY()
    AActor* SpawnedBroomVisual;

    // ========================================================================
    // COMPONENT REFERENCES
    // ========================================================================

    UPROPERTY()
    UAC_StaminaComponent* StaminaComponent;

    UPROPERTY()
    UCharacterMovementComponent* MovementComponent;

    UPROPERTY()
    APlayerController* PlayerController;

    UPROPERTY()
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem;
};