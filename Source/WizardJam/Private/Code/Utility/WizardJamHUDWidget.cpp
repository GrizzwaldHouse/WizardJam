// WizardJamHUDWidget.cpp
// Main HUD widget with broom flight integration
//
// Developer: Marcus Daley
// Date: January 1, 2026
// Project: WizardJam
//
// CRITICAL SAFETY RULES:
// - ALWAYS check for nullptr before accessing any object
// - NEVER assume components exist
// - Log warnings when nullptr found
// - Gracefully handle missing references

#include "Code/Utility/WizardJamHUDWidget.h"
#include "Code/GameMode/WizardJamGameMode.h"
#include "Code/Utility/AC_HealthComponent.h"
#include "Code/Utility/AC_StaminaComponent.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Code/Utility/AC_BroomComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWizardJamHUD, Log, All);

// ============================================================================
// LIFECYCLE
// ============================================================================

void UWizardJamHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] NativeConstruct called"));

    // Cache owner actor
    OwnerActor = GetOwningPlayerPawn();
    if (!OwnerActor)
    {
        UE_LOG(LogWizardJamHUD, Error, TEXT("[WizardJamHUD] No owning player pawn!"));
        return;
    }

    // Find and cache all components
    CacheComponents();

    // Bind to component delegates
    BindComponentDelegates();

    // Bind to GameMode scoring events
    AWizardJamGameMode* GameMode = Cast<AWizardJamGameMode>(GetWorld()->GetAuthGameMode());
    if (GameMode)
    {
        GameMode->OnPlayerScored.AddDynamic(this, &UWizardJamHUDWidget::OnPlayerScored);
        GameMode->OnAIScored.AddDynamic(this, &UWizardJamHUDWidget::OnAIScored);

        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Bound to GameMode scoring events"));

        // Initialize score displays
        if (PlayerScoreText)
        {
            PlayerScoreText->SetText(FText::FromString(TEXT("0")));
        }

        if (AIScoreText)
        {
            AIScoreText->SetText(FText::FromString(TEXT("0")));
        }

        if (MatchTimerText)
        {
            MatchTimerText->SetText(FText::FromString(TEXT("")));
        }
    }
    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Widget initialized successfully"));
}

void UWizardJamHUDWidget::NativeDestruct()
{
    // Unbind all delegates to prevent crashes
    UnbindComponentDelegates();

    Super::NativeDestruct();
}

// ============================================================================
// COMPONENT CACHING
// ============================================================================

void UWizardJamHUDWidget::CacheComponents()
{
    if (!OwnerActor)
    {
        UE_LOG(LogWizardJamHUD, Error, TEXT("[WizardJamHUD] Cannot cache components - OwnerActor is null"));
        return;
    }

    // Cache health component
    HealthComp = OwnerActor->FindComponentByClass<UAC_HealthComponent>();
    if (!HealthComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] No AC_HealthComponent found on owner"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_HealthComponent"));
    }

    // Cache stamina component
    StaminaComp = OwnerActor->FindComponentByClass<UAC_StaminaComponent>();
    if (!StaminaComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] No AC_StaminaComponent found on owner"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_StaminaComponent"));
    }

    // Cache spell collection component
    SpellCollectionComp = OwnerActor->FindComponentByClass<UAC_SpellCollectionComponent>();
    if (!SpellCollectionComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] No AC_SpellCollectionComponent found on owner"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_SpellCollectionComponent"));
    }

    // Cache broom component
    BroomComp = OwnerActor->FindComponentByClass<UAC_BroomComponent>();
    if (!BroomComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] No AC_BroomComponent found on owner"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_BroomComponent"));
    }
}

// ============================================================================
// DELEGATE BINDING
// ============================================================================

void UWizardJamHUDWidget::BindComponentDelegates()
{
    BindHealthComponentDelegates();
    BindStaminaComponentDelegates();
    BindSpellCollectionDelegates();
    BindBroomComponentDelegates();
}

