// ============================================================================
// WizardJamPlayerController.cpp
// Developer: Marcus Daley
// Date: December 27, 2025
// ============================================================================
// Purpose:
// Implementation of the PlayerController that creates and owns the player HUD.
//
// How CreateWidget Works:
// CreateWidget<T>(PlayerController, WidgetClass) does three things:
// 1. Allocates memory for a new widget instance
// 2. Sets the OwningPlayer to the provided PlayerController
// 3. Calls the widget's Initialize() and NativeConstruct()
// ============================================================================

#include "Code/GameMode/WizardJamPlayerController.h"
#include "Code/Utility/WizardJamHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
DEFINE_LOG_CATEGORY(LogWizardJamController);

// ============================================================================
// CONSTRUCTOR
// ============================================================================
AWizardJamPlayerController::AWizardJamPlayerController()
    : FlightMappingContext(nullptr)
    , FlightContextPriority(1)
    , bFlightInputActive(false)
{
}

void AWizardJamPlayerController::EnableFlightInput()
{
    if (bFlightInputActive || !FlightMappingContext)
    {
        return; // Already active or no context assigned
    }

    // Get Enhanced Input subsystem
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // Add flight mapping context with configured priority
        Subsystem->AddMappingContext(FlightMappingContext, FlightContextPriority);
        bFlightInputActive = true;

        UE_LOG(LogTemp, Log, TEXT("[%s] Flight input context enabled"), *GetName());
    }
}

void AWizardJamPlayerController::DisableFlightInput()
{
    if (!bFlightInputActive || !FlightMappingContext)
    {
        return; // Already inactive or no context assigned
    }

    // Get Enhanced Input subsystem
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        // Remove flight mapping context
        Subsystem->RemoveMappingContext(FlightMappingContext);
        bFlightInputActive = false;

        UE_LOG(LogTemp, Log, TEXT("[%s] Flight input context disabled"), *GetName());
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AWizardJamPlayerController::BeginPlay()
{
    // ALWAYS call parent BeginPlay first
    // Parent class may have important initialization logic
    Super::BeginPlay();

    // Create the HUD widget
    // This creates an instance of HUDWidgetClass and adds it to viewport
    CreateHUDWidget();

    UE_LOG(LogWizardJamController, Display,
        TEXT("[%s] WizardJamPlayerController BeginPlay complete"),
        *GetName());
}

void AWizardJamPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // ========================================================================
    // WIDGET CLEANUP
    // Remove widget from viewport and clear our reference
    // This prevents:
    // - Memory leaks from orphaned widgets
    // - Crashes from accessing destroyed widgets
    // - Visual artifacts from widgets staying on screen
    // ========================================================================

    DestroyHUDWidget();

    // ALWAYS call parent EndPlay last
    // This ensures our cleanup runs before parent destroys anything we depend on
    Super::EndPlay(EndPlayReason);

    UE_LOG(LogWizardJamController, Display,
        TEXT("[%s] WizardJamPlayerController EndPlay complete"),
        *GetName());
}

// ============================================================================
// HUD WIDGET MANAGEMENT
// ============================================================================

void AWizardJamPlayerController::CreateHUDWidget()
{
    // Check if widget already exists 
    if (HUDWidgetInstance)
    {
        UE_LOG(LogWizardJamController, Warning,
            TEXT("[%s] HUD widget already exists - skipping creation"),
            *GetName());
        return;
    }

    // Check if widget class is assigned
    // If null, designer forgot to set HUDWidgetClass in Blueprint
    if (!HUDWidgetClass)
    {
        UE_LOG(LogWizardJamController, Error,
            TEXT("[%s] HUDWidgetClass is not set! Cannot create HUD."),
            *GetName());
        UE_LOG(LogWizardJamController, Error,
            TEXT("       Fix: Open BP_WizardJamPlayerController in editor"));
        UE_LOG(LogWizardJamController, Error,
            TEXT("       Set 'HUD Widget Class' to WBP_WizardJamHUD"));
        return;
    }

    // Check if this is a local controller (has a viewport)
    // In multiplayer, remote players don't have local viewports
    // We only create HUD for the local player
    if (!IsLocalController())
    {
        UE_LOG(LogWizardJamController, Log,
            TEXT("[%s] Not a local controller - skipping HUD creation"),
            *GetName());
        return;
    }

    // ========================================================================
    // CREATE THE WIDGET
    // CreateWidget<T> is the Unreal function for instantiating widgets
    // ========================================================================

    // CreateWidget takes:
    // - this: The owning player controller (used for input routing and viewport)
    // - HUDWidgetClass: The Blueprint class to instantiate
    //
    // After this call:
    // - Widget exists in memory
    // - Widget's NativeConstruct() has been called (this is where binding happens)
    // - Widget is NOT yet visible (needs AddToViewport)
    HUDWidgetInstance = CreateWidget<UWizardJamHUDWidget>(this, HUDWidgetClass);

    // Verify creation succeeded
    if (!HUDWidgetInstance)
    {
        UE_LOG(LogWizardJamController, Error,
            TEXT("[%s] CreateWidget failed! Check that HUDWidgetClass is valid."),
            *GetName());
        return;
    }

    // ========================================================================
    // ADD TO VIEWPORT
    // This makes the widget visible on the player's screen
    // ========================================================================

    // AddToViewport takes an optional ZOrder parameter
    // Lower ZOrder = rendered first (behind other widgets)
    // Higher ZOrder = rendered last (in front of other widgets)
    // Default (0) is fine for HUD since it's usually the only widget
    HUDWidgetInstance->AddToViewport();

    UE_LOG(LogWizardJamController, Display,
        TEXT("[%s] HUD widget created and added to viewport | Class: %s"),
        *GetName(),
        *HUDWidgetClass->GetName());
}

void AWizardJamPlayerController::DestroyHUDWidget()
{
    // ========================================================================
    // WIDGET CLEANUP
    // Properly removing a widget involves two steps:
    // 1. Remove from viewport (stops rendering)
    // 2. Clear reference (allows garbage collection)
    // ========================================================================

    if (HUDWidgetInstance)
    {
        // RemoveFromParent removes from viewport and from any parent container
        // After this call, widget is no longer visible but still exists in memory
        HUDWidgetInstance->RemoveFromParent();

        UE_LOG(LogWizardJamController, Display,
            TEXT("[%s] HUD widget removed from viewport"),
            *GetName());

        // Clear our reference
        // Since HUDWidgetInstance is UPROPERTY, clearing it allows garbage collection
        // The widget will be destroyed during the next GC cycle
        HUDWidgetInstance = nullptr;
    }
    else
    {
        UE_LOG(LogWizardJamController, Log,
            TEXT("[%s] No HUD widget to destroy"),
            *GetName());
    }
}