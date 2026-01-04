
// ============================================================================
// WizardJamPlayerController.h
// Developer: Marcus Daley
// Date: December 27, 2025
// ============================================================================
// Purpose:
// Custom PlayerController that creates and owns the player's HUD widget.
//
// - PlayerController owns UI that displays on the player's viewport
// - HUD persists even when pawn dies and respawns
// - Each player in multiplayer automatically gets their own HUD
//
// Setup Requirements:
// 1. Create this class in your project
// 2. Create BP_WizardJamPlayerController (Blueprint child class)
// 3. In BP_WizardJamPlayerController Details panel, set HUDWidgetClass to WBP_WizardJamHUD
// 4. In BP_WizardJamGameMode Details panel, set Player Controller Class to BP_WizardJamPlayerController
//
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WizardJamPlayerController.generated.h"

class UWizardJamHUDWidget;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
DECLARE_LOG_CATEGORY_EXTERN(LogWizardJamController, Log, All);

// ============================================================================
// WIZARDJAM PLAYER CONTROLLER
// ============================================================================

UCLASS()
class WIZARDJAM_API AWizardJamPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    AWizardJamPlayerController();

    // ========================================================================
    // HUD ACCESS
    // Other classes can query the HUD through the Controller
    // ========================================================================

    // Returns the player's HUD widget instance
    // Returns nullptr if HUD hasn't been created yet
    UFUNCTION(BlueprintPure, Category = "UI")
    UWizardJamHUDWidget* GetHUDWidget() const { return HUDWidgetInstance; }
    UFUNCTION(BlueprintCallable, Category = "Input|Flight")
    void EnableFlightInput();

    // Call this when flight mode is toggled off
    UFUNCTION(BlueprintCallable, Category = "Input|Flight")
    void DisableFlightInput();
protected:
    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    // BeginPlay is called when the Controller enters the world
    // This is where we create and display the HUD
    virtual void BeginPlay() override;

    // EndPlay is called when the Controller is destroyed
    // This is where we remove the HUD from viewport
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========================================================================
    // DESIGNER-CONFIGURABLE PROPERTIES
    // ========================================================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
    TSubclassOf<UWizardJamHUDWidget> HUDWidgetClass;
    // Flight Input Mapping Context 
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight")
    UInputMappingContext* FlightMappingContext;

    // Priority for flight context (higher = overrides default movement)
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight")
    int32 FlightContextPriority;
private:

    bool bFlightInputActive;
    UPROPERTY()
    UWizardJamHUDWidget* HUDWidgetInstance;

    // ========================================================================
    // INTERNAL HELPERS
    // ========================================================================

    // Creates the HUD widget and adds to viewport
    // Called once in BeginPlay
    // Safe to call multiple times (checks if already created)
    void CreateHUDWidget();

    // Removes HUD widget from viewport and clears reference
    // Called once in EndPlay
    // Safe to call multiple times (checks if widget exists)
    void DestroyHUDWidget();
};