#include "MCPCommandPanelWidget.h"
// Copyright Marcus Daley 2025. All Rights Reserved.
// Updated: 12/06/2025 - Fixed Phase 3 dynamic buttons without lambdas

#include "MCPCommandPanelSubsystem.h"
#include "MCPButtonWidget.h"
#include "Editor.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
UMCPCommandPanelWidget::UMCPCommandPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommandInput = nullptr;
	ResponseLog = nullptr;
	SendButton = nullptr;
	GetOperationsButton = nullptr;
	GetSchemaButton = nullptr;
	MCPSubsystem = nullptr;
	ProgressBar = nullptr;
	StatusLabel = nullptr;
	PhaseLabel = nullptr;
	ActorCountLabel = nullptr;
	ElapsedTimeLabel = nullptr;
	CancelButton = nullptr;
	OperationButtonContainer = nullptr;
	SchemaButtonContainer = nullptr;
	ButtonWidgetClass = nullptr;
	CurrentTaskId = TEXT("");
	TaskStartTime = 0.0;
}

void UMCPCommandPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Display, TEXT("MCPCommandPanelWidget: NativeConstruct called"));

	if (!ValidateWidgets())
	{
		UE_LOG(LogTemp, Error, TEXT("MCPCommandPanelWidget: Widget validation failed"));
		return;
	}

	AcquireSubsystemReference();

	if (!MCPSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPCommandPanelWidget: Failed to acquire MCP subsystem"));
		return;
	}

	BindButtonEvents();
	BindSubsystemDelegates();

	AppendToResponseLog(TEXT("======================================="));
	AppendToResponseLog(TEXT("  MCP Command Panel Ready (Async Mode)"));
	AppendToResponseLog(TEXT("======================================="));
	AppendToResponseLog(FString::Printf(TEXT(" Connected to: %s"), *MCPSubsystem->GetServerURL()));
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("Type a command and click Send to start an async task."));
	AppendToResponseLog(TEXT("Click 'Get Operations' to load server tools."));
	AppendToResponseLog(TEXT("Click 'Get Schema' to scan project Blueprints."));
	AppendToResponseLog(TEXT("Example: 'create large castle at 0,0,0'"));
	// Set Clear All button text
	if (ClearAllButton)
	{
		ClearAllButton->SetButtonText(FText::FromString(TEXT("Clear Buttons")));
	}

	ClearProgressUI();
	UE_LOG(LogTemp, Display, TEXT("MCPCommandPanelWidget: Initialization complete"));
}

void UMCPCommandPanelWidget::NativeDestruct()
{
	UE_LOG(LogTemp, Display, TEXT("MCPCommandPanelWidget: NativeDestruct called"));
	StopElapsedTimeTimer();
	ClearOperationButtons();
	ClearSchemaButtons();
	UnbindAllDelegates();

	MCPSubsystem = nullptr;
	CommandInput = nullptr;
	ResponseLog = nullptr;
	SendButton = nullptr;
	GetOperationsButton = nullptr;
	GetSchemaButton = nullptr;
	ProgressBar = nullptr;
	StatusLabel = nullptr;
	PhaseLabel = nullptr;
	ActorCountLabel = nullptr;
	ElapsedTimeLabel = nullptr;
	CancelButton = nullptr;
	OperationButtonContainer = nullptr;
	SchemaButtonContainer = nullptr;

	Super::NativeDestruct();
}

void UMCPCommandPanelWidget::SendCommand(const FString& Command)
{
	if (!MCPSubsystem)
	{
		AppendToResponseLog(TEXT("[ERROR] MCP Subsystem not available"));
		return;
	}
	if (Command.IsEmpty())
	{
		AppendToResponseLog(TEXT("[ERROR] Cannot send empty command"));
		return;
	}

	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(FString::Printf(TEXT("> SENDING: %s"), *Command));

	bool bSent = MCPSubsystem->SendCommand(Command);
	if (!bSent)
	{
		AppendToResponseLog(TEXT("[FAILED] Could not send command (check server connection)"));
		ClearProgressUI();
	}
	else
	{
		TaskStartTime = FPlatformTime::Seconds();
		AppendToResponseLog(TEXT("[OK] Task started - polling for progress..."));
		StartElapsedTimeTimer();
	}
}

void UMCPCommandPanelWidget::ClearResponseLog()
{
	if (ResponseLog)
	{
		ResponseLog->SetText(FText::GetEmpty());
	}
}

void UMCPCommandPanelWidget::ClearAllButtons()
{
	ClearOperationButtons();
	ClearSchemaButtons();
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("All dynamic buttons cleared."));

}

