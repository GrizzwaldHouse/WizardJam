// AC_BroomComponent.cpp
// Spawn-on-demand broom flight with full HUD integration
//
// Developer: Marcus Daley
// Date: January 1, 2026
//
// FULL HUD INTEGRATION:
// - Broadcasts OnStaminaVisualUpdate with color codes
// - Broadcasts OnForcedDismount when stamina depletes
// - Broadcasts OnBoostStateChanged when shift pressed
// - Provides GetFlightStaminaPercent() for UI

#include "Code/Utility/AC_BroomComponent.h"
#include "Code/Utility/AC_StaminaComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY(LogBroomComponent);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UAC_BroomComponent::UAC_BroomComponent()
    : BroomVisualClass(nullptr)
    , FlightMappingContext(nullptr)
    , ToggleAction(nullptr)
    , AscendAction(nullptr)
    , DescendAction(nullptr)
    , BoostAction(nullptr)
    , FlySpeed(600.0f)
    , BoostSpeed(1200.0f)
    , VerticalSpeed(400.0f)
    , StaminaDrainRate(10.0f)
    , BoostStaminaDrainRate(25.0f)
    , MinStaminaToFly(20.0f)
    , MountSocketName(FName("MountSocket"))
    , bIsFlying(false)
    , bIsBoosting(false)
    , CurrentVerticalVelocity(0.0f)
    , SpawnedBroomVisual(nullptr)
    , StaminaComponent(nullptr)
    , MovementComponent(nullptr)
    , PlayerController(nullptr)
    , InputSubsystem(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAC_BroomComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[BroomComponent] No owner actor!"));
        return;
    }

    // Cache stamina component
    StaminaComponent = Owner->FindComponentByClass<UAC_StaminaComponent>();
    if (!StaminaComponent)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[%s] No AC_StaminaComponent found! Flight disabled."),
            *Owner->GetName());
        return;
    }

    // Bind to stamina changed event
    StaminaComponent->OnStaminaChanged.AddDynamic(this, &UAC_BroomComponent::OnStaminaChanged);

    // Cache character movement component
    ACharacter* OwnerChar = Cast<ACharacter>(Owner);
    if (!OwnerChar)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[%s] Owner is not ACharacter! Flight disabled."),
            *Owner->GetName());
        return;
    }

    MovementComponent = OwnerChar->GetCharacterMovement();
    if (!MovementComponent)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[%s] Character has no CharacterMovementComponent!"),
            *Owner->GetName());
        return;
    }

    // Cache player controller
    PlayerController = Cast<APlayerController>(OwnerChar->GetController());
    if (!PlayerController)
    {
        UE_LOG(LogBroomComponent, Warning,
            TEXT("[%s] No PlayerController (might be AI or not possessed yet)"),
            *Owner->GetName());
        return;
    }

    // Cache enhanced input subsystem
    InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
        PlayerController->GetLocalPlayer());

    if (!InputSubsystem)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[%s] Failed to get EnhancedInputLocalPlayerSubsystem!"),
            *Owner->GetName());
        return;
    }

    // Verify mount socket exists on player mesh
    if (OwnerChar->GetMesh())
    {
        if (OwnerChar->GetMesh()->DoesSocketExist(MountSocketName))
        {
            UE_LOG(LogBroomComponent, Display,
                TEXT("[%s] ? Found mount socket: %s on player mesh"),
                *Owner->GetName(), *MountSocketName.ToString());
        }
        else
        {
            UE_LOG(LogBroomComponent, Error,
                TEXT("[%s] ? Socket '%s' NOT FOUND on player mesh! Broom won't attach!"),
                *Owner->GetName(), *MountSocketName.ToString());

            // List available sockets for debugging
            TArray<FName> SocketNames = OwnerChar->GetMesh()->GetAllSocketNames();
            UE_LOG(LogBroomComponent, Log, TEXT("  Available sockets (%d):"), SocketNames.Num());
            for (const FName& SocketName : SocketNames)
            {
                UE_LOG(LogBroomComponent, Log, TEXT("    - %s"), *SocketName.ToString());
            }
        }
    }

    // Verify BroomVisualClass is set
    if (!BroomVisualClass)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("[%s] ? BroomVisualClass is NULL! Set to BP_Broom_Combat in Blueprint!"),
            *Owner->GetName());
    }
    else
    {
        UE_LOG(LogBroomComponent, Display,
            TEXT("[%s] ? BroomVisualClass set: %s"),
            *Owner->GetName(), *BroomVisualClass->GetName());
    }

    // Setup input bindings
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(
        PlayerController->InputComponent))
    {
        SetupFlightInputBindings(EnhancedInput);
    }

    UE_LOG(LogBroomComponent, Display,
        TEXT("[%s] BroomComponent ready | Stamina: %.0f/%.0f | FlySpeed: %.0f | Socket: %s"),
        *Owner->GetName(),
        StaminaComponent->GetCurrentStamina(),
        StaminaComponent->GetMaxStamina(),
        FlySpeed,
        OwnerChar->GetMesh()->DoesSocketExist(MountSocketName) ? TEXT("FOUND") : TEXT("MISSING"));
}

