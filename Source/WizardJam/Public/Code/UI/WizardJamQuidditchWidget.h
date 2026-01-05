// ============================================================================
// WizardJamQuidditchWidget.h
// Developer: Marcus Daley
// Date: January 5, 2026
// Project: WizardJam
// ============================================================================
// Purpose:
// Separate widget for Quidditch-specific UI elements (scoreboard, timer, etc.)
// This widget is created and managed by WizardJamHUDWidget, allowing the main
// HUD to show/hide Quidditch features based on game mode.
//
// Designer Workflow:
// 1. Create WBP_QuidditchHUD Blueprint (parent: WizardJamQuidditchWidget)
// 2. Add TextBlocks named: PlayerScoreText, AIScoreText, MatchTimerText
// 3. Optionally add PlayerScoreLabel, AIScoreLabel for team names
// 4. In WBP_PlayerHUD, set QuidditchWidgetClass to WBP_QuidditchHUD
// 5. Toggle bShowQuidditchOnStart based on game mode
//
// Runtime Control:
// - WizardJamHUDWidget::ShowQuidditchUI() / HideQuidditchUI()
// - Or call SetVisibility() directly on this widget
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WizardJamQuidditchWidget.generated.h"

// Forward declarations
class UTextBlock;
class AWizardJamGameMode;

UCLASS()
class WIZARDJAM_API UWizardJamQuidditchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ========================================================================
    // PUBLIC API - Called by WizardJamHUDWidget or GameMode
    // ========================================================================

    // Update player score display
    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void UpdatePlayerScore(int32 NewScore, int32 PointsAdded);

    // Update AI score display
    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void UpdateAIScore(int32 NewScore, int32 PointsAdded);

    // Update match timer display (formats as MM:SS)
    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void UpdateTimer(float TimeRemaining);

    // Set team labels (e.g., "Gryffindor" vs "Slytherin")
    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void SetTeamLabels(const FText& PlayerTeamName, const FText& AITeamName);

protected:
    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ========================================================================
    // WIDGET REFERENCES (Bound in Blueprint via BindWidgetOptional)
    // ========================================================================

    // Score displays
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* PlayerScoreText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* AIScoreText;

    // Timer display
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* MatchTimerText;

    // Team labels (optional - for showing team names)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* PlayerScoreLabel;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* AIScoreLabel;

    // ========================================================================
    // DESIGNER CONFIGURATION
    // ========================================================================

    // Default team names (can be overridden at runtime)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch|Labels")
    FText DefaultPlayerTeamName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch|Labels")
    FText DefaultAITeamName;

    // ========================================================================
    // GAMEMODE BINDING
    // ========================================================================

    // Cached reference to game mode for delegate binding
    UPROPERTY()
    AWizardJamGameMode* CachedGameMode;

    // Bind to GameMode scoring delegates
    void BindToGameMode();

    // Unbind from GameMode delegates
    void UnbindFromGameMode();

private:
    // ========================================================================
    // DELEGATE HANDLERS
    // ========================================================================

    UFUNCTION()
    void HandlePlayerScored(int32 NewScore, int32 PointsAdded);

    UFUNCTION()
    void HandleAIScored(int32 NewScore, int32 PointsAdded);
};