void UMCPCommandPanelWidget::AppendToResponseLog(const FString& Text)
{
	if (!ResponseLog) return;
	FString CurrentText = ResponseLog->GetText().ToString();
	FString NewText = CurrentText.IsEmpty() ? Text : CurrentText + TEXT("\n") + Text;
	ResponseLog->SetText(FText::FromString(NewText));
}

void UMCPCommandPanelWidget::CancelCurrentTask()
{
	if (CurrentTaskId.IsEmpty())
	{
		AppendToResponseLog(TEXT("[ERROR] No active task to cancel"));
		return;
	}
	if (!MCPSubsystem)
	{
		AppendToResponseLog(TEXT("[ERROR] MCP Subsystem not available"));
		return;
	}

	AppendToResponseLog(FString::Printf(TEXT("[CANCEL] Cancelling task: %s"), *CurrentTaskId));
	bool bSent = MCPSubsystem->CancelTask(CurrentTaskId);
	if (!bSent)
	{
		AppendToResponseLog(TEXT("[FAILED] Could not send cancel request"));
	}
	else
	{
		AppendToResponseLog(TEXT("[OK] Cancel request sent"));
		ClearProgressUI();
		CurrentTaskId = TEXT("");
	}
}

void UMCPCommandPanelWidget::ClearOperationButtons()
{
	if (!OperationButtonContainer)
	{
		return;
	}

	// Unbind delegates before removing
	for (UMCPButtonWidget* Button : DynamicOperationButtons)
	{
		if (Button)
		{
			Button->OnButtonClicked.RemoveDynamic(this, &UMCPCommandPanelWidget::OnDynamicOperationButtonClicked);
		}
	}

	// Clear the container
	OperationButtonContainer->ClearChildren();

	// Clear tracking array
	DynamicOperationButtons.Empty();

	UE_LOG(LogTemp, Display, TEXT("Operation buttons cleared"));
}

void UMCPCommandPanelWidget::ClearSchemaButtons()
{
	if (!SchemaButtonContainer)
	{
		return;
	}

	// Unbind delegates before removing
	for (UMCPButtonWidget* Button : DynamicSchemaButtons)
	{
		if (Button)
		{
			Button->OnButtonClicked.RemoveDynamic(this, &UMCPCommandPanelWidget::OnDynamicSchemaButtonClicked);
		}
	}

	// Clear the container
	SchemaButtonContainer->ClearChildren();

	// Clear tracking array
	DynamicSchemaButtons.Empty();

	UE_LOG(LogTemp, Display, TEXT("Schema buttons cleared"));
}

void UMCPCommandPanelWidget::OnSendButtonClicked()
{
	if (!CommandInput) return;
	FString Command = CommandInput->GetText().ToString().TrimStartAndEnd();
	SendCommand(Command);
	CommandInput->SetText(FText::GetEmpty());
}

void UMCPCommandPanelWidget::OnGetOperationsButtonClicked()
{
	if (!MCPSubsystem)
	{
		AppendToResponseLog(TEXT("[ERROR] MCP Subsystem not available"));
		return;
	}
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("> Requesting operations list..."));
	bool bSent = MCPSubsystem->RequestOperationsList();
	if (!bSent)
	{
		AppendToResponseLog(TEXT("[FAILED] Could not request operations list"));
	}
}



void UMCPCommandPanelWidget::OnGetSchemaButtonClicked()
{
	if (!MCPSubsystem)
	{
		AppendToResponseLog(TEXT("[ERROR] MCP Subsystem not available"));
		return;
	}
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("> Scanning project for Blueprint actors..."));

	// This now triggers LOCAL scan instead of HTTP request
	bool bSent = MCPSubsystem->RequestProjectSchema();
	if (!bSent)
	{
		AppendToResponseLog(TEXT("[FAILED] Could not scan project"));
	}
}

void UMCPCommandPanelWidget::OnCancelButtonClicked()
{
	CancelCurrentTask();
}

void UMCPCommandPanelWidget::HandleCommandResponse(const FString& Response, bool bSuccess)
{
	if (bSuccess)
	{
		AppendToResponseLog(FString::Printf(TEXT("[SUCCESS] %s"), *Response));
	}
	else
	{
		AppendToResponseLog(FString::Printf(TEXT("[FAILED] %s"), *Response));
	}
}

void UMCPCommandPanelWidget::HandleCommandError(const FString& ErrorMessage)
{
	AppendToResponseLog(FString::Printf(TEXT("[ERROR] %s"), *ErrorMessage));
	ClearProgressUI();
}