void UAC_BroomComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsFlying)
    {
        return;
    }

    // Drain stamina while flying
    DrainStamina(DeltaTime);

    // Check stamina depletion
    if (!HasSufficientStamina())
    {
        ForceDismount();
        return;
    }

    // Apply vertical movement
    ApplyVerticalMovement(DeltaTime);
}

// ============================================================================
// PUBLIC API
// ============================================================================

void UAC_BroomComponent::SetFlightEnabled(bool bEnabled)
{
    if (bIsFlying == bEnabled)
    {
        return; // No change
    }

    UE_LOG(LogBroomComponent, Warning,
        TEXT("========== SetFlightEnabled(%s) START =========="),
        bEnabled ? TEXT("TRUE") : TEXT("FALSE"));

    // Check stamina before enabling
    if (bEnabled && !HasSufficientStamina())
    {
        UE_LOG(LogBroomComponent, Warning,
            TEXT("  ? Insufficient stamina (need %.0f, have %.0f)"),
            MinStaminaToFly,
            StaminaComponent ? StaminaComponent->GetCurrentStamina() : 0.0f);

        // Broadcast red color for insufficient stamina
        OnStaminaVisualUpdate.Broadcast(FLinearColor::Red);
        return;
    }

    // Update state
    bIsFlying = bEnabled;

    if (bEnabled)
    {
        // 1. Spawn broom visual and attach to player
        SpawnBroomVisual();

        // 2. Switch to flying movement mode
        SetMovementMode(true);

        // 3. Add flight input context
        UpdateInputContext(true);

        // 4. Update UI - cyan color for flying
        OnFlightStateChanged.Broadcast(true);
        OnStaminaVisualUpdate.Broadcast(FLinearColor(0.0f, 1.0f, 1.0f)); // Cyan
    }
    else
    {
        // 1. Destroy broom visual
        DestroyBroomVisual();

        // 2. Return to walking mode
        SetMovementMode(false);

        // 3. Remove flight input context
        UpdateInputContext(false);

        // 4. Reset boost state
        if (bIsBoosting)
        {
            bIsBoosting = false;
            OnBoostStateChanged.Broadcast(false);
        }
        CurrentVerticalVelocity = 0.0f;

        // 5. Update UI - green color for grounded
        OnFlightStateChanged.Broadcast(false);
        OnStaminaVisualUpdate.Broadcast(FLinearColor::Green);
    }

    UE_LOG(LogBroomComponent, Warning,
        TEXT("========== SetFlightEnabled COMPLETE | Flying: %s =========="),
        bIsFlying ? TEXT("YES") : TEXT("NO"));
}

float UAC_BroomComponent::GetFlightStaminaPercent() const
{
    if (!StaminaComponent)
    {
        return 0.0f;
    }

    float MaxStamina = StaminaComponent->GetMaxStamina();
    if (MaxStamina <= 0.0f)
    {
        return 0.0f;
    }

    return StaminaComponent->GetCurrentStamina() / MaxStamina;
}

// ============================================================================
// BROOM VISUAL SPAWNING
// ============================================================================

