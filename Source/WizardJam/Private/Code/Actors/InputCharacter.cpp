// ============================================================================
// InputCharacter.cpp
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Implementation of base input character with Enhanced Input System.
// Handles movement, camera, and provides virtual functions for combat input.
//
// Single Action Spell Selection:
// Instead of binding 4 separate actions for keys 1-4, we use ONE action
// (IA_SelectSpellSlot) with Scalar modifiers in the Input Mapping Context.
// The scalar value (0.0, 1.0, 2.0, 3.0) determines which slot is selected.
// This is cleaner, more scalable, and requires less Blueprint configuration.
// ============================================================================

#include "Code/Actors/InputCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Code/Utility/InteractionComponent.h"
DEFINE_LOG_CATEGORY(LogInputCharacter);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

AInputCharacter::AInputCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Don't rotate character with controller - camera handles rotation
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // Configure character movement for third-person style
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

    // Create camera boom (spring arm)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    // Create follow camera attached to boom
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void AInputCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Add Input Mapping Context to local player subsystem
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
                UE_LOG(LogInputCharacter, Display,
                    TEXT("[%s] Input Mapping Context '%s' added successfully"),
                    *GetName(), *DefaultMappingContext->GetName());
            }
            else
            {
                UE_LOG(LogInputCharacter, Error,
                    TEXT("[%s] DefaultMappingContext is NULL! Assign in Blueprint."),
                    *GetName());
            }
        }
    }
}
void AInputCharacter::OnInteractPressed()
{
    // Find interaction component
    UInteractionComponent* InteractionComp = FindComponentByClass<UInteractionComponent>();

    if (!InteractionComp)
    {
        UE_LOG(LogInputCharacter, Warning,
            TEXT("[%s] No InteractionComponent found!"), *GetName());
        return;
    }

    // Attempt interaction
    InteractionComp->AttemptInteraction();
}
// ============================================================================
// INPUT SETUP
// ============================================================================

void AInputCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogInputCharacter, Error,
            TEXT("[%s] Failed to cast to EnhancedInputComponent!"),
            *GetName());
        return;
    }

    // ========================================================================
    // MOVEMENT BINDINGS
    // ========================================================================

    if (MoveAction)
    {
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInputCharacter::Move);
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning, TEXT("[%s] MoveAction is NULL"), *GetName());
    }

    if (LookAction)
    {
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInputCharacter::Look);
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning, TEXT("[%s] LookAction is NULL"), *GetName());
    }

    if (JumpAction)
    {
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning, TEXT("[%s] JumpAction is NULL"), *GetName());
    }

    // ========================================================================
    // COMBAT BINDINGS
    // ========================================================================

    if (FireAction)
    {
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Triggered, this, &AInputCharacter::HandleFireInput);
        UE_LOG(LogInputCharacter, Display,
            TEXT("[%s] FireAction '%s' bound successfully"),
            *GetName(), *FireAction->GetName());
    }
    else
    {
        UE_LOG(LogInputCharacter, Error,
            TEXT("[%s] *** FireAction is NULL! *** Create IA_Fire and assign in Blueprint!"),
            *GetName());
    }
    if (InteractAction)
    {
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started,
            this, &AInputCharacter::OnInteractPressed);
        UE_LOG(LogInputCharacter, Display,
            TEXT("[%s] InteractAction bound successfully"), *GetName());
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning, TEXT("[%s] InteractAction is NULL"), *GetName());
    }
    // ========================================================================
    // SPELL CYCLING (Mouse Wheel)
    // ========================================================================

    if (CycleSpellAction)
    {
        EnhancedInput->BindAction(CycleSpellAction, ETriggerEvent::Triggered,
            this, &AInputCharacter::HandleCycleSpellInput);
        UE_LOG(LogInputCharacter, Display,
            TEXT("[%s] CycleSpellAction bound (mouse wheel)"),
            *GetName());
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning,
            TEXT("[%s] CycleSpellAction is NULL - mouse wheel spell cycling disabled"),
            *GetName());
    }

    // ========================================================================
    // SPELL SLOT SELECTION (Single Action with Scalar Modifiers)
    // 
    // ONE action handles all number keys (1, 2, 3, 4)
    // Each key has a Scalar modifier in IMC_Default that sets the value:
    //   Key 1 -> Scalar 0.0 -> SlotIndex 0 (Flame)
    //   Key 2 -> Scalar 1.0 -> SlotIndex 1 (Ice)
    //   Key 3 -> Scalar 2.0 -> SlotIndex 2 (Lightning)
    //   Key 4 -> Scalar 3.0 -> SlotIndex 3 (Arcane)
    // ========================================================================

    if (SelectSpellSlotAction)
    {
        EnhancedInput->BindAction(SelectSpellSlotAction, ETriggerEvent::Started,
            this, &AInputCharacter::HandleSelectSpellSlotInput);
        UE_LOG(LogInputCharacter, Display,
            TEXT("[%s] SelectSpellSlotAction bound (keys 1-4 via Scalar modifiers)"),
            *GetName());
    }
    else
    {
        UE_LOG(LogInputCharacter, Warning,
            TEXT("[%s] SelectSpellSlotAction is NULL - number key spell selection disabled"),
            *GetName());
    }

    UE_LOG(LogInputCharacter, Display, TEXT("[%s] Input setup complete"), *GetName());
}

// ============================================================================
// MOVEMENT INPUT HANDLERS
// ============================================================================

void AInputCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller)
    {
        // Get controller yaw rotation (ignore pitch and roll)
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        // Calculate forward and right directions based on camera
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // Apply movement input
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void AInputCharacter::Look(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

// ============================================================================
// COMBAT INPUT HANDLERS (Virtual - Override in Child Classes)
// ============================================================================

void AInputCharacter::HandleFireInput()
{
    // Base implementation does nothing
    // BasePlayer overrides this to call CombatComponent->FireProjectileByType()
    UE_LOG(LogInputCharacter, Verbose,
        TEXT("[%s] HandleFireInput called (base - no action)"),
        *GetName());
}

void AInputCharacter::HandleCycleSpellInput(const FInputActionValue& Value)
{
    // Base implementation does nothing
    // BasePlayer overrides this to cycle through unlocked spells
    // Value.Get<float>() > 0 = scroll up (next spell)
    // Value.Get<float>() < 0 = scroll down (previous spell)
    float ScrollValue = Value.Get<float>();
    UE_LOG(LogInputCharacter, Verbose,
        TEXT("[%s] HandleCycleSpellInput called (base - no action) | ScrollValue: %.2f"),
        *GetName(), ScrollValue);
}

void AInputCharacter::HandleSelectSpellSlot(int32 SlotIndex)
{
    // Base implementation does nothing
    // BasePlayer overrides this to select specific spell by index
    UE_LOG(LogInputCharacter, Verbose,
        TEXT("[%s] HandleSelectSpellSlot called (base - no action) | Slot: %d"),
        *GetName(), SlotIndex);
}


void AInputCharacter::HandleSelectSpellSlotInput(const FInputActionValue& Value)
{
    // Get the scalar value from the Input Action
    // This value is set by the Scalar modifier on each key binding in IMC_Default
    float ScalarValue = Value.Get<float>();

    // Convert to integer slot index (round to handle any float precision issues)
    int32 SlotIndex = FMath::RoundToInt(ScalarValue);

    UE_LOG(LogInputCharacter, Display,
        TEXT("[%s] Spell slot input | Scalar: %.1f | SlotIndex: %d"),
        *GetName(), ScalarValue, SlotIndex);

    // Call the virtual handler that child classes override
    HandleSelectSpellSlot(SlotIndex);
}