void UMCPCommandPanelWidget::HandleOperationsReceived(const FString& OperationsJson)
{
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("=== OPERATIONS RECEIVED ==="));
	TArray<FString> OperationNames = ParseOperationsJson(OperationsJson);
	if (OperationNames.Num() > 0)
	{
		AppendToResponseLog(FString::Printf(TEXT("Found %d operations"), OperationNames.Num()));
		PopulateOperationButtons(OperationNames);
	}
	else
	{
		AppendToResponseLog(TEXT("No operations found in response"));
		AppendToResponseLog(OperationsJson);
	}
}

void UMCPCommandPanelWidget::HandleSchemaReceived(const FString& SchemaJson)
{
	if (!ButtonWidgetClass || !SchemaButtonContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("Schema: Missing ButtonWidgetClass or Container"));
		return;
	}

	// Clear existing buttons
	ClearSchemaButtons();

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(SchemaJson);

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse schema JSON"));
		AppendToResponseLog(TEXT("[ERROR] Invalid schema JSON"));
		return;
	}

	// Get project name
	FString ProjectName = JsonObject->HasField(TEXT("project"))
		? JsonObject->GetStringField(TEXT("project"))
		: TEXT("Unknown");

	AppendToResponseLog(FString::Printf(TEXT("Project: %s"), *ProjectName));

	// Extract schema object
	const TSharedPtr<FJsonObject>* SchemaObject;
	if (!JsonObject->TryGetObjectField(TEXT("schema"), SchemaObject))
	{
		UE_LOG(LogTemp, Error, TEXT("No schema object in JSON"));
		return;
	}

	// Get systems object (contains categories like "Spawning", "Pickups", etc.)
	const TSharedPtr<FJsonObject>* SystemsObject;
	if ((*SchemaObject)->TryGetObjectField(TEXT("systems"), SystemsObject))
	{
		int32 TotalButtons = 0;

		// Iterate through each category
		for (const auto& SystemPair : (*SystemsObject)->Values)
		{
			FString Category = SystemPair.Key;
			const TArray<TSharedPtr<FJsonValue>>* ClassArray;

			if (SystemPair.Value->TryGetArray(ClassArray))
			{
				// Log the category
				AppendToResponseLog(FString::Printf(TEXT("  [%s] %d actors"), *Category, ClassArray->Num()));

				// Create button for each Blueprint in this category
				for (const TSharedPtr<FJsonValue>& ClassValue : *ClassArray)
				{
					FString BlueprintName = ClassValue->AsString();

					// Create button widget
					UMCPButtonWidget* NewButton = CreateWidget<UMCPButtonWidget>(this, ButtonWidgetClass);
					if (!NewButton) continue;

					// Set button text: "Category: BlueprintName"
					FString ButtonLabel = FString::Printf(TEXT("%s: %s"), *Category, *BlueprintName);
					NewButton->SetButtonText(FText::FromString(ButtonLabel));

					// Store the Blueprint name for spawn command
					// The spawn command will use a generic format that Python can interpret
					NewButton->AssociatedCommand = BlueprintName;

					// Bind click event
					NewButton->OnButtonClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnDynamicSchemaButtonClicked);

					// Add to container with proper layout
					UPanelSlot* PanelSlot = SchemaButtonContainer->AddChild(NewButton);
					if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
					{
						ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
						ScrollSlot->SetVerticalAlignment(VAlign_Top);
						ScrollSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f));
					}

					DynamicSchemaButtons.Add(NewButton);
					TotalButtons++;

					UE_LOG(LogTemp, Display, TEXT("Created button: %s (Blueprint: %s)"), *ButtonLabel, *BlueprintName);
				}
			}
		}

		AppendToResponseLog(FString::Printf(TEXT("Loaded %d Blueprint actors from project"), TotalButtons));
	}

	// Force UI refresh
	SchemaButtonContainer->InvalidateLayoutAndVolatility();
}
void UMCPCommandPanelWidget::HandleTaskProgress(const FString& TaskId, const FString& Status, float Progress, const FString& Message, int32 SpawnedActors)
{
	CurrentTaskId = TaskId;
	float ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - TaskStartTime);
	FString Phase = Message.IsEmpty() ? Status : Message;

	int32 TotalActors = 0;
	FMCPActiveTask TaskInfo;
	if (MCPSubsystem && MCPSubsystem->GetTaskInfo(TaskId, TaskInfo))
	{
		TotalActors = TaskInfo.TotalActors;
	}

	UpdateProgressUI(Progress, Status, Phase, SpawnedActors, TotalActors);
	if (ElapsedTimeLabel)
	{
		ElapsedTimeLabel->SetText(FText::FromString(FormatElapsedTime(ElapsedSeconds)));
	}
}