void UAC_BroomComponent::SpawnBroomVisual()
{
    if (!BroomVisualClass)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Cannot spawn broom - BroomVisualClass is NULL!"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Cannot spawn broom - World is NULL!"));
        return;
    }

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Cannot spawn broom - Owner is not ACharacter!"));
        return;
    }

    // Destroy old broom if it somehow still exists
    if (SpawnedBroomVisual)
    {
        UE_LOG(LogBroomComponent, Warning,
            TEXT("  ?? Old broom visual still exists - destroying before spawning new one"));
        SpawnedBroomVisual->Destroy();
        SpawnedBroomVisual = nullptr;
    }

    // Spawn broom visual actor at player location
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnedBroomVisual = World->SpawnActor<AActor>(
        BroomVisualClass,
        OwnerChar->GetActorLocation(),
        OwnerChar->GetActorRotation(),
        SpawnParams
    );

    if (!SpawnedBroomVisual)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Failed to spawn broom visual actor!"));
        return;
    }

    UE_LOG(LogBroomComponent, Display,
        TEXT("  ? Spawned broom visual: %s"), *SpawnedBroomVisual->GetName());

    // Attach broom to player's mount socket
    USkeletalMeshComponent* PlayerMesh = OwnerChar->GetMesh();
    if (!PlayerMesh)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Player has no skeletal mesh - cannot attach broom!"));
        SpawnedBroomVisual->Destroy();
        SpawnedBroomVisual = nullptr;
        return;
    }

    // Verify socket exists
    if (!PlayerMesh->DoesSocketExist(MountSocketName))
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Socket '%s' does not exist on player mesh!"),
            *MountSocketName.ToString());
        SpawnedBroomVisual->Destroy();
        SpawnedBroomVisual = nullptr;
        return;
    }

    // Attach to player
    bool bAttached = SpawnedBroomVisual->AttachToComponent(
        PlayerMesh,
        FAttachmentTransformRules::SnapToTargetIncludingScale,
        MountSocketName
    );

    if (bAttached)
    {
        UE_LOG(LogBroomComponent, Display,
            TEXT("  ? Broom attached to player socket: %s"), *MountSocketName.ToString());
    }
    else
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? Failed to attach broom to socket!"));
    }

    // Disable collision on spawned broom (visual only)
    SpawnedBroomVisual->SetActorEnableCollision(false);
}

void UAC_BroomComponent::DestroyBroomVisual()
{
    if (!SpawnedBroomVisual)
    {
        return; // Nothing to destroy
    }

    UE_LOG(LogBroomComponent, Display,
        TEXT("  ? Destroying broom visual: %s"), *SpawnedBroomVisual->GetName());

    SpawnedBroomVisual->Destroy();
    SpawnedBroomVisual = nullptr;
}

// ============================================================================
// INPUT BINDING
// ============================================================================

void UAC_BroomComponent::SetupFlightInputBindings(UEnhancedInputComponent* EnhancedInput)
{
    if (!EnhancedInput)
    {
        return;
    }

    if (ToggleAction)
    {
        EnhancedInput->BindAction(ToggleAction, ETriggerEvent::Started,
            this, &UAC_BroomComponent::HandleToggleInput);
        UE_LOG(LogBroomComponent, Display,
            TEXT("[%s] Bound ToggleAction"), *GetOwner()->GetName());
    }

    if (AscendAction)
    {
        EnhancedInput->BindAction(AscendAction, ETriggerEvent::Triggered,
            this, &UAC_BroomComponent::HandleAscendInput);
        EnhancedInput->BindAction(AscendAction, ETriggerEvent::Completed,
            this, &UAC_BroomComponent::HandleAscendInput);
        UE_LOG(LogBroomComponent, Display,
            TEXT("[%s] Bound AscendAction"), *GetOwner()->GetName());
    }

    if (DescendAction)
    {
        EnhancedInput->BindAction(DescendAction, ETriggerEvent::Triggered,
            this, &UAC_BroomComponent::HandleDescendInput);
        EnhancedInput->BindAction(DescendAction, ETriggerEvent::Completed,
            this, &UAC_BroomComponent::HandleDescendInput);
        UE_LOG(LogBroomComponent, Display,
            TEXT("[%s] Bound DescendAction"), *GetOwner()->GetName());
    }

    if (BoostAction)
    {
        EnhancedInput->BindAction(BoostAction, ETriggerEvent::Started,
            this, &UAC_BroomComponent::HandleBoostInput);
        EnhancedInput->BindAction(BoostAction, ETriggerEvent::Completed,
            this, &UAC_BroomComponent::HandleBoostInput);
        UE_LOG(LogBroomComponent, Display,
            TEXT("[%s] Bound BoostAction"), *GetOwner()->GetName());
    }
}

// ============================================================================
// INPUT HANDLERS
// ============================================================================

void UAC_BroomComponent::HandleToggleInput(const FInputActionValue& Value)
{
    SetFlightEnabled(!bIsFlying);
}

void UAC_BroomComponent::HandleAscendInput(const FInputActionValue& Value)
{
    if (!bIsFlying) return;

    float InputValue = Value.Get<float>();
    CurrentVerticalVelocity = InputValue * VerticalSpeed;
}

void UAC_BroomComponent::HandleDescendInput(const FInputActionValue& Value)
{
    if (!bIsFlying) return;

    float InputValue = Value.Get<float>();
    CurrentVerticalVelocity = -InputValue * VerticalSpeed;
}

