// InteractionComponent.cpp
// Implementation of interaction detection and tooltip display system
// Developer: Marcus Daley
// Date: December 31, 2025

#include "Code/Utility/InteractionComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Code/Utility/Interactable.h"
#include "Code/UI/TooltipWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteraction, Log, All);

UInteractionComponent::UInteractionComponent()
    : InteractionTraceRange(300.0f)
    , bShowDebugTrace(false)
    , CurrentFocusedActor(nullptr)
    , PreviousFocusedActor(nullptr)
    , TooltipWidgetInstance(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // Check 10 times per second
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogInteraction, Log,
        TEXT("[InteractionComponent] Initialized - Range: %.1f"), InteractionTraceRange);

    if (!CreateTooltipWidget())
    {
        UE_LOG(LogInteraction, Error, TEXT("[InteractionComponent] Failed to create tooltip widget!"));
    }
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Perform interaction trace
    FHitResult HitResult;
    bool bHitInteractable = PerformInteractionTrace(HitResult);

    // Update focused actor
    AActor* NewFocusedActor = bHitInteractable ? HitResult.GetActor() : nullptr;
    UpdateFocusedActor(NewFocusedActor);
}

bool UInteractionComponent::PerformInteractionTrace(FHitResult& OutHitResult)
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        return false;
    }

    APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
    if (!PC)
    {
        return false;
    }

    // Get viewport size to find screen center
    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

    // Screen center is where the crosshair is displayed
    FVector2D ScreenCenter(ViewportSizeX / 2.0f, ViewportSizeY / 2.0f);

    // Convert screen position to 3D world ray
    // This gives us the exact direction the crosshair is pointing
    FVector WorldLocation;
    FVector WorldDirection;
    if (!PC->DeprojectScreenPositionToWorld(ScreenCenter.X, ScreenCenter.Y, WorldLocation, WorldDirection))
    {
        // Deprojection failed - viewport might not be valid
        return false;
    }

    // Trace from screen center outward along crosshair direction
    FVector TraceStart = WorldLocation;
    FVector TraceEnd = WorldLocation + (WorldDirection * InteractionTraceRange);

    // Setup trace parameters
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(OwnerPawn);
    TraceParams.bTraceComplex = false;

    // Perform line trace using Visibility channel
    // Interactable actors must have collision set to block Visibility
    UWorld* World = GetWorld();
    bool bHit = World->LineTraceSingleByChannel(
        OutHitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        TraceParams
    );

    // Debug visualization (enable in Blueprint with bShowDebugTrace)
    if (bShowDebugTrace)
    {
        FColor DebugColor = bHit ? FColor::Yellow : FColor::Blue;
        DrawDebugLine(World, TraceStart, bHit ? OutHitResult.ImpactPoint : TraceEnd,
            DebugColor, false, 0.2f, 0, 2.0f);

        if (bHit)
        {
            DrawDebugSphere(World, OutHitResult.ImpactPoint, 10.0f, 12, DebugColor, false, 0.2f);
        }
    }

    // Return true only if hit valid interactable actor
    if (bHit && IsValidInteractableActor(OutHitResult.GetActor()))
    {
        return true;
    }

    return false;
}

void UInteractionComponent::UpdateFocusedActor(AActor* NewFocusedActor)
{
    // No change in focus
    if (NewFocusedActor == CurrentFocusedActor)
    {
        return;
    }

    // Store previous
    PreviousFocusedActor = CurrentFocusedActor;
    CurrentFocusedActor = NewFocusedActor;

    // Player stopped looking at interactable
    if (!CurrentFocusedActor)
    {
        HideTooltip();
        OnInteractableTargeted.Broadcast(false);  // Broadcast to HUD
        UE_LOG(LogInteraction, Log, TEXT("[InteractionComponent] Lost focus on interactable"));
        return;
    }

    // Player started looking at interactable
    IInteractable* InteractableInterface = Cast<IInteractable>(CurrentFocusedActor);
    if (!InteractableInterface)
    {
        HideTooltip();
        OnInteractableTargeted.Broadcast(false);
        return;
    }

    // Get tooltip text
    FText TooltipText = InteractableInterface->Execute_GetTooltipText(CurrentFocusedActor);
    FText InteractionPrompt = InteractableInterface->Execute_GetInteractionPrompt(CurrentFocusedActor);

    // Show tooltip and change crosshair
    ShowTooltip(TooltipText, InteractionPrompt);
    OnInteractableTargeted.Broadcast(true);  //  Crosshair turns green

    UE_LOG(LogInteraction, Log, TEXT("[InteractionComponent] Now focusing: %s — Tooltip: %s"),
        *CurrentFocusedActor->GetName(), *TooltipText.ToString());
}