void UMCPCommandPanelWidget::HandleTaskCompleted(const FString& TaskId, bool bSuccess, const FString& ResultJson)
{
	float ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - TaskStartTime);
	StopElapsedTimeTimer();

	if (bSuccess)
	{
		int32 FinalActorCount = 0;
		FMCPActiveTask TaskInfo;
		if (MCPSubsystem && MCPSubsystem->GetTaskInfo(TaskId, TaskInfo))
		{
			FinalActorCount = TaskInfo.SpawnedActors;
		}
		UpdateProgressUI(1.0f, TEXT("completed"), TEXT("Complete!"), FinalActorCount, FinalActorCount);
		AppendToResponseLog(TEXT(""));
		AppendToResponseLog(FString::Printf(TEXT("[COMPLETED] Task finished in %s"), *FormatElapsedTime(ElapsedSeconds)));
		AppendToResponseLog(TEXT("---------------------------------------"));
	}
	else
	{
		if (StatusLabel)
		{
			StatusLabel->SetText(FText::FromString(TEXT("Status: FAILED")));
		}
		AppendToResponseLog(TEXT(""));
		AppendToResponseLog(FString::Printf(TEXT("[FAILED] Task error: %s"), *ResultJson));
		AppendToResponseLog(TEXT("---------------------------------------"));
	}
	CurrentTaskId = TEXT("");
}

void UMCPCommandPanelWidget::OnDynamicOperationButtonClicked(UMCPButtonWidget* ClickedButton)
{
	if (!ClickedButton)
	{
		UE_LOG(LogTemp, Error, TEXT("OnDynamicOperationButtonClicked: Null button"));
		return;
	}

	// Extract the operation name from the button's stored command
	FString OperationName = ClickedButton->AssociatedCommand;

	if (OperationName.IsEmpty())
	{
		// Fallback: use button text if command is empty
		OperationName = ClickedButton->ButtonText.ToString();
	}

	UE_LOG(LogTemp, Display, TEXT("Dynamic operation button clicked: %s"), *OperationName);

	// Call the command formatter
	OnOperationButtonClicked(OperationName);
}

void UMCPCommandPanelWidget::OnDynamicSchemaButtonClicked(UMCPButtonWidget* ClickedButton)
{
	if (!ClickedButton)
	{
		UE_LOG(LogTemp, Error, TEXT("OnDynamicSchemaButtonClicked: Null button"));
		return;
	}

	// Extract the class name from the button's stored command
	FString ClassName = ClickedButton->AssociatedCommand;

	if (ClassName.IsEmpty())
	{
		// Fallback: use button text if command is empty
		ClassName = ClickedButton->ButtonText.ToString();
	}

	UE_LOG(LogTemp, Display, TEXT("Dynamic schema button clicked: %s"), *ClassName);

	// Call the command formatter
	OnSchemaButtonClicked(ClassName);
}
void UMCPCommandPanelWidget::OnSchemaButtonClicked(const FString& ClassName)
{
	UE_LOG(LogTemp, Display, TEXT("=== Schema Button Command Formatter ==="));
	UE_LOG(LogTemp, Display, TEXT("  Class Name: %s"), *ClassName);

	// Generate proper MCP tool command
	
	FString Command = FString::Printf(
		TEXT("spawn_blueprint blueprint_name=\"%s\" location=[0,0,50]"),
		*ClassName
	);

	// Fill the command input field
	if (CommandInput)
	{
		CommandInput->SetText(FText::FromString(Command));
		UE_LOG(LogTemp, Display, TEXT("  Command loaded: %s"), *Command);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  CommandInput is null!"));
		return;
	}

	// Show user-friendly message in log
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("╔══════════════════════════════════════════╗"));
	AppendToResponseLog(FString::Printf(TEXT("║  Schema: %s"), *ClassName.LeftPad(32)));
	AppendToResponseLog(TEXT("╚══════════════════════════════════════════╝"));
	AppendToResponseLog(FString::Printf(TEXT("Command: %s"), *Command));
	AppendToResponseLog(TEXT("Click 'Send' to spawn, or modify count/location first."));
	AppendToResponseLog(TEXT("Examples:"));
	AppendToResponseLog(TEXT("  - Change count=5 to spawn 5 actors"));
	AppendToResponseLog(TEXT("  - Change location=[500,0,50] for different position"));
	AppendToResponseLog(TEXT(""));

	// Auto-focus the command input so user can edit if needed
	if (CommandInput)
	{
		CommandInput->SetKeyboardFocus();
	}
}
void UMCPCommandPanelWidget::OnOperationButtonClicked(const FString& OperationName)
{
	UE_LOG(LogTemp, Display, TEXT("=== Operation Button Command Formatter ==="));
	UE_LOG(LogTemp, Display, TEXT("  Operation: %s"), *OperationName);

	// For operations, generate an example command
	FString Command = GenerateExampleCommand(OperationName);

	if (CommandInput)
	{
		CommandInput->SetText(FText::FromString(Command));
		UE_LOG(LogTemp, Display, TEXT("  Command loaded: %s"), *Command);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  CommandInput is null!"));
		return;
	}

	// Show user-friendly message
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(FString::Printf(TEXT("[Operation] %s"), *OperationName));
	AppendToResponseLog(FString::Printf(TEXT("Example: %s"), *Command));
	AppendToResponseLog(TEXT("Modify parameters as needed, then click 'Send'."));
	AppendToResponseLog(TEXT(""));

	// Auto-focus
	if (CommandInput)
	{
		CommandInput->SetKeyboardFocus();
	}
}

