// Copyright Marcus Daley 2025. All Rights Reserved.
// Created: 11/30/2025
// Updated: 12/09/2025 - Fixed duplicate declarations and added schema button handler
// Purpose: C++ base widget for MCP command panel - handles all subsystem communication

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h" 
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "MCPCommandPanelWidget.generated.h"

// Forward declarations
class UMCPCommandPanelSubsystem;
class UMCPButtonWidget;

// C++ base class for the MCP command panel editor utility widget
// Handles all subsystem communication, delegate binding, and logic internally
//
// Blueprint Usage (WBP_MCPCommandPanel):
//   1. Inherit from this class (Reparent Blueprint)
//   2. Create UI layout with widgets using exact names below
//   3. All logic happens in C++ automatically
//
// Required Widget Names:
//   CORE: CommandInput, ResponseLog, SendButton, GetOperationsButton, GetSchemaButton
//   PROGRESS: ProgressBar, StatusLabel, PhaseLabel, ActorCountLabel, ElapsedTimeLabel, CancelButton
//   DYNAMIC BUTTONS: OperationButtonContainer (ScrollBox), SchemaButtonContainer (ScrollBox)
UCLASS(Blueprintable)
class UNREALMCP_API UMCPCommandPanelWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UMCPCommandPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// ========== BLUEPRINT-CALLABLE FUNCTIONS ==========

	// Send a command to the MCP server for execution
	// @param Command - The natural language command to execute (e.g., "create castle at 0,0,0")
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void SendCommand(const FString& Command);

	// Clear all text from the response log
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void ClearResponseLog();

	// Clear all dynamically created buttons (both operations and schema)
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void ClearAllButtons();

	// Append a line of text to the response log
	// @param Text - The text to append
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void AppendToResponseLog(const FString& Text);

	// Cancel the currently running async task
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void CancelCurrentTask();

	// Clear all dynamically created operation buttons
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void ClearOperationButtons();

	// Clear all dynamically created schema buttons
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void ClearSchemaButtons();

protected:
	// ========== CORE WIDGET BINDINGS ==========

	// Text input for typing commands - MUST be named "CommandInput"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UEditableTextBox> CommandInput;

	// Multi-line text box showing command history and responses - MUST be named "ResponseLog"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UMultiLineEditableTextBox> ResponseLog;

	// Button to send the current command - MUST be named "SendButton"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UButton> SendButton;

	// Button to request available operations from server - MUST be named "GetOperationsButton"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UButton> GetOperationsButton;

	// Button to request project schema from server - MUST be named "GetSchemaButton"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UButton> GetSchemaButton;

	// Button to clear all dynamically created buttons - MUST be named "ClearAllButton"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UMCPButtonWidget> ClearAllButton;

	// ========== PROGRESS WIDGET BINDINGS ==========

	// Visual progress indicator 0.0 to 1.0 - MUST be named "ProgressBar"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	// Text showing current task status - MUST be named "StatusLabel"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusLabel;

	// Text showing current build phase - MUST be named "PhaseLabel"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UTextBlock> PhaseLabel;

	// Text showing spawned actor count - MUST be named "ActorCountLabel"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UTextBlock> ActorCountLabel;

	// Text showing elapsed time in MM:SS format - MUST be named "ElapsedTimeLabel"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UTextBlock> ElapsedTimeLabel;

	// Button to cancel the current task - MUST be named "CancelButton"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	// ========== DYNAMIC BUTTON CONTAINER BINDINGS ==========

	// ScrollBox container for operation buttons - MUST be named "OperationButtonContainer"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UScrollBox> OperationButtonContainer;

	// ScrollBox container for schema buttons - MUST be named "SchemaButtonContainer"
	UPROPERTY(BlueprintReadOnly, Category = "MCP Widgets", meta = (BindWidget))
	TObjectPtr<UScrollBox> SchemaButtonContainer;

	// Blueprint class used to create dynamic buttons - set in Blueprint defaults or code
	UPROPERTY(EditDefaultsOnly, Category = "MCP Button Config")
	TSubclassOf<UMCPButtonWidget> ButtonWidgetClass;