void UWizardJamHUDWidget::UnbindComponentDelegates()
{
    // Health component
    if (HealthComp && HealthComp->OnHealthChanged.IsBound())
    {
        HealthComp->OnHealthChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleHealthChanged);
    }

    // Stamina component
    if (StaminaComp && StaminaComp->OnStaminaChanged.IsBound())
    {
        StaminaComp->OnStaminaChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleStaminaChanged);
    }

    // Spell collection component
    if (SpellCollectionComp && SpellCollectionComp->OnChannelAdded.IsBound())
    {
        SpellCollectionComp->OnChannelAdded.RemoveDynamic(this, &UWizardJamHUDWidget::HandleChannelAdded);
    }

    // Broom component
    if (BroomComp)
    {
        if (BroomComp->OnFlightStateChanged.IsBound())
        {
            BroomComp->OnFlightStateChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleFlightStateChanged);
        }
        if (BroomComp->OnStaminaVisualUpdate.IsBound())
        {
            BroomComp->OnStaminaVisualUpdate.RemoveDynamic(this, &UWizardJamHUDWidget::HandleStaminaColorChange);
        }
        if (BroomComp->OnForcedDismount.IsBound())
        {
            BroomComp->OnForcedDismount.RemoveDynamic(this, &UWizardJamHUDWidget::HandleForcedDismount);
        }
        if (BroomComp->OnBoostStateChanged.IsBound())
        {
            BroomComp->OnBoostStateChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleBoostChange);
        }
    }

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] All delegates unbound"));
}

void UWizardJamHUDWidget::BindHealthComponentDelegates()
{
    if (!HealthComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] Cannot bind health delegates - HealthComp is null"));
        return;
    }

    // Bind to health changed event
    HealthComp->OnHealthChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleHealthChanged);

    // Get initial health values
    float CurrentHealth = HealthComp->GetCurrentHealth();
    float MaxHealth = HealthComp->GetMaxHealth();

    // Update UI immediately
    HandleHealthChanged(OwnerActor, CurrentHealth, MaxHealth - CurrentHealth);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Health component delegates bound | HP: %.0f/%.0f"),
        CurrentHealth, MaxHealth);
}

void UWizardJamHUDWidget::BindStaminaComponentDelegates()
{
    if (!StaminaComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] Cannot bind stamina delegates - StaminaComp is null"));
        return;
    }

    // Bind to stamina changed event
    StaminaComp->OnStaminaChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleStaminaChanged);

    // Get initial stamina values
    float CurrentStamina = StaminaComp->GetCurrentStamina();
    float MaxStamina = StaminaComp->GetMaxStamina();

    // Update UI immediately
    HandleStaminaChanged(OwnerActor, CurrentStamina, MaxStamina - CurrentStamina);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Stamina component delegates bound | Stamina: %.0f/%.0f"),
        CurrentStamina, MaxStamina);
}

void UWizardJamHUDWidget::BindSpellCollectionDelegates()
{
    if (!SpellCollectionComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] Cannot bind spell delegates - SpellCollectionComp is null"));
        return;
    }

    // Bind to channel added event (was OnChannelUnlocked - FIXED)
    SpellCollectionComp->OnChannelAdded.AddDynamic(this, &UWizardJamHUDWidget::HandleChannelAdded);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Spell collection delegates bound"));
}

void UWizardJamHUDWidget::BindBroomComponentDelegates()
{
    if (!BroomComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] Cannot bind broom delegates - BroomComp is null"));
        return;
    }

    // Bind flight state changed (show/hide broom icon)
    BroomComp->OnFlightStateChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleFlightStateChanged);

    // Bind stamina color changes (cyan/orange/red)
    BroomComp->OnStaminaVisualUpdate.AddDynamic(this, &UWizardJamHUDWidget::HandleStaminaColorChange);

    // Bind forced dismount (stamina depletion warning)
    BroomComp->OnForcedDismount.AddDynamic(this, &UWizardJamHUDWidget::HandleForcedDismount);

    // Bind boost state change (boost indicator)
    BroomComp->OnBoostStateChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleBoostChange);

    // Get initial flight state
    bool bIsFlying = BroomComp->IsFlying();
    float StaminaPercent = BroomComp->GetFlightStaminaPercent();

    // Update UI immediately
    HandleFlightStateChanged(bIsFlying);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Broom component delegates bound | Flying: %s | Stamina: %.0f%%"),
        bIsFlying ? TEXT("YES") : TEXT("NO"), StaminaPercent * 100.0f);
}

