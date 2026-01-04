// ============================================================================
// InputCharacter.h
// Developer: Marcus Daley
// Date: December 30, 2025
// Project: WizardJam
// ============================================================================
// Purpose:
// Base character class with Enhanced Input System integration.
// Provides camera setup, movement, look, jump, fire, and spell cycling.
//
// Designer Usage:
// 1. In Blueprint child, set DefaultMappingContext to your IMC_Default
// 2. Set MoveAction, LookAction, JumpAction, FireAction to your Input Actions
// 3. Set CycleSpellAction for spell switching (mouse wheel)
// 4. Set SelectSpellSlotAction for number key selection (1-4)
// 5. Play - all input works automatically
//
// Input Action Setup Required:
// - IA_Move (Axis2D): WASD movement
// - IA_Look (Axis2D): Mouse look
// - IA_Jump (Digital): Spacebar
// - IA_Fire (Digital): Left mouse button
// - IA_CycleSpell (Axis1D): Mouse wheel scroll for spell cycling
// - IA_SelectSpellSlot (Axis1D): Number keys 1-4 with Scalar modifiers
//
// IMC_Default Setup for IA_SelectSpellSlot:
// Add ONE mapping with multiple keys using Scalar modifiers:
//   Key: 1 -> Modifier: Scalar = 0.0  (Slot 0 / Flame)
//   Key: 2 -> Modifier: Scalar = 1.0  (Slot 1 / Ice)
//   Key: 3 -> Modifier: Scalar = 2.0  (Slot 2 / Lightning)
//   Key: 4 -> Modifier: Scalar = 3.0  (Slot 3 / Arcane)
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_LOG_CATEGORY_EXTERN(LogInputCharacter, Log, All);

UCLASS()
class WIZARDJAM_API AInputCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AInputCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // ========================================================================
    // Input Handlers - Movement
    // ========================================================================

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void OnInteractPressed();
    // ========================================================================
    // Input Handlers - Combat (virtual for child override)
    // ========================================================================

    // Fire input - child classes override to call their combat system
    UFUNCTION(BlueprintCallable, Category = "Input|Combat")
    virtual void HandleFireInput();

    // Spell cycling - child classes override to cycle through available spells
    // Value.Get<float>() returns positive for scroll up, negative for scroll down
    UFUNCTION(BlueprintCallable, Category = "Input|Combat")
    virtual void HandleCycleSpellInput(const FInputActionValue& Value);

    // Direct spell selection by index (for number keys 1-4)
    // Override in child class to select specific spell slot
    UFUNCTION(BlueprintCallable, Category = "Input|Combat")
    virtual void HandleSelectSpellSlot(int32 SlotIndex);

    // ========================================================================
    // Camera Components
    // ========================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    // ========================================================================
    // Input Configuration
    // Set these in your Blueprint child class
    // ========================================================================

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* JumpAction;

    // Fire action - bound to HandleFireInput() which child classes override
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* FireAction;

    // Spell cycling action - mouse wheel
    // Create as Axis1D: positive = next spell, negative = previous spell
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* CycleSpellAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input",
        meta = (DisplayName = "Select Spell Slot Action",
            ToolTip = "Single action for all spell slots. Use Scalar modifiers in IMC: Key 1=0.0, Key 2=1.0, Key 3=2.0, Key 4=3.0"))
    UInputAction* SelectSpellSlotAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;  // E key for interaction
private:
    // Internal handler that extracts slot index from scalar value
    void HandleSelectSpellSlotInput(const FInputActionValue& Value);
};