void UInteractionComponent::ShowTooltip(const FText& TooltipText, const FText& InteractionPrompt)
{
    if (!TooltipWidgetInstance)
    {
        UE_LOG(LogInteraction, Warning,
            TEXT("[InteractionComponent] Cannot show tooltip — Widget instance is null!"));
        return;
    }

    UTooltipWidget* TooltipWidget = Cast<UTooltipWidget>(TooltipWidgetInstance);
    if (!TooltipWidget)
    {
        UE_LOG(LogInteraction, Error,
            TEXT("[InteractionComponent] TooltipWidgetInstance is not UTooltipWidget class!"));
        return;
    }

    // Update tooltip text using renamed function to avoid UWidget collision
    TooltipWidget->SetDisplayText(TooltipText);
    TooltipWidget->SetInteractionPrompt(InteractionPrompt);

    // Show widget
    TooltipWidgetInstance->SetVisibility(ESlateVisibility::Visible);
}

void UInteractionComponent::HideTooltip()
{
    if (!TooltipWidgetInstance)
    {
        return;
    }

    TooltipWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
}

bool UInteractionComponent::CreateTooltipWidget()
{
    if (!TooltipWidgetClass)
    {
        UE_LOG(LogInteraction, Error,
            TEXT("[InteractionComponent] TooltipWidgetClass not set in Player Blueprint!"));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
    {
        return false;
    }

    // Create widget instance
    TooltipWidgetInstance = CreateWidget<UUserWidget>(PC, TooltipWidgetClass);
    if (!TooltipWidgetInstance)
    {
        UE_LOG(LogInteraction, Error, TEXT("[InteractionComponent] Failed to create tooltip widget!"));
        return false;
    }

    // Add to viewport (hidden by default)
    TooltipWidgetInstance->AddToViewport(9998); // High Z-order but below results widget
    TooltipWidgetInstance->SetVisibility(ESlateVisibility::Hidden);

    UE_LOG(LogInteraction, Log, TEXT("[InteractionComponent] Tooltip widget created and cached"));
    return true;
}

bool UInteractionComponent::IsValidInteractableActor(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    // Check if actor implements IInteractable
    IInteractable* InteractableInterface = Cast<IInteractable>(Actor);
    if (!InteractableInterface)
    {
        return false;
    }

    // Check if actor can be interacted with right now
    bool bCanInteract = InteractableInterface->Execute_CanInteract(Actor);
    if (!bCanInteract)
    {
        return false;
    }

    return true;
}

bool UInteractionComponent::AttemptInteraction()
{
    if (!CurrentFocusedActor)
    {
        UE_LOG(LogInteraction, Log, TEXT("[InteractionComponent] No focused actor to interact with"));
        return false;
    }

    // Validate interactable interface
    IInteractable* InteractableInterface = Cast<IInteractable>(CurrentFocusedActor);
    if (!InteractableInterface)
    {
        UE_LOG(LogInteraction, Warning,
            TEXT("[InteractionComponent] Focused actor does not implement IInteractable!"));
        return false;
    }

    // Check if can interact
    if (!InteractableInterface->Execute_CanInteract(CurrentFocusedActor))
    {
        UE_LOG(LogInteraction, Log,
            TEXT("[InteractionComponent] Cannot interact with %s — CanInteract() returned false"),
            *CurrentFocusedActor->GetName());
        return false;
    }

    // Check interaction range
    float InteractionRange = InteractableInterface->Execute_GetInteractionRange(CurrentFocusedActor);
    float DistanceToActor = FVector::Dist(GetOwner()->GetActorLocation(),
        CurrentFocusedActor->GetActorLocation());

    if (DistanceToActor > InteractionRange)
    {
        UE_LOG(LogInteraction, Log,
            TEXT("[InteractionComponent] Too far from %s — Distance: %.1f, Range: %.1f"),
            *CurrentFocusedActor->GetName(), DistanceToActor, InteractionRange);
        return false;
    }

    // Perform interaction
    InteractableInterface->Execute_OnInteract(CurrentFocusedActor, GetOwner());

    UE_LOG(LogInteraction, Log,
        TEXT("[InteractionComponent] Interacted with %s"), *CurrentFocusedActor->GetName());
    return true;
}