void UAC_BroomComponent::HandleBoostInput(const FInputActionValue& Value)
{
    if (!bIsFlying) return;

    bool bBoostPressed = Value.Get<bool>();

    // Only broadcast if state actually changed
    if (bIsBoosting != bBoostPressed)
    {
        bIsBoosting = bBoostPressed;
        OnBoostStateChanged.Broadcast(bIsBoosting);

        if (MovementComponent)
        {
            MovementComponent->MaxFlySpeed = bIsBoosting ? BoostSpeed : FlySpeed;

            UE_LOG(LogBroomComponent, Log,
                TEXT("[%s] Boost %s | Speed: %.0f"),
                *GetOwner()->GetName(),
                bIsBoosting ? TEXT("ON") : TEXT("OFF"),
                MovementComponent->MaxFlySpeed);
        }

        // Update stamina bar color
        if (bIsBoosting)
        {
            OnStaminaVisualUpdate.Broadcast(FLinearColor(1.0f, 0.5f, 0.0f)); // Orange
        }
        else
        {
            OnStaminaVisualUpdate.Broadcast(FLinearColor(0.0f, 1.0f, 1.0f)); // Cyan
        }
    }
}

// ============================================================================
// FLIGHT MECHANICS
// ============================================================================

void UAC_BroomComponent::ApplyVerticalMovement(float DeltaTime)
{
    if (!MovementComponent || FMath::IsNearlyZero(CurrentVerticalVelocity))
    {
        return;
    }

    FVector Velocity = MovementComponent->Velocity;
    Velocity.Z = CurrentVerticalVelocity;
    MovementComponent->Velocity = Velocity;
}

void UAC_BroomComponent::SetMovementMode(bool bFlying)
{
    if (!MovementComponent)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? MovementComponent is NULL!"));
        return;
    }

    UE_LOG(LogBroomComponent, Log,
        TEXT("  Before: MovementMode = %s"),
        *UEnum::GetValueAsString(MovementComponent->MovementMode));

    if (bFlying)
    {
        MovementComponent->SetMovementMode(MOVE_Flying);
        MovementComponent->MaxFlySpeed = FlySpeed;

        UE_LOG(LogBroomComponent, Display,
            TEXT("  ? Movement mode = MOVE_Flying | Speed: %.0f"), FlySpeed);
    }
    else
    {
        MovementComponent->SetMovementMode(MOVE_Walking);
        CurrentVerticalVelocity = 0.0f;

        UE_LOG(LogBroomComponent, Display,
            TEXT("  ? Movement mode = MOVE_Walking"));
    }

    UE_LOG(LogBroomComponent, Log,
        TEXT("  After: MovementMode = %s"),
        *UEnum::GetValueAsString(MovementComponent->MovementMode));
}

void UAC_BroomComponent::UpdateInputContext(bool bAddContext)
{
    if (!InputSubsystem)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? InputSubsystem is NULL!"));
        return;
    }

    if (!FlightMappingContext)
    {
        UE_LOG(LogBroomComponent, Error,
            TEXT("  ? FlightMappingContext is NULL! Set in BP_WizardPlayer!"));
        return;
    }

    if (bAddContext)
    {
        InputSubsystem->AddMappingContext(FlightMappingContext, 1);
        UE_LOG(LogBroomComponent, Display,
            TEXT("  ? Added FlightMappingContext (Priority 1)"));
    }
    else
    {
        InputSubsystem->RemoveMappingContext(FlightMappingContext);
        UE_LOG(LogBroomComponent, Display,
            TEXT("  ? Removed FlightMappingContext"));
    }
}

bool UAC_BroomComponent::HasSufficientStamina() const
{
    if (!StaminaComponent)
    {
        return false;
    }

    return StaminaComponent->GetCurrentStamina() >= MinStaminaToFly;
}

void UAC_BroomComponent::ForceDismount()
{
    UE_LOG(LogBroomComponent, Warning,
        TEXT("[%s] ?? FORCE DISMOUNT - Stamina depleted!"),
        *GetOwner()->GetName());

    // Broadcast forced dismount event for HUD
    OnForcedDismount.Broadcast();

    // Disable flight
    SetFlightEnabled(false);

    // Red color for depleted stamina
    OnStaminaVisualUpdate.Broadcast(FLinearColor::Red);
}

// ============================================================================
// STAMINA INTEGRATION
// ============================================================================

void UAC_BroomComponent::DrainStamina(float DeltaTime)
{
    if (!StaminaComponent)
    {
        return;
    }

    float DrainRate = bIsBoosting ? BoostStaminaDrainRate : StaminaDrainRate;
    float DrainAmount = DrainRate * DeltaTime;

    // Use ConsumeStamina() to drain stamina
    StaminaComponent->ConsumeStamina(DrainAmount);
}

void UAC_BroomComponent::OnStaminaChanged(AActor* Owner, float NewStamina, float Delta)
{
    // Force dismount if stamina drops below minimum while flying
    if (bIsFlying && NewStamina < MinStaminaToFly)
    {
        ForceDismount();
    }
}