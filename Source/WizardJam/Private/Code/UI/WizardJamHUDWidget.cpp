// ============================================================================
// WizardJamHUDWidget.cpp
// Developer: Marcus Daley
// Date: January 5, 2026
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of modular HUD widget with designer-configurable spell slots.
//
// Key Implementation Details:
// - InitializeSpellSlotSystem() builds TMap lookups from designer's config array
// - FindSpellSlotWidget() searches SpellSlotContainer for "SpellSlot_X" by name
// - UpdateSpellSlotVisual() swaps textures using FSpellSlotConfig helper functions
// - Quidditch widget created dynamically if QuidditchWidgetClass is set
//
// Modular Design Benefits:
// - Designer adds new spells by editing SpellSlotConfigs array (no C++ changes)
// - Slot count is determined by array size and widget names in Blueprint
// - Spell type names (FName) used everywhere - no hardcoded enums
// ============================================================================

#include "Code/UI/WizardJamHUDWidget.h"
#include "Code/UI/WizardJamQuidditchWidget.h"
#include "Code/GameMode/WizardJamGameMode.h"
#include "Code/Utility/AC_HealthComponent.h"
#include "Code/Utility/AC_StaminaComponent.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Code/Utility/AC_BroomComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
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

    // Initialize modular spell slot system
    InitializeSpellSlotSystem();

    // Initialize Quidditch widget if configured
    InitializeQuidditchWidget();

    // Find and cache all components
    CacheComponents();

    // Bind to component delegates
    BindComponentDelegates();

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Widget initialized successfully"));
}

void UWizardJamHUDWidget::NativeDestruct()
{
    UnbindComponentDelegates();

    // Clear runtime maps
    SpellSlotWidgets.Empty();
    SpellConfigLookup.Empty();

    Super::NativeDestruct();
}

// ============================================================================
// SPELL SLOT SYSTEM INITIALIZATION
// ============================================================================

void UWizardJamHUDWidget::InitializeSpellSlotSystem()
{
    // Clear previous state
    SpellSlotWidgets.Empty();
    SpellConfigLookup.Empty();

    // Validate config array
    if (SpellSlotConfigs.Num() == 0)
    {
        UE_LOG(LogWizardJamHUD, Warning,
            TEXT("[WizardJamHUD] SpellSlotConfigs array is empty! Designer must add spell definitions."));
        return;
    }

    // Build lookup map and find widgets for each configured spell
    for (const FSpellSlotConfig& Config : SpellSlotConfigs)
    {
        // Skip invalid entries
        if (!Config.IsValid())
        {
            UE_LOG(LogWizardJamHUD, Warning,
                TEXT("[WizardJamHUD] Skipping invalid spell config (SpellTypeName is None)"));
            continue;
        }

        // Add to lookup map for fast access during gameplay
        SpellConfigLookup.Add(Config.SpellTypeName, Config);

        // Find the widget for this slot index
        UImage* SlotWidget = FindSpellSlotWidget(Config.SlotIndex);
        if (SlotWidget)
        {
            // Map spell name to widget
            SpellSlotWidgets.Add(Config.SpellTypeName, SlotWidget);

            // Initialize to locked state
            UpdateSpellSlotVisual(Config.SpellTypeName, false);

            UE_LOG(LogWizardJamHUD, Display,
                TEXT("[WizardJamHUD] Spell slot configured: '%s' -> SpellSlot_%d"),
                *Config.SpellTypeName.ToString(), Config.SlotIndex);
        }
        else
        {
            UE_LOG(LogWizardJamHUD, Warning,
                TEXT("[WizardJamHUD] Widget 'SpellSlot_%d' not found for spell '%s'"),
                Config.SlotIndex, *Config.SpellTypeName.ToString());
        }
    }

    UE_LOG(LogWizardJamHUD, Display,
        TEXT("[WizardJamHUD] Spell slot system initialized | Configs: %d | Widgets found: %d"),
        SpellSlotConfigs.Num(), SpellSlotWidgets.Num());
}