bool UMCPCommandPanelWidget::ValidateWidgets() const
{
	bool bValid = true;
	if (!CommandInput) { UE_LOG(LogTemp, Error, TEXT("CommandInput is null")); bValid = false; }
	if (!ResponseLog) { UE_LOG(LogTemp, Error, TEXT("ResponseLog is null")); bValid = false; }
	if (!SendButton) { UE_LOG(LogTemp, Error, TEXT("SendButton is null")); bValid = false; }
	if (!GetOperationsButton) { UE_LOG(LogTemp, Error, TEXT("GetOperationsButton is null")); bValid = false; }
	if (!GetSchemaButton) { UE_LOG(LogTemp, Error, TEXT("GetSchemaButton is null")); bValid = false; }
	if (!ProgressBar) { UE_LOG(LogTemp, Error, TEXT("ProgressBar is null")); bValid = false; }
	if (!StatusLabel) { UE_LOG(LogTemp, Error, TEXT("StatusLabel is null")); bValid = false; }
	if (!PhaseLabel) { UE_LOG(LogTemp, Error, TEXT("PhaseLabel is null")); bValid = false; }
	if (!ActorCountLabel) { UE_LOG(LogTemp, Error, TEXT("ActorCountLabel is null")); bValid = false; }
	if (!ElapsedTimeLabel) { UE_LOG(LogTemp, Error, TEXT("ElapsedTimeLabel is null")); bValid = false; }
	if (!ClearAllButton) { UE_LOG(LogTemp, Error, TEXT("ClearAllButton is null")); bValid = false; }
	if (!CancelButton) { UE_LOG(LogTemp, Error, TEXT("CancelButton is null")); bValid = false; }
	if (!OperationButtonContainer) { UE_LOG(LogTemp, Warning, TEXT("OperationButtonContainer is null")); }
	if (!SchemaButtonContainer) { UE_LOG(LogTemp, Warning, TEXT("SchemaButtonContainer is null")); }
	return bValid;
}

void UMCPCommandPanelWidget::AcquireSubsystemReference()
{
	if (GEditor)
	{
		MCPSubsystem = GEditor->GetEditorSubsystem<UMCPCommandPanelSubsystem>();
		if (MCPSubsystem)
		{
			UE_LOG(LogTemp, Display, TEXT("MCPCommandPanelWidget: Acquired MCP subsystem"));
		}
	}
}

void UMCPCommandPanelWidget::BindButtonEvents()
{
	// Bind static buttons
	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnSendButtonClicked);
		UE_LOG(LogTemp, Display, TEXT("Send button bound"));
	}

	if (GetOperationsButton)
	{
		GetOperationsButton->OnClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnGetOperationsButtonClicked);
		UE_LOG(LogTemp, Display, TEXT("Get Operations button bound"));
	}

	if (GetSchemaButton)
	{
		GetSchemaButton->OnClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnGetSchemaButtonClicked);
		UE_LOG(LogTemp, Display, TEXT("Get Schema button bound"));
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnCancelButtonClicked);
		UE_LOG(LogTemp, Display, TEXT("Cancel button bound"));
	}

	// Bind Clear All button (it's a MCPButtonWidget, not a regular Button)
	if (ClearAllButton)
	{
		ClearAllButton->OnButtonClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnClearAllButtonClicked);
		ClearAllButton->SetButtonText(FText::FromString(TEXT("Clear All")));
		UE_LOG(LogTemp, Display, TEXT("Clear All button bound"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearAllButton not found - clear functionality unavailable"));
	}
}

