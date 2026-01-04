// BroomActor.h
// World-placeable broom that grants flight when player interacts
//
// Developer: Marcus Daley
// Date: December 31, 2025
// Project: WizardJam
//
// Purpose:
// Implements IInteractable to display tooltips and handle player mounting.
// Checks for "BroomFlight" channel requirement before enabling player's AC_BroomComponent.
// Uses socket-based attachment for future animation support.
//
// Designer Workflow:
// 1. Place BP_Broom_Flight in level
// 2. Configure RequiredChannel (default: "BroomFlight")
// 3. Set MountSocket name on broom mesh
// 4. Player collects unlock item ? gains channel ? can interact with broom

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Code/Utility/Interactable.h"
#include "BroomActor.generated.h"

UCLASS()
class WIZARDJAM_API ABroomActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ABroomActor();

    // ========================================================================
    // IINTERACTABLE INTERFACE IMPLEMENTATION
    // ========================================================================

    // Returns broom name for hover tooltip
    virtual FText GetTooltipText_Implementation() const override;

    // Returns interaction prompt or locked message
    virtual FText GetInteractionPrompt_Implementation() const override;

    // Unused for this actor
    virtual FText GetDetailedInfo_Implementation() const override;

    // Checks if player has required channel
    virtual bool CanInteract_Implementation() const override;

    // Mounts player to broom, enables AC_BroomComponent
    virtual void OnInteract_Implementation(AActor* Interactor) override;

    // Returns interaction range
    virtual float GetInteractionRange_Implementation() const override;

protected:
    virtual void BeginPlay() override;

    // ========================================================================
    // COMPONENTS
    // ========================================================================

    // Root component - broom mesh
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BroomMesh;

    // ========================================================================
    // CONFIGURATION
    // Designer sets these in Blueprint
    // ========================================================================

    // Display name for tooltip
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Configuration")
    FText BroomName;

    // Channel player must have to interact (e.g., "BroomFlight")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Configuration")
    FName RequiredChannel;

    // Interaction range in units
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Configuration")
    float InteractionRange;

    // Socket name on broom mesh where player attaches
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Broom|Configuration")
    FName MountSocketName;

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================

    // Checks if interacting actor has required channel
    bool HasRequiredChannel(AActor* Interactor) const;

    // Enables flight on player's AC_BroomComponent
    void EnablePlayerFlight(AActor* Player);

    // Attaches player to broom mount point
    void MountPlayer(AActor* Player);
};