UImage* UWizardJamHUDWidget::FindSpellSlotWidget(int32 SlotIndex)
{
    // Build expected widget name: "SpellSlot_X"
    FString WidgetName = FString::Printf(TEXT("SpellSlot_%d"), SlotIndex);

    // First try: Search in SpellSlotContainer if it exists
    if (SpellSlotContainer)
    {
        // Get all children of the container
        TArray<UWidget*> Children = SpellSlotContainer->GetAllChildren();

        for (UWidget* Child : Children)
        {
            if (Child && Child->GetName() == WidgetName)
            {
                UImage* ImageWidget = Cast<UImage>(Child);
                if (ImageWidget)
                {
                    return ImageWidget;
                }
            }
        }
    }

    // Second try: Search entire widget tree
    if (WidgetTree)
    {
        UWidget* FoundWidget = WidgetTree->FindWidget(FName(*WidgetName));
        if (FoundWidget)
        {
            return Cast<UImage>(FoundWidget);
        }
    }

    return nullptr;
}

void UWizardJamHUDWidget::UpdateSpellSlotVisual(FName SpellTypeName, bool bIsUnlocked)
{
    // Find the widget for this spell
    UImage** FoundWidget = SpellSlotWidgets.Find(SpellTypeName);
    if (!FoundWidget || !(*FoundWidget))
    {
        UE_LOG(LogWizardJamHUD, Warning,
            TEXT("[WizardJamHUD] No widget mapped for spell '%s'"), *SpellTypeName.ToString());
        return;
    }

    // Find the config for this spell
    FSpellSlotConfig* Config = SpellConfigLookup.Find(SpellTypeName);
    if (!Config)
    {
        UE_LOG(LogWizardJamHUD, Warning,
            TEXT("[WizardJamHUD] No config found for spell '%s'"), *SpellTypeName.ToString());
        return;
    }

    UImage* SlotWidget = *FoundWidget;

    // Get appropriate texture using config helper
    UTexture2D* TargetTexture = Config->GetIcon(bIsUnlocked);
    if (TargetTexture)
    {
        SlotWidget->SetBrushFromTexture(TargetTexture);
    }

    // Apply color tint using config helper
    FLinearColor TintColor = Config->GetColor(bIsUnlocked);
    SlotWidget->SetColorAndOpacity(TintColor);

    UE_LOG(LogWizardJamHUD, Display,
        TEXT("[WizardJamHUD] Spell slot updated: '%s' -> %s"),
        *SpellTypeName.ToString(), bIsUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"));
}

void UWizardJamHUDWidget::UpdateSpellCountText()
{
    if (!SpellCountText)
    {
        return;
    }

    // Get collected count from component
    int32 CollectedCount = 0;
    if (SpellCollectionComp)
    {
        CollectedCount = SpellCollectionComp->GetAllChannels().Num();
    }

    // Total possible = number of configured spell slots
    int32 TotalCount = SpellSlotConfigs.Num();

    // Format as "X / Y" (e.g., "2 / 4")
    FString CountString = FString::Printf(TEXT("%d / %d"), CollectedCount, TotalCount);
    SpellCountText->SetText(FText::FromString(CountString));

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Spell count updated: %s"), *CountString);
}

void UWizardJamHUDWidget::RefreshAllSpellSlots()
{
    if (!SpellCollectionComp)
    {
        UE_LOG(LogWizardJamHUD, Warning,
            TEXT("[WizardJamHUD] Cannot refresh slots - SpellCollectionComp is null"));
        return;
    }

    // Get currently collected channels from component
    TArray<FName> CollectedSpells = SpellCollectionComp->GetAllChannels();

    // Update each configured slot based on collection state
    for (const auto& ConfigPair : SpellConfigLookup)
    {
        FName SpellName = ConfigPair.Key;
        bool bIsCollected = CollectedSpells.Contains(SpellName);
        UpdateSpellSlotVisual(SpellName, bIsCollected);
    }
    // Update spell count text display
    UpdateSpellCountText();

    UE_LOG(LogWizardJamHUD, Display,
        TEXT("[WizardJamHUD] Refreshed all spell slots | Collected: %d"), CollectedSpells.Num());
}

// ============================================================================
// QUIDDITCH WIDGET INITIALIZATION
// ============================================================================

void UWizardJamHUDWidget::InitializeQuidditchWidget()
{
    // Only create if class is configured
    if (!QuidditchWidgetClass)
    {
        UE_LOG(LogWizardJamHUD, Log,
            TEXT("[WizardJamHUD] No QuidditchWidgetClass set - Quidditch UI disabled"));
        return;
    }

    // Create the widget
    QuidditchWidget = CreateWidget<UWizardJamQuidditchWidget>(GetOwningPlayer(), QuidditchWidgetClass);
    if (!QuidditchWidget)
    {
        UE_LOG(LogWizardJamHUD, Error,
            TEXT("[WizardJamHUD] Failed to create Quidditch widget!"));
        return;
    }

    // Add to viewport
    QuidditchWidget->AddToViewport(1);

    // Set initial visibility based on designer setting
    if (bShowQuidditchOnStart)
    {
        QuidditchWidget->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Quidditch UI created and VISIBLE"));
    }
    else
    {
        QuidditchWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Quidditch UI created but HIDDEN"));
    }
}

void UWizardJamHUDWidget::ShowQuidditchUI()
{
    if (QuidditchWidget)
    {
        QuidditchWidget->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Quidditch UI shown"));
    }
}

void UWizardJamHUDWidget::HideQuidditchUI()
{
    if (QuidditchWidget)
    {
        QuidditchWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Quidditch UI hidden"));
    }
}

bool UWizardJamHUDWidget::IsQuidditchUIVisible() const
{
    if (QuidditchWidget)
    {
        return QuidditchWidget->GetVisibility() == ESlateVisibility::Visible;
    }
    return false;
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

    HealthComp = OwnerActor->FindComponentByClass<UAC_HealthComponent>();
    if (HealthComp)
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_HealthComponent"));
    }

    StaminaComp = OwnerActor->FindComponentByClass<UAC_StaminaComponent>();
    if (StaminaComp)
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_StaminaComponent"));
    }

    SpellCollectionComp = OwnerActor->FindComponentByClass<UAC_SpellCollectionComponent>();
    if (SpellCollectionComp)
    {
        UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Cached AC_SpellCollectionComponent"));
    }

    BroomComp = OwnerActor->FindComponentByClass<UAC_BroomComponent>();
    if (BroomComp)
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
    if (HealthComp)
    {
        HealthComp->OnHealthChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleHealthChanged);
    }

    if (StaminaComp)
    {
        StaminaComp->OnStaminaChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleStaminaChanged);
    }

    if (SpellCollectionComp)
    {
        SpellCollectionComp->OnChannelAdded.RemoveDynamic(this, &UWizardJamHUDWidget::HandleChannelAdded);
    }

    if (BroomComp)
    {
        BroomComp->OnFlightStateChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleFlightStateChanged);
        BroomComp->OnStaminaVisualUpdate.RemoveDynamic(this, &UWizardJamHUDWidget::HandleStaminaColorChange);
        BroomComp->OnForcedDismount.RemoveDynamic(this, &UWizardJamHUDWidget::HandleForcedDismount);
        BroomComp->OnBoostStateChanged.RemoveDynamic(this, &UWizardJamHUDWidget::HandleBoostChange);
    }

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] All delegates unbound"));
}

