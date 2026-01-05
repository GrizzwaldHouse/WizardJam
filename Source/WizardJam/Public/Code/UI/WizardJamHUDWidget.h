// ============================================================================
// WizardJamHUDWidget.h
// Developer: Marcus Daley
// Date: January 5, 2026
// Project: WizardJam
// ============================================================================
// Purpose:
// Main HUD widget with modular spell slot system and broom flight integration.
// Designer configures spell slots via SpellSlotConfigs array - no C++ changes
// needed to add new spell types.
//
// Modular Architecture:
// - SpellSlotConfigs: Designer-editable array of FSpellSlotConfig
// - SpellSlotWidgets: TMap linking FName spell type to UImage widget
// - QuidditchWidget: Optional child widget for Quidditch-specific UI
//
// Designer Workflow:
// 1. Open WBP_PlayerHUD Blueprint
// 2. Add Image widgets named "SpellSlot_0", "SpellSlot_1", etc. in SpellSlotContainer
// 3. Fill SpellSlotConfigs array with spell definitions (Flame, Ice, Lightning, Arcane)
// 4. Assign locked/unlocked textures per spell
// 5. System auto-matches spell collection events to slot configs
//
// Widget Naming Convention:
// - SpellSlot_X where X = SlotIndex from FSpellSlotConfig
// - Example: SlotIndex=2 -> finds widget named "SpellSlot_2"
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Code/UI/SpellSlotConfig.h"
#include "Code/UI/WizardJamQuidditchWidget.h"
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

public:
    // ========================================================================
    // DESIGNER CONFIGURATION - SPELL SLOTS
    // Fill this array in Blueprint to define available spell types
    // ========================================================================

    // Array of spell slot configurations
    // Designer adds entries here - each entry defines one spell's textures and slot position
    // Example: Add "Flame" with SlotIndex=0, "Ice" with SlotIndex=1, etc.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spell Slots")
    TArray<FSpellSlotConfig> SpellSlotConfigs;

    // ========================================================================
    // DESIGNER CONFIGURATION - QUIDDITCH UI
    // ========================================================================

    // Widget class for Quidditch-specific UI (scoreboard, timer)
    // Set this to show Quidditch UI only during Quidditch game modes
    // Leave None to disable Quidditch UI entirely
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch")
    TSubclassOf<UUserWidget> QuidditchWidgetClass;

    // Show Quidditch UI on startup? (Designer toggles in Blueprint)
    // Can also be controlled at runtime via ShowQuidditchUI() / HideQuidditchUI()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quidditch")
    bool bShowQuidditchOnStart;

    // ========================================================================
    // PUBLIC FUNCTIONS - RUNTIME CONTROL
    // ========================================================================

    // Show or hide Quidditch scoreboard at runtime
    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void ShowQuidditchUI();

    UFUNCTION(BlueprintCallable, Category = "Quidditch")
    void HideQuidditchUI();

    UFUNCTION(BlueprintPure, Category = "Quidditch")
    bool IsQuidditchUIVisible() const;

    // Manually refresh all spell slots (useful after loading save data)
    UFUNCTION(BlueprintCallable, Category = "Spell Slots")
    void RefreshAllSpellSlots();

protected:
    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ========================================================================
    // COMPONENT REFERENCES (Cached at construction)
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
    // WIDGET REFERENCES - CORE HUD (Bound in Blueprint via BindWidgetOptional)
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

    // Spell Slot Container - holds dynamically found SpellSlot_X widgets
    UPROPERTY(meta = (BindWidgetOptional))
    UPanelWidget* SpellSlotContainer;
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* SpellCountText;

    // Broom Flight UI
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* BroomIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* BoostIndicatorImage;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* OutOfStaminaWarningText;

    // ========================================================================
    // RUNTIME STATE - SPELL SLOTS
    // ========================================================================

    // Maps spell type name to the Image widget displaying it
    // Populated at startup by scanning SpellSlotContainer for SpellSlot_X widgets
    UPROPERTY()
    TMap<FName, UImage*> SpellSlotWidgets;

    // Maps spell type name to its config for fast lookup
    // Populated from SpellSlotConfigs array at startup
    TMap<FName, FSpellSlotConfig> SpellConfigLookup;

    // ========================================================================
    // RUNTIME STATE - QUIDDITCH
    // ========================================================================

    // Runtime instance of Quidditch widget (created if QuidditchWidgetClass is set)
    UPROPERTY()
    UWizardJamQuidditchWidget* QuidditchWidget;

    // ========================================================================
    // COMPONENT INITIALIZATION
    // ========================================================================

    void CacheComponents();
    void BindComponentDelegates();
    void UnbindComponentDelegates();

    void BindHealthComponentDelegates();
    void BindStaminaComponentDelegates();
    void BindSpellCollectionDelegates();
    void BindBroomComponentDelegates();

    // ========================================================================
    // SPELL SLOT INITIALIZATION
    // ========================================================================

    // Build lookup maps and find slot widgets
    void InitializeSpellSlotSystem();

    // Find Image widget by slot index (searches for "SpellSlot_X" in container)
    UImage* FindSpellSlotWidget(int32 SlotIndex);

    // Update a single spell slot's texture based on unlock state
    void UpdateSpellSlotVisual(FName SpellTypeName, bool bIsUnlocked);
    void UpdateSpellCountText();
    // ========================================================================
    // QUIDDITCH INITIALIZATION
    // ========================================================================

    void InitializeQuidditchWidget();

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

    UFUNCTION()
    void HandleFlightStateChanged(bool bIsFlying);

    UFUNCTION()
    void HandleStaminaColorChange(FLinearColor NewColor);

    UFUNCTION()
    void HandleForcedDismount();

    UFUNCTION()
    void HandleBoostChange(bool bIsBoosting);
};