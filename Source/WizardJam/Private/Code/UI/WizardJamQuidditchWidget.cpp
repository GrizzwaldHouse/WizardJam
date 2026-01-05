// ============================================================================
// WizardJamQuidditchWidget.cpp
// Developer: Marcus Daley
// Date: January 5, 2026
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of the separate Quidditch scoreboard widget.
// Binds to GameMode scoring delegates and updates UI accordingly.
//
// Key Implementation Details:
// - Self-contained widget that manages its own GameMode binding
// - Can be created/destroyed independently of main HUD
// - Formats timer as MM:SS for clean display
// - Supports optional team name labels for customization
// ============================================================================

#include "Code/UI/WizardJamQuidditchWidget.h"
#include "Code/GameMode/WizardJamGameMode.h"
#include "Components/TextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuidditchWidget, Log, All);

// ============================================================================
// LIFECYCLE
// ============================================================================

void UWizardJamQuidditchWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] NativeConstruct called"));

    // Initialize score displays to zero
    if (PlayerScoreText)
    {
        PlayerScoreText->SetText(FText::FromString(TEXT("0")));
    }

    if (AIScoreText)
    {
        AIScoreText->SetText(FText::FromString(TEXT("0")));
    }

    // Initialize timer to empty (or could show 00:00)
    if (MatchTimerText)
    {
        MatchTimerText->SetText(FText::GetEmpty());
    }

    // Set default team labels if configured
    if (PlayerScoreLabel && !DefaultPlayerTeamName.IsEmpty())
    {
        PlayerScoreLabel->SetText(DefaultPlayerTeamName);
    }

    if (AIScoreLabel && !DefaultAITeamName.IsEmpty())
    {
        AIScoreLabel->SetText(DefaultAITeamName);
    }

    // Bind to GameMode scoring events
    BindToGameMode();

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] Widget initialized"));
}

void UWizardJamQuidditchWidget::NativeDestruct()
{
    UnbindFromGameMode();

    Super::NativeDestruct();
}

// ============================================================================
// GAMEMODE BINDING
// ============================================================================

void UWizardJamQuidditchWidget::BindToGameMode()
{
    if (!GetWorld())
    {
        UE_LOG(LogQuidditchWidget, Warning, TEXT("[QuidditchWidget] Cannot bind - World is null"));
        return;
    }

    CachedGameMode = Cast<AWizardJamGameMode>(GetWorld()->GetAuthGameMode());
    if (!CachedGameMode)
    {
        UE_LOG(LogQuidditchWidget, Warning, TEXT("[QuidditchWidget] Cannot bind - GameMode is not WizardJamGameMode"));
        return;
    }

    // Bind to scoring delegates
    CachedGameMode->OnPlayerScored.AddDynamic(this, &UWizardJamQuidditchWidget::HandlePlayerScored);
    CachedGameMode->OnAIScored.AddDynamic(this, &UWizardJamQuidditchWidget::HandleAIScored);

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] Bound to GameMode scoring events"));
}

void UWizardJamQuidditchWidget::UnbindFromGameMode()
{
    if (CachedGameMode)
    {
        CachedGameMode->OnPlayerScored.RemoveDynamic(this, &UWizardJamQuidditchWidget::HandlePlayerScored);
        CachedGameMode->OnAIScored.RemoveDynamic(this, &UWizardJamQuidditchWidget::HandleAIScored);

        UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] Unbound from GameMode"));
    }

    CachedGameMode = nullptr;
}

// ============================================================================
// DELEGATE HANDLERS
// ============================================================================

void UWizardJamQuidditchWidget::HandlePlayerScored(int32 NewScore, int32 PointsAdded)
{
    UpdatePlayerScore(NewScore, PointsAdded);
}

void UWizardJamQuidditchWidget::HandleAIScored(int32 NewScore, int32 PointsAdded)
{
    UpdateAIScore(NewScore, PointsAdded);
}

// ============================================================================
// PUBLIC API
// ============================================================================

void UWizardJamQuidditchWidget::UpdatePlayerScore(int32 NewScore, int32 PointsAdded)
{
    if (!PlayerScoreText)
    {
        UE_LOG(LogQuidditchWidget, Warning, TEXT("[QuidditchWidget] PlayerScoreText not bound"));
        return;
    }

    PlayerScoreText->SetText(FText::AsNumber(NewScore));

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] Player score: %d (+%d)"),
        NewScore, PointsAdded);
}

void UWizardJamQuidditchWidget::UpdateAIScore(int32 NewScore, int32 PointsAdded)
{
    if (!AIScoreText)
    {
        UE_LOG(LogQuidditchWidget, Warning, TEXT("[QuidditchWidget] AIScoreText not bound"));
        return;
    }

    AIScoreText->SetText(FText::AsNumber(NewScore));

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] AI score: %d (+%d)"),
        NewScore, PointsAdded);
}

void UWizardJamQuidditchWidget::UpdateTimer(float TimeRemaining)
{
    if (!MatchTimerText)
    {
        return;
    }

    // Format time as MM:SS
    int32 Minutes = FMath::FloorToInt(TimeRemaining / 60.0f);
    int32 Seconds = FMath::FloorToInt(FMath::Fmod(TimeRemaining, 60.0f));

    FString TimeString = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
    MatchTimerText->SetText(FText::FromString(TimeString));
}

void UWizardJamQuidditchWidget::SetTeamLabels(const FText& PlayerTeamName, const FText& AITeamName)
{
    if (PlayerScoreLabel)
    {
        PlayerScoreLabel->SetText(PlayerTeamName);
    }

    if (AIScoreLabel)
    {
        AIScoreLabel->SetText(AITeamName);
    }

    UE_LOG(LogQuidditchWidget, Display, TEXT("[QuidditchWidget] Team labels set: '%s' vs '%s'"),
        *PlayerTeamName.ToString(), *AITeamName.ToString());
}