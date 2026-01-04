// WizardJamHUDWidget.h
// Main HUD widget with broom flight integration
//
// Developer: Marcus Daley
// Date: January 1, 2026
// Project: WizardJam

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WizardJamHUDWidget.generated.h"

// Forward declarations
class UAC_HealthComponent;
class UAC_StaminaComponent;
class UAC_SpellCollectionComponent;
class UAC_BroomComponent;
class UProgressBar;
class UImage;
class UTextBlock;
class UPanelWidget;

UCLASS()
class WIZARDJAM_API UWizardJamHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ========================================================================
    // COMPONENT REFERENCES (Cached at BeginPlay)
    // ========================================================================

    UPROPERTY()
    AActor* OwnerActor;

    UPROPERTY()
    UAC_HealthComponent* HealthComp;

    UPROPERTY()
    UAC_StaminaComponent* StaminaComp;

    UPROPERTY()
    UAC_SpellCollectionComponent* SpellCollectionComp;

    UPROPERTY()
    UAC_BroomComponent* BroomComp;

    // ========================================================================
    // WIDGET REFERENCES (Bound in Blueprint)
    // Use BindWidget or BindWidgetOptional in Blueprint
    // ========================================================================

    // Health UI
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* HealthProgressBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* HealthText;

    // Stamina UI
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* StaminaProgressBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* StaminaText;

    // Spell Slot UI
    UPROPERTY(meta = (BindWidgetOptional))
    UPanelWidget* SpellSlotContainer;

    // Broom Flight UI
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* BroomIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* BoostIndicatorImage;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* OutOfStaminaWarningText;

    // Quidditch Scoreboard UI
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* PlayerScoreText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* AIScoreText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* MatchTimerText;

    // ========================================================================
    // COMPONENT INITIALIZATION
    // ========================================================================

    // Find and cache all actor components
    void CacheComponents();

    // Bind to all component delegates
    void BindComponentDelegates();

    // Unbind all delegates (called in NativeDestruct)
    void UnbindComponentDelegates();

    // Individual binding functions
    void BindHealthComponentDelegates();
    void BindStaminaComponentDelegates();
    void BindSpellCollectionDelegates();
    void BindBroomComponentDelegates();

    // ========================================================================
    // HEALTH COMPONENT HANDLERS
    // ========================================================================

    UFUNCTION()
    void HandleHealthChanged(AActor* Owner, float NewHealth, float Delta);

    // ========================================================================
    // STAMINA COMPONENT HANDLERS
    // ========================================================================

    UFUNCTION()
    void HandleStaminaChanged(AActor* Owner, float NewStamina, float Delta);

    // ========================================================================
    // SPELL COLLECTION HANDLERS
    // ========================================================================

    UFUNCTION()
    void HandleChannelAdded(FName ChannelName);

    // ========================================================================
    // BROOM COMPONENT HANDLERS
    // ========================================================================

    // Called when flight state changes (show/hide broom icon)
    UFUNCTION()
    void HandleFlightStateChanged(bool bIsFlying);

    // Called when stamina bar color should change (cyan/orange/red)
    UFUNCTION()
    void HandleStaminaColorChange(FLinearColor NewColor);

    // Called when forced to dismount due to stamina depletion
    UFUNCTION()
    void HandleForcedDismount();

    // Called when boost state changes (show/hide boost indicator)
    UFUNCTION()
    void HandleBoostChange(bool bIsBoosting);

private:
    // ========================================================================
    // QUIDDITCH SCORING HANDLERS
    // ========================================================================

    // Called when player scores
    UFUNCTION()
    void OnPlayerScored(int32 NewScore, int32 PointsAdded);

    // Called when AI scores
    UFUNCTION()
    void OnAIScored(int32 NewScore, int32 PointsAdded);

    // Update timer display
    void UpdateTimerDisplay(float TimeRemaining);
};