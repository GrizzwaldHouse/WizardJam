// Copyright Marcus Daley 2025. All Rights Reserved.
// Updated: 12/06/2025 - Fixed delegate to pass button reference on click

#include "MCPButtonWidget.h"

// Constructor - initialize default values
UMCPButtonWidget::UMCPButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ButtonText(FText::FromString(TEXT("Button")))
	, AssociatedCommand(TEXT(""))
	, Button(nullptr)
	, Label(nullptr)
{
}

// Called when the widget is fully constructed
// Binds button click events and sets initial text
void UMCPButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Validate required widgets exist
	if (!ValidateWidgets())
	{
		UE_LOG(LogTemp, Error, TEXT("MCPButtonWidget: Failed validation - ensure Button and Label exist in Blueprint"));
		return;
	}

	// Set initial text from property
	Label->SetText(ButtonText);

	// Bind internal click handler to the button
	Button->OnClicked.AddDynamic(this, &UMCPButtonWidget::HandleButtonClicked);

	UE_LOG(LogTemp, Display, TEXT("MCPButtonWidget: Initialized - %s"), *ButtonText.ToString());
}

// Called when the widget is being destroyed
// Cleans up delegate bindings to prevent dangling references
void UMCPButtonWidget::NativeDestruct()
{
	// Remove delegate binding before destruction
	if (Button)
	{
		Button->OnClicked.RemoveDynamic(this, &UMCPButtonWidget::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

// Set the button text at runtime
// Updates both the stored property and the visible label
// @param NewText - The text to display on the button
void UMCPButtonWidget::SetButtonText(const FText& NewText)
{
	ButtonText = NewText;

	if (Label)
	{
		Label->SetText(NewText);
	}
}

// Enable or disable the button
// Disabled buttons cannot be clicked and typically appear grayed out
// @param bEnabled - True to enable interaction, false to disable
void UMCPButtonWidget::SetButtonEnabled(bool bEnabled)
{
	if (Button)
	{
		Button->SetIsEnabled(bEnabled);
		UE_LOG(LogTemp, Display, TEXT("MCPButtonWidget: %s - Enabled: %s"),
			*ButtonText.ToString(), bEnabled ? TEXT("true") : TEXT("false"));
	}
}

// Internal handler called when the underlying UButton is clicked
// Broadcasts the OnButtonClicked delegate with this button instance
// Parent widgets can then identify which button was clicked
void UMCPButtonWidget::HandleButtonClicked()
{
	UE_LOG(LogTemp, Display, TEXT("MCPButtonWidget: Clicked - %s (Command: %s)"),
		*ButtonText.ToString(), *AssociatedCommand);

	// Broadcast with self reference so parent can identify which button was clicked
	OnButtonClicked.Broadcast(this);
}

// Validate that required child widgets exist
// Called during NativeConstruct to ensure proper Blueprint setup
// @return True if both Button and Label widgets are valid
bool UMCPButtonWidget::ValidateWidgets() const
{
	if (!Button)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPButtonWidget: Button widget is null - must be named 'Button' in Blueprint"));
		return false;
	}

	if (!Label)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPButtonWidget: Label widget is null - must be named 'Label' in Blueprint"));
		return false;
	}

	return true;
}