private:
	// ========== SUBSYSTEM REFERENCE ==========

	// Reference to the MCP command panel subsystem for HTTP communication
	UPROPERTY()
	TObjectPtr<UMCPCommandPanelSubsystem> MCPSubsystem;

	// ========== STATIC BUTTON CLICK HANDLERS ==========

	// Called when SendButton is clicked
	UFUNCTION()
	void OnSendButtonClicked();

	// Called when GetOperationsButton is clicked
	UFUNCTION()
	void OnGetOperationsButtonClicked();

	// Called when GetSchemaButton is clicked
	UFUNCTION()
	void OnGetSchemaButtonClicked();

	// Called when CancelButton is clicked
	UFUNCTION()
	void OnCancelButtonClicked();

	// Called when Clear All button is clicked
	UFUNCTION()
	void OnClearAllButtonClicked(UMCPButtonWidget* ClickedButton);

	// ========== SUBSYSTEM DELEGATE HANDLERS ==========

	// Called when server responds to a command
	// @param Response - The response text from the server
	// @param bSuccess - Whether the command succeeded
	UFUNCTION()
	void HandleCommandResponse(const FString& Response, bool bSuccess);

	// Called when a command fails with an error
	// @param ErrorMessage - Description of the error
	UFUNCTION()
	void HandleCommandError(const FString& ErrorMessage);

	// Called when operations list is received from server
	// @param OperationsJson - JSON string containing available operations
	UFUNCTION()
	void HandleOperationsReceived(const FString& OperationsJson);

	// Called when project schema is received from server
	// @param SchemaJson - JSON string containing project schema
	UFUNCTION()
	void HandleSchemaReceived(const FString& SchemaJson);

	// Called periodically with task progress updates
	// @param TaskId - Unique identifier for the task
	// @param Status - Current status string (e.g., "running", "completed")
	// @param Progress - Progress value from 0.0 to 1.0
	// @param Message - Current phase message
	// @param SpawnedActors - Number of actors spawned so far
	UFUNCTION()
	void HandleTaskProgress(
		const FString& TaskId,
		const FString& Status,
		float Progress,
		const FString& Message,
		int32 SpawnedActors
	);

	// Called when a task completes (success or failure)
	// @param TaskId - Unique identifier for the task
	// @param bSuccess - Whether the task succeeded
	// @param ResultJson - JSON result or error message
	UFUNCTION()
	void HandleTaskCompleted(
		const FString& TaskId,
		bool bSuccess,
		const FString& ResultJson
	);

	// ========== DYNAMIC BUTTON CLICK HANDLERS ==========

	// Called when any dynamically created operation button is clicked
	// @param ClickedButton - The button widget that was clicked
	UFUNCTION()
	void OnDynamicOperationButtonClicked(UMCPButtonWidget* ClickedButton);

	// Called when any dynamically created schema button is clicked
	// @param ClickedButton - The button widget that was clicked
	UFUNCTION()
	void OnDynamicSchemaButtonClicked(UMCPButtonWidget* ClickedButton);

	// ========== COMMAND FORMATTING HANDLERS ==========
	// These are called by the dynamic button handlers to format commands

	// Format and load a schema button command into the input field
	// @param ClassName - The actor class name (e.g., "Spawner", "BaseAgent")
	void OnSchemaButtonClicked(const FString& ClassName);

	// Format and load an operation button command into the input field
	// @param OperationName - The operation name from the server
	void OnOperationButtonClicked(const FString& OperationName);

	// ========== HELPER FUNCTIONS ==========

	// Validate that all required widgets are properly bound
	// @return True if all required widgets exist
	bool ValidateWidgets() const;

	// Get reference to the MCP subsystem from the editor
	void AcquireSubsystemReference();

	// Bind click events for all static buttons
	void BindButtonEvents();

	// Bind all subsystem delegate handlers
	void BindSubsystemDelegates();

	// Unbind all delegates during destruction
	void UnbindAllDelegates();

	// Update all progress UI elements
	// @param Progress - Progress value 0.0 to 1.0
	// @param Status - Status text
	// @param Phase - Current phase description
	// @param Current - Current actor count
	// @param Total - Total expected actors
	void UpdateProgressUI(float Progress, const FString& Status, const FString& Phase, int32 Current, int32 Total);

	// Reset progress UI to idle state
	void ClearProgressUI();

	// Format seconds as MM:SS string
	// @param Seconds - Time in seconds
	// @return Formatted string like "02:30"
	FString FormatElapsedTime(float Seconds) const;

	// Parse operations JSON and extract operation names
	// @param OperationsJson - Raw JSON from server
	// @return Array of operation name strings
	TArray<FString> ParseOperationsJson(const FString& OperationsJson);

	// Parse schema JSON and extract schema items
	// @param SchemaJson - Raw JSON from server
	// @return Array of schema item strings
	TArray<FString> ParseSchemaJson(const FString& SchemaJson);

	// Create operation buttons from parsed operation names
	// @param OperationNames - List of operation names to create buttons for
	void PopulateOperationButtons(const TArray<FString>& OperationNames);

	// Create schema buttons from parsed schema items
	// @param SchemaItems - List of schema items to create buttons for
	void PopulateSchemaButtons(const TArray<FString>& SchemaItems);

	// Generate a user-friendly example command for an operation name
	// @param OperationName - The operation identifier
	// @return Example command string like "create medium castle at 0,0,0"
	FString GenerateExampleCommand(const FString& OperationName) const;

	// ========== INTERNAL STATE ==========

	// Task ID of the currently running task (empty if idle)
	FString CurrentTaskId;

	// Timestamp when current task started (for elapsed time calculation)
	double TaskStartTime;

	// Timer handle for periodic elapsed time updates
	FTimerHandle ElapsedTimeTimerHandle;

	// Called every 0.25 seconds to update elapsed time display
	void UpdateElapsedTimeDisplay();

	// Start the elapsed time update timer
	void StartElapsedTimeTimer();

	// Stop the elapsed time update timer
	void StopElapsedTimeTimer();

	// Array tracking dynamically created operation buttons for cleanup
	UPROPERTY()
	TArray<TObjectPtr<UMCPButtonWidget>> DynamicOperationButtons;

	// Array tracking dynamically created schema buttons for cleanup
	UPROPERTY()
	TArray<TObjectPtr<UMCPButtonWidget>> DynamicSchemaButtons;
};