void UWizardJamHUDWidget::BindHealthComponentDelegates()
{
    if (!HealthComp) return;

    HealthComp->OnHealthChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleHealthChanged);

    float CurrentHealth = HealthComp->GetCurrentHealth();
    float MaxHealth = HealthComp->GetMaxHealth();
    HandleHealthChanged(OwnerActor, CurrentHealth, 0.0f);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Health delegates bound | HP: %.0f/%.0f"),
        CurrentHealth, MaxHealth);
}

void UWizardJamHUDWidget::BindStaminaComponentDelegates()
{
    if (!StaminaComp) return;

    StaminaComp->OnStaminaChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleStaminaChanged);

    float CurrentStamina = StaminaComp->GetCurrentStamina();
    float MaxStamina = StaminaComp->GetMaxStamina();
    HandleStaminaChanged(OwnerActor, CurrentStamina, 0.0f);

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Stamina delegates bound | Stamina: %.0f/%.0f"),
        CurrentStamina, MaxStamina);
}

void UWizardJamHUDWidget::BindSpellCollectionDelegates()
{
    if (!SpellCollectionComp) return;

    SpellCollectionComp->OnChannelAdded.AddDynamic(this, &UWizardJamHUDWidget::HandleChannelAdded);

    // Refresh slots to show any already-collected spells
    RefreshAllSpellSlots();

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Spell collection delegates bound"));
}