void UMCPCommandPanelWidget::BindSubsystemDelegates()
{
	if (!MCPSubsystem) return;
	MCPSubsystem->OnCommandResponse.AddDynamic(this, &UMCPCommandPanelWidget::HandleCommandResponse);
	MCPSubsystem->OnCommandError.AddDynamic(this, &UMCPCommandPanelWidget::HandleCommandError);
	MCPSubsystem->OnOperationsReceived.AddDynamic(this, &UMCPCommandPanelWidget::HandleOperationsReceived);
	MCPSubsystem->OnSchemaReceived.AddDynamic(this, &UMCPCommandPanelWidget::HandleSchemaReceived);
	MCPSubsystem->OnTaskProgress.AddDynamic(this, &UMCPCommandPanelWidget::HandleTaskProgress);
	MCPSubsystem->OnTaskCompleted.AddDynamic(this, &UMCPCommandPanelWidget::HandleTaskCompleted);
	UE_LOG(LogTemp, Display, TEXT("MCPCommandPanelWidget: All delegates bound"));
}

void UMCPCommandPanelWidget::UnbindAllDelegates()
{
	if (SendButton) SendButton->OnClicked.RemoveAll(this);
	if (GetOperationsButton) GetOperationsButton->OnClicked.RemoveAll(this);
	if (GetSchemaButton) GetSchemaButton->OnClicked.RemoveAll(this);
	if (CancelButton) CancelButton->OnClicked.RemoveAll(this);
	if (MCPSubsystem && IsValid(MCPSubsystem))
	{
		MCPSubsystem->OnCommandResponse.RemoveAll(this);
		MCPSubsystem->OnCommandError.RemoveAll(this);
		MCPSubsystem->OnOperationsReceived.RemoveAll(this);
		MCPSubsystem->OnSchemaReceived.RemoveAll(this);
		MCPSubsystem->OnTaskProgress.RemoveAll(this);
		MCPSubsystem->OnTaskCompleted.RemoveAll(this);
	}
}

void UMCPCommandPanelWidget::UpdateProgressUI(float Progress, const FString& Status, const FString& Phase, int32 Current, int32 Total)
{
	if (ProgressBar) ProgressBar->SetPercent(FMath::Min(Progress, 1.0f));
	if (StatusLabel) StatusLabel->SetText(FText::FromString(FString::Printf(TEXT("Status: %s"), *Status)));
	if (PhaseLabel) PhaseLabel->SetText(FText::FromString(FString::Printf(TEXT("Phase: %s"), *Phase)));
	if (ActorCountLabel)
	{
		if (Total > 0)
		{
			ActorCountLabel->SetText(FText::FromString(FString::Printf(TEXT("Actors: %d / %d (%.0f%%)"), Current, Total, (float)Current / (float)Total * 100.0f)));
		}
		else if (Current > 0)
		{
			ActorCountLabel->SetText(FText::FromString(FString::Printf(TEXT("Actors: %d"), Current)));
		}
		else
		{
			ActorCountLabel->SetText(FText::FromString(TEXT("Actors: --")));
		}
	}
}

void UMCPCommandPanelWidget::ClearProgressUI()
{
	StopElapsedTimeTimer();
	if (ProgressBar) ProgressBar->SetPercent(0.0f);
	if (StatusLabel) StatusLabel->SetText(FText::FromString(TEXT("Status: Idle")));
	if (PhaseLabel) PhaseLabel->SetText(FText::FromString(TEXT("Phase: --")));
	if (ActorCountLabel) ActorCountLabel->SetText(FText::FromString(TEXT("Actors: --")));
	if (ElapsedTimeLabel) ElapsedTimeLabel->SetText(FText::FromString(TEXT("00:00")));
}

FString UMCPCommandPanelWidget::FormatElapsedTime(float Seconds) const
{
	int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
	int32 Secs = FMath::FloorToInt(Seconds) % 60;
	return FString::Printf(TEXT("%02d:%02d"), Minutes, Secs);
}

TArray<FString> UMCPCommandPanelWidget::ParseOperationsJson(const FString& OperationsJson)
{
	TArray<FString> OperationNames;
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OperationsJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return OperationNames;
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationsArray;
	if (JsonObject->TryGetArrayField(TEXT("operations"), OperationsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *OperationsArray)
		{
			if (Value->Type == EJson::Object)
			{
				FString OpName;
				if (Value->AsObject()->TryGetStringField(TEXT("name"), OpName))
				{
					OperationNames.Add(OpName);
				}
			}
			else if (Value->Type == EJson::String)
			{
				OperationNames.Add(Value->AsString());
			}
		}
	}
	else if (JsonObject->TryGetArrayField(TEXT("tools"), OperationsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *OperationsArray)
		{
			if (Value->Type == EJson::Object)
			{
				FString OpName;
				if (Value->AsObject()->TryGetStringField(TEXT("name"), OpName))
				{
					OperationNames.Add(OpName);
				}
			}
		}
	}
	return OperationNames;
}

