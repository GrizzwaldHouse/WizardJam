// Interactable.h
// Interface for actors that display tooltips/dialogue on player gaze
// Developer: Marcus Daley
// Date: December 31, 2025

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
    GENERATED_BODY()
};

// Interface for actors that can be interacted with or display information
// Usage: Implement on brooms, NPCs, doors, pickups, or any tooltip-enabled actor
class WIZARDJAM_API IInteractable
{
    GENERATED_BODY()

public:
    // Get the tooltip text to display when player looks at this actor
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    FText GetTooltipText() const;

    // Get the interaction prompt text ("Press E to Mount", etc.)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    FText GetInteractionPrompt() const;

    // Get detailed information (optional - unused for broom)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    FText GetDetailedInfo() const;

    // Check if this actor can be interacted with right now
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    bool CanInteract() const;

    // Called when player presses interaction key while looking at this actor
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    void OnInteract(AActor* Interactor);

    // Get maximum interaction distance (default: 300 units = 3 meters)
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interactable")
    float GetInteractionRange() const;
};