// ============================================================================
// HEALTH HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleHealthChanged(AActor* Owner, float NewHealth, float Delta)
{
    if (!HealthComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] HandleHealthChanged called but HealthComp is null"));
        return;
    }

    float MaxHealth = HealthComp->GetMaxHealth();
    if (MaxHealth <= 0.0f)
    {
        UE_LOG(LogWizardJamHUD, Error, TEXT("[WizardJamHUD] MaxHealth is zero or negative!"));
        return;
    }

    float HealthPercent = NewHealth / MaxHealth;

    // Update health bar
    if (HealthProgressBar)
    {
        HealthProgressBar->SetPercent(HealthPercent);
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] HealthProgressBar widget is null - cannot update"));
    }

    // Update health text
    if (HealthText)
    {
        FText HealthDisplayText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), NewHealth, MaxHealth));
        HealthText->SetText(HealthDisplayText);
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] HealthText widget is null - cannot update"));
    }

    // Change health bar color based on percentage
    if (HealthProgressBar)
    {
        FLinearColor HealthColor;
        if (HealthPercent > 0.6f)
        {
            HealthColor = FLinearColor::Green;
        }
        else if (HealthPercent > 0.3f)
        {
            HealthColor = FLinearColor::Yellow;
        }
        else
        {
            HealthColor = FLinearColor::Red;
        }
        HealthProgressBar->SetFillColorAndOpacity(HealthColor);
    }

    UE_LOG(LogWizardJamHUD, Verbose, TEXT("[WizardJamHUD] Health updated: %.0f/%.0f (%.0f%%)"),
        NewHealth, MaxHealth, HealthPercent * 100.0f);
}

// ============================================================================
// STAMINA HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleStaminaChanged(AActor* Owner, float NewStamina, float Delta)
{
    if (!StaminaComp)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] HandleStaminaChanged called but StaminaComp is null"));
        return;
    }

    float MaxStamina = StaminaComp->GetMaxStamina();
    if (MaxStamina <= 0.0f)
    {
        UE_LOG(LogWizardJamHUD, Error, TEXT("[WizardJamHUD] MaxStamina is zero or negative!"));
        return;
    }

    float StaminaPercent = NewStamina / MaxStamina;

    // Update stamina bar
    if (StaminaProgressBar)
    {
        StaminaProgressBar->SetPercent(StaminaPercent);
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] StaminaProgressBar widget is null - cannot update"));
    }

    // Update stamina text
    if (StaminaText)
    {
        FText StaminaDisplayText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), NewStamina, MaxStamina));
        StaminaText->SetText(StaminaDisplayText);
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] StaminaText widget is null - cannot update"));
    }

    UE_LOG(LogWizardJamHUD, Verbose, TEXT("[WizardJamHUD] Stamina updated: %.0f/%.0f (%.0f%%)"),
        NewStamina, MaxStamina, StaminaPercent * 100.0f);
}

// ============================================================================
// SPELL COLLECTION HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleChannelAdded(FName ChannelName)
{
    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Channel added: %s"), *ChannelName.ToString());

    // Update spell slot UI (implement based on your spell slot system)
    // Example: Show unlocked spell icon, change locked icon to unlocked

    // This is a placeholder - replace with your actual spell slot update logic
    if (SpellSlotContainer)
    {
        // TODO: Update specific spell slot based on ChannelName
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Updating spell slot for channel: %s"), *ChannelName.ToString());
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] SpellSlotContainer widget is null - cannot update spell slots"));
    }
}