TArray<FString> UMCPCommandPanelWidget::ParseSchemaJson(const FString& SchemaJson)
{
	TArray<FString> SchemaItems;
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemaJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return SchemaItems;
	}
	for (const auto& Pair : JsonObject->Values)
	{
		SchemaItems.Add(Pair.Key);
	}
	return SchemaItems;
}

void UMCPCommandPanelWidget::PopulateOperationButtons(const TArray<FString>& OperationNames)
{
	UE_LOG(LogTemp, Display, TEXT("=== Populating Operation Buttons ==="));
	UE_LOG(LogTemp, Display, TEXT("  Operation count: %d"), OperationNames.Num());

	if (!OperationButtonContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("  OperationButtonContainer is NULL!"));
		return;
	}

	if (!ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("  ButtonWidgetClass is NULL!"));
		return;
	}

	// Clear existing operation buttons
	ClearOperationButtons();

	// Create button for each operation
	int32 SuccessCount = 0;
	for (const FString& OperationName : OperationNames)
	{
		UMCPButtonWidget* NewButton = CreateWidget<UMCPButtonWidget>(this, ButtonWidgetClass);

		if (NewButton)
		{
			// Set button text to operation name
			NewButton->SetButtonText(FText::FromString(OperationName));

			// Store operation name in button
			NewButton->AssociatedCommand = OperationName;

			// Bind click event - will call OnDynamicOperationButtonClicked
			NewButton->OnButtonClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnDynamicOperationButtonClicked);

			// Add to container
			OperationButtonContainer->AddChild(NewButton);

			// Track for cleanup
			DynamicOperationButtons.Add(NewButton);

			UE_LOG(LogTemp, Display, TEXT("  Created: %s"), *OperationName);
			SuccessCount++;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  Failed to create button: %s"), *OperationName);
		}
	}

	// Show success message
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(FString::Printf(TEXT("Loaded %d operations from server."), SuccessCount));
	AppendToResponseLog(TEXT("Click any button to load example command."));
	AppendToResponseLog(TEXT(""));

	UE_LOG(LogTemp, Display, TEXT("=== Operation Buttons Complete: %d/%d ==="), SuccessCount, OperationNames.Num());
}

void UMCPCommandPanelWidget::PopulateSchemaButtons(const TArray<FString>& SchemaItems)
{
	UE_LOG(LogTemp, Display, TEXT("=== Populating Schema Buttons ==="));
	UE_LOG(LogTemp, Display, TEXT("  Schema item count: %d"), SchemaItems.Num());

	if (!SchemaButtonContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("  SchemaButtonContainer is NULL!"));
		AppendToResponseLog(TEXT("[ERROR] Schema button container not found!"));
		return;
	}

	if (!ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("  ButtonWidgetClass is NULL!"));
		AppendToResponseLog(TEXT("[ERROR] Button widget class not set in Blueprint!"));
		return;
	}

	// Clear existing schema buttons first
	ClearSchemaButtons();

	// Create button for each schema item
	int32 SuccessCount = 0;
	for (const FString& SchemaItem : SchemaItems)
	{
		// Parse the schema item format
		// Expected format from server: "Category: BlueprintName" or "BlueprintName" or "Category: BlueprintName -> spawn blueprint..."

		FString DisplayText = SchemaItem;
		FString BlueprintName;

		// Check if item contains " -> " (indicates it has command text)
		int32 ArrowIndex;
		if (SchemaItem.FindChar('>', ArrowIndex) && ArrowIndex > 0)
		{
			// Format: "Category: Name -> command..."
			// Extract just the "Category: Name" part before the arrow
			DisplayText = SchemaItem.Left(ArrowIndex - 2).TrimEnd(); // -2 to remove " -"
		}

		// Now extract just the Blueprint name (after the category prefix if present)
		int32 ColonIndex;
		if (DisplayText.FindChar(':', ColonIndex))
		{
			// Format: "Category: BlueprintName"
			BlueprintName = DisplayText.Mid(ColonIndex + 1).TrimStart(); // Get text after ": "
		}
		else
		{
			// Format: "BlueprintName" (no category)
			BlueprintName = DisplayText.TrimStartAndEnd();
		}

		// Validate we extracted a valid Blueprint name
		if (BlueprintName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Skipping invalid schema item: %s"), *SchemaItem);
			continue;
		}

		// Create the button
		UMCPButtonWidget* NewButton = CreateWidget<UMCPButtonWidget>(this, ButtonWidgetClass);

		if (NewButton)
		{
			// Set button display text (with category for user-friendly display)
			NewButton->SetButtonText(FText::FromString(DisplayText));

			// CRITICAL FIX: Store ONLY the Blueprint name, not the full command!
			NewButton->AssociatedCommand = BlueprintName;

			// Bind click event
			NewButton->OnButtonClicked.AddDynamic(this, &UMCPCommandPanelWidget::OnDynamicSchemaButtonClicked);

			// Add to container
			SchemaButtonContainer->AddChild(NewButton);

			// Track for cleanup
			DynamicSchemaButtons.Add(NewButton);

			UE_LOG(LogTemp, Display, TEXT("  Created: Display='%s', BlueprintName='%s'"),
				*DisplayText, *BlueprintName);
			SuccessCount++;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  Failed to create button for: %s"), *SchemaItem);
		}
	}

	// Show success message
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(FString::Printf(TEXT(" Loaded %d actor types from project."), SuccessCount));
	AppendToResponseLog(TEXT("Click any button to load spawn command."));
	AppendToResponseLog(TEXT(""));

	UE_LOG(LogTemp, Display, TEXT("=== Schema Buttons Complete: %d/%d ==="), SuccessCount, SchemaItems.Num());
}