void UWizardJamHUDWidget::BindBroomComponentDelegates()
{
    if (!BroomComp) return;

    BroomComp->OnFlightStateChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleFlightStateChanged);
    BroomComp->OnStaminaVisualUpdate.AddDynamic(this, &UWizardJamHUDWidget::HandleStaminaColorChange);
    BroomComp->OnForcedDismount.AddDynamic(this, &UWizardJamHUDWidget::HandleForcedDismount);
    BroomComp->OnBoostStateChanged.AddDynamic(this, &UWizardJamHUDWidget::HandleBoostChange);

    HandleFlightStateChanged(BroomComp->IsFlying());

    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Broom delegates bound | Flying: %s"),
        BroomComp->IsFlying() ? TEXT("YES") : TEXT("NO"));
}

// ============================================================================
// HEALTH HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleHealthChanged(AActor* Owner, float NewHealth, float Delta)
{
    if (!HealthComp) return;

    float MaxHealth = HealthComp->GetMaxHealth();
    if (MaxHealth <= 0.0f) return;

    float HealthPercent = NewHealth / MaxHealth;

    if (HealthProgressBar)
    {
        HealthProgressBar->SetPercent(HealthPercent);

        // Color based on health level
        FLinearColor HealthColor = FLinearColor::Green;
        if (HealthPercent <= 0.3f)
        {
            HealthColor = FLinearColor::Red;
        }
        else if (HealthPercent <= 0.6f)
        {
            HealthColor = FLinearColor::Yellow;
        }
        HealthProgressBar->SetFillColorAndOpacity(HealthColor);
    }

    if (HealthText)
    {
        HealthText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f / %.0f"), NewHealth, MaxHealth)));
    }
}

// ============================================================================
// STAMINA HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleStaminaChanged(AActor* Owner, float NewStamina, float Delta)
{
    if (!StaminaComp) return;

    float MaxStamina = StaminaComp->GetMaxStamina();
    if (MaxStamina <= 0.0f) return;

    float StaminaPercent = NewStamina / MaxStamina;

    if (StaminaProgressBar)
    {
        StaminaProgressBar->SetPercent(StaminaPercent);
    }

    if (StaminaText)
    {
        StaminaText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f / %.0f"), NewStamina, MaxStamina)));
    }
}

// ============================================================================
// SPELL COLLECTION HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleChannelAdded(FName ChannelName)
{
    UE_LOG(LogWizardJamHUD, Display, TEXT("[WizardJamHUD] Channel added: %s"), *ChannelName.ToString());

    // Update the spell slot visual to unlocked state
    UpdateSpellSlotVisual(ChannelName, true);
}

// ============================================================================
// BROOM FLIGHT HANDLERS
// ============================================================================

void UWizardJamHUDWidget::HandleFlightStateChanged(bool bIsFlying)
{
    if (BroomIcon)
    {
        BroomIcon->SetVisibility(bIsFlying ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Flight state: %s"),
        bIsFlying ? TEXT("FLYING") : TEXT("GROUNDED"));
}

void UWizardJamHUDWidget::HandleStaminaColorChange(FLinearColor NewColor)
{
    if (StaminaProgressBar)
    {
        StaminaProgressBar->SetFillColorAndOpacity(NewColor);
    }
}

void UWizardJamHUDWidget::HandleForcedDismount()
{
    UE_LOG(LogWizardJamHUD, Warning, TEXT("[WizardJamHUD] FORCED DISMOUNT - Out of stamina!"));

    if (OutOfStaminaWarningText)
    {
        OutOfStaminaWarningText->SetVisibility(ESlateVisibility::Visible);

        // Hide after 2 seconds
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
    }
}

void UWizardJamHUDWidget::HandleBoostChange(bool bIsBoosting)
{
    if (BoostIndicatorImage)
    {
        BoostIndicatorImage->SetVisibility(bIsBoosting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    UE_LOG(LogWizardJamHUD, Log, TEXT("[WizardJamHUD] Boost: %s"),
        bIsBoosting ? TEXT("ON") : TEXT("OFF"));
}