// ============================================================================
// BROOM FLIGHT HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleFlightStateChanged(bool bIsFlying)
{
    // Show/hide broom icon
    if (BroomIcon)
    {
        ESlateVisibility NewVisibility = bIsFlying ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        BroomIcon->SetVisibility(NewVisibility);

        UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Broom icon visibility: %s"),
            bIsFlying ? TEXT("VISIBLE") : TEXT("COLLAPSED"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] BroomIcon widget is null - cannot update visibility"));
    }

    // Update stamina bar if flying
    if (bIsFlying && BroomComp)
    {
        float StaminaPercent = BroomComp->GetFlightStaminaPercent();
        if (StaminaProgressBar)
        {
            StaminaProgressBar->SetPercent(StaminaPercent);
        }
    }

    UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Flight state: %s"),
        bIsFlying ? TEXT("FLYING") : TEXT("GROUNDED"));
}

void UWizardJamHUDWidget::HandleStaminaColorChange(FLinearColor NewColor)
{
    // Change stamina bar color based on flight state
    // Green = grounded, Cyan = flying, Orange = boosting, Red = depleted
    if (StaminaProgressBar)
    {
        StaminaProgressBar->SetFillColorAndOpacity(NewColor);

        UE_LOG(LogWizardJamHUD, Verbose, TEXT("[WizardJamHUD] Stamina color changed: R=%.2f G=%.2f B=%.2f"),
            NewColor.R, NewColor.G, NewColor.B);
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] StaminaProgressBar is null - cannot update color"));
    }
}

void UWizardJamHUDWidget::HandleForcedDismount()
{
    UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] FORCED DISMOUNT - Out of stamina!"));

    // Show warning text if available
    if (OutOfStaminaWarningText)
    {
        OutOfStaminaWarningText->SetVisibility(ESlateVisibility::Visible);

        // Hide warning after 2 seconds
        if (GetWorld())
        {
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(
                TimerHandle,
                [this]()
                {
                    if (OutOfStaminaWarningText)
                    {
                        OutOfStaminaWarningText->SetVisibility(ESlateVisibility::Collapsed);
                    }
                },
                2.0f,
                false
            );
        }
        else
        {
            UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] World is null - cannot set timer for warning text"));
        }
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] OutOfStaminaWarningText widget is null - cannot show warning"));
    }

    // Optional: Play warning sound
    // TODO: Add sound playback here if desired
}

void UWizardJamHUDWidget::HandleBoostChange(bool bIsBoosting)
{
    // Show/hide boost indicator
    if (BoostIndicatorImage)
    {
        ESlateVisibility NewVisibility = bIsBoosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        BoostIndicatorImage->SetVisibility(NewVisibility);

        UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Boost indicator visibility: %s"),
            bIsBoosting ? TEXT("VISIBLE") : TEXT("COLLAPSED"));
    }
    else
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] BoostIndicatorImage widget is null - cannot update visibility"));
    }

    UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Boost state: %s"),
        bIsBoosting ? TEXT("ON") : TEXT("OFF"));
}

void UWizardJamHUDWidget::OnPlayerScored(int32 NewScore, int32 PointsAdded)
{
    if (!PlayerScoreText)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] PlayerScoreText not bound in widget"));
        return;
    }

    // Update score display
    PlayerScoreText->SetText(FText::AsNumber(NewScore));

    // Optional: Flash text or play animation here
    // For jam scope, simple text update is sufficient

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Player score updated: %d (+%d)"),
        NewScore, PointsAdded);
}

void UWizardJamHUDWidget::OnAIScored(int32 NewScore, int32 PointsAdded)
{
    if (!AIScoreText)
    {
        UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] AIScoreText not bound in widget"));
        return;
    }

    // Update score display
    AIScoreText->SetText(FText::AsNumber(NewScore));

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] AI score updated: %d (+%d)"),
        NewScore, PointsAdded);
}

void UWizardJamHUDWidget::UpdateTimerDisplay(float TimeRemaining)
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