FString UMCPCommandPanelWidget::GenerateExampleCommand(const FString& OperationName) const
{
	if (OperationName.Contains(TEXT("castle"))) return TEXT("create medium castle at 0,0,0");
	if (OperationName.Contains(TEXT("mansion"))) return TEXT("create large mansion at 5000,0,0");
	if (OperationName.Contains(TEXT("town"))) return TEXT("create small town at 0,5000,0");
	if (OperationName.Contains(TEXT("house"))) return TEXT("create modern house at 1000,0,0");
	if (OperationName.Contains(TEXT("tower"))) return TEXT("create tower at 2000,0,0");
	if (OperationName.Contains(TEXT("bridge"))) return TEXT("create bridge at 0,-3000,0");
	if (OperationName.Contains(TEXT("spawner"))) return TEXT("spawn spawner faction 0 at 0,0,50");
	if (OperationName.Contains(TEXT("agent"))) return TEXT("spawn agent faction 1 at 500,0,50");
	if (OperationName.Contains(TEXT("pyramid"))) return TEXT("create pyramid at 0,0,0");
	return OperationName;
}

void UMCPCommandPanelWidget::StartElapsedTimeTimer()
{
	UWorld* World = GetWorld();
	if (!World) return;
	StopElapsedTimeTimer();
	World->GetTimerManager().SetTimer(ElapsedTimeTimerHandle, this, &UMCPCommandPanelWidget::UpdateElapsedTimeDisplay, 0.25f, true);
}

void UMCPCommandPanelWidget::StopElapsedTimeTimer()
{
	UWorld* World = GetWorld();
	if (World && ElapsedTimeTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(ElapsedTimeTimerHandle);
		ElapsedTimeTimerHandle.Invalidate();
	}
}

void UMCPCommandPanelWidget::OnClearAllButtonClicked(UMCPButtonWidget* ClickedButton)
{
	UE_LOG(LogTemp, Display, TEXT("=== Clear All Button Clicked ==="));

	// Clear both button containers
	ClearOperationButtons();
	ClearSchemaButtons();

	// Clear text fields
	if (ResponseLog)
	{
		ResponseLog->SetText(FText::FromString(TEXT("")));
		UE_LOG(LogTemp, Display, TEXT("  Response log cleared"));
	}

	if (CommandInput)
	{
		CommandInput->SetText(FText::FromString(TEXT("")));
		UE_LOG(LogTemp, Display, TEXT("  Command input cleared"));
	}

	// Reset progress UI
	ClearProgressUI();

	// Show reset message
	AppendToResponseLog(TEXT("╔════════════════════════════════════════════╗"));
	AppendToResponseLog(TEXT("║  All Buttons and Logs Cleared              ║"));
	AppendToResponseLog(TEXT("╚════════════════════════════════════════════╝"));
	AppendToResponseLog(TEXT(""));
	AppendToResponseLog(TEXT("Click 'Get Operations' to reload operation buttons."));
	AppendToResponseLog(TEXT("Click 'Get Schema' to reload actor type buttons."));
	AppendToResponseLog(TEXT(""));

	UE_LOG(LogTemp, Display, TEXT("  Clear All complete"));
}


void UMCPCommandPanelWidget::UpdateElapsedTimeDisplay()
{
	if (CurrentTaskId.IsEmpty() || !ElapsedTimeLabel) return;
	float ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - TaskStartTime);
	ElapsedTimeLabel->SetText(FText::FromString(FormatElapsedTime(ElapsedSeconds)));
}