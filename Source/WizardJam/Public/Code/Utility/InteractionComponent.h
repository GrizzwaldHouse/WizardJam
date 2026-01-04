// InteractionComponent.h
// Component for player that detects and displays tooltips for interactable objects
// Developer: Marcus Daley
// Date: December 31, 2025

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class IInteractable;
class UTooltipWidget;
// Broadcast when player looks at/away from interactable object
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableTargeted, bool, bIsTargeting);

// Player component that handles interaction detection and tooltip display
// Designer configures: Interaction range, tooltip widget class, trace frequency
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WIZARDJAM_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // Attempt to interact with currently focused actor (called when player presses E)
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool AttemptInteraction();

protected:
    // Designer-configurable properties

    // Maximum distance for interaction traces
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Settings")
    float InteractionTraceRange;

    // Widget class to spawn for tooltip display
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Settings")
    TSubclassOf<UUserWidget> TooltipWidgetClass;

    // Show debug lines for interaction traces (development only)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
    bool bShowDebugTrace;
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnInteractableTargeted OnInteractableTargeted;
private:
    // Runtime state

    UPROPERTY()
    AActor* CurrentFocusedActor;

    UPROPERTY()
    AActor* PreviousFocusedActor;

    UPROPERTY()
    UUserWidget* TooltipWidgetInstance;

    // Helper functions

    bool PerformInteractionTrace(FHitResult& OutHitResult);
    void UpdateFocusedActor(AActor* NewFocusedActor);
    void ShowTooltip(const FText& TooltipText, const FText& InteractionPrompt);
    void HideTooltip();
    bool CreateTooltipWidget();
    bool IsValidInteractableActor(AActor* Actor) const;
};