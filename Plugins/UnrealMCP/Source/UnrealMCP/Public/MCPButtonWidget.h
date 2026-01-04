// Copyright Marcus Daley 2025. All Rights Reserved.
// Created: 11/30/2025
// Updated: 12/06/2025 - Fixed delegate signature to pass button reference for click handling
// Purpose: Reusable button widget for MCP command panel with C++ functionality

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "MCPButtonWidget.generated.h"

// Forward declaration for delegate parameter
class UMCPButtonWidget;

// Delegate fired when button is clicked - passes the clicked button for identification
// This allows parent widgets to determine which button was clicked without using lambdas
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMCPButtonClicked, UMCPButtonWidget*, ClickedButton);

// Base class for reusable MCP buttons with text labels
// Designers can extend this in Blueprint and bind click events without C++ knowledge
//
// Usage:
//   1. Create Blueprint child class (e.g., WBP_MCPButton)
//   2. Add Button and TextBlock widgets named "Button" and "Label"
//   3. Bind to OnButtonClicked delegate in parent widget
//   4. Use AssociatedCommand to store context data for the button
UCLASS()
class UNREALMCP_API UMCPButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMCPButtonWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when widget is constructed
	virtual void NativeConstruct() override;

	// Called when widget is destroyed
	virtual void NativeDestruct() override;

public:
	// ========== EXPOSED PROPERTIES ==========

	// The text displayed on the button
	UPROPERTY(BlueprintReadWrite, Category = "MCP Button", meta = (ExposeOnSpawn = true))
	FText ButtonText;

	// Command or identifier associated with this button
	// Parent widgets can read this to determine what action to take on click
	UPROPERTY(BlueprintReadWrite, Category = "MCP Button")
	FString AssociatedCommand;

	// ========== DELEGATES ==========

	// Fired when button is clicked - passes this button instance for identification
	UPROPERTY(BlueprintAssignable, Category = "MCP Button|Events")
	FOnMCPButtonClicked OnButtonClicked;

	// ========== BLUEPRINT-CALLABLE FUNCTIONS ==========

	// Set the button text at runtime
	// @param NewText - The text to display on the button
	UFUNCTION(BlueprintCallable, Category = "MCP Button")
	void SetButtonText(const FText& NewText);

	// Enable or disable the button
	// @param bEnabled - True to enable, false to disable
	UFUNCTION(BlueprintCallable, Category = "MCP Button")
	void SetButtonEnabled(bool bEnabled);

protected:
	// ========== WIDGET BINDINGS ==========

	// The button component - MUST be named "Button" in Blueprint
	UPROPERTY(BlueprintReadOnly, Category = "MCP Button", meta = (BindWidget))
	UButton* Button;

	// The text label - MUST be named "Label" in Blueprint
	UPROPERTY(BlueprintReadOnly, Category = "MCP Button", meta = (BindWidget))
	UTextBlock* Label;

private:
	// Internal click handler bound to Button component
	// Broadcasts OnButtonClicked with this button instance
	UFUNCTION()
	void HandleButtonClicked();

	// Validate that required widgets exist
	// @return True if Button and Label widgets are valid
	bool ValidateWidgets() const;
};