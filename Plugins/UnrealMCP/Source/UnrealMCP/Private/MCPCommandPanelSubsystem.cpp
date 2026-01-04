// Copyright (c) 2025 Marcus Daley. All Rights Reserved.
// Island Escape - Game Architecture Capstone (GAR2510/END2507)
// File: MCPCommandPanelSubsystem.cpp
// Purpose: Implementation of MCP UI communication with async task polling
// Updated: Added local Blueprint scanning - no hardcoded paths!

#include "MCPCommandPanelSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Asset Registry for Blueprint scanning
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

#define DEFAULT_MCP_SERVER_URL "http://127.0.0.1:8000"

void UMCPCommandPanelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ServerURL = DEFAULT_MCP_SERVER_URL;
	HttpModule = &FHttpModule::Get();

	// Set default scan folders
	ScanFolders.Add(TEXT("/Game/"));

	// Set default parent classes to look for
	ScanParentClasses.Add(TEXT("Actor"));
	ScanParentClasses.Add(TEXT("Pawn"));
	ScanParentClasses.Add(TEXT("Character"));
	ScanParentClasses.Add(TEXT("GameModeBase"));
	ScanParentClasses.Add(TEXT("PlayerController"));
	ScanParentClasses.Add(TEXT("AIController"));

	// Initialize debug log file
	InitDebugLog();

	LogMessage(TEXT("MCPCommandPanelSubsystem initialized with async task polling"));
	LogMessage(FString::Printf(TEXT("Server URL: %s"), *ServerURL));
	LogMessage(FString::Printf(TEXT("Polling interval: %.1f seconds"), PollingInterval));
	LogMessage(FString::Printf(TEXT("Project: %s"), *GetCurrentProjectName()));

	WriteDebugLog(TEXT("INIT: Subsystem initialized"));
	WriteDebugLog(FString::Printf(TEXT("INIT: Server URL = %s"), *ServerURL));
	WriteDebugLog(FString::Printf(TEXT("INIT: Project = %s"), *GetCurrentProjectName()));
}

void UMCPCommandPanelSubsystem::Deinitialize()
{
	// Cancel all active tasks before shutdown
	TArray<FString> TaskIds;
	ActiveTasks.GetKeys(TaskIds);
	for (const FString& TaskId : TaskIds)
	{
		StopPollingTask(TaskId);
	}

	CloseDebugLog();
	LogMessage(TEXT("MCPCommandPanelSubsystem shutting down"));
	Super::Deinitialize();
}

void UMCPCommandPanelSubsystem::Tick(float DeltaTime)
{
	// Debug tick counter
	static int32 DebugTickCount = 0;
	DebugTickCount++;
	if (DebugTickCount % 90 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MCPSubsystem TICK #%d: Active=%d, Tasks=%d, TimeSincePoll=%.2f"),
			DebugTickCount, bHasActiveTasks ? 1 : 0, ActiveTasks.Num(), TimeSinceLastPoll);
	}

	if (!bHasActiveTasks)
	{
		return;
	}

	TimeSinceLastPoll += DeltaTime;

	// Only poll at specified interval
	if (TimeSinceLastPoll < PollingInterval)
	{
		return;
	}

	TimeSinceLastPoll = 0.0f;

	// Poll all active tasks
	TArray<FString> TaskIds;
	ActiveTasks.GetKeys(TaskIds);

	WriteDebugLog(FString::Printf(TEXT("TICK: Polling %d active tasks"), TaskIds.Num()));

	for (const FString& TaskId : TaskIds)
	{
		PollTaskStatus(TaskId);
	}
}

bool UMCPCommandPanelSubsystem::IsTickableInEditor() const
{
	return true;
}

bool UMCPCommandPanelSubsystem::IsTickableWhenPaused() const
{
	return true;
}

// ========== LOCAL BLUEPRINT SCANNING ==========

FString UMCPCommandPanelSubsystem::GetCurrentProjectName() const
{
	return FApp::GetProjectName();
}

void UMCPCommandPanelSubsystem::ScanProjectBlueprints()
{
	LogMessage(FString::Printf(TEXT("Scanning project '%s' for Blueprint actors..."), *GetCurrentProjectName()));
	WriteDebugLog(FString::Printf(TEXT("SCAN: Starting Blueprint scan for project '%s'"), *GetCurrentProjectName()));

	PerformBlueprintScan();

	// Convert to JSON and broadcast (for Widget compatibility)
	FString SchemaJson = ConvertDiscoveredBlueprintsToJson();
	OnSchemaReceived.Broadcast(SchemaJson);

	LogMessage(FString::Printf(TEXT("Scan complete: Found %d Blueprint actors"), DiscoveredBlueprints.Num()));
	WriteDebugLog(FString::Printf(TEXT("SCAN: Complete - Found %d Blueprints"), DiscoveredBlueprints.Num()));
}

void UMCPCommandPanelSubsystem::PerformBlueprintScan()
{
	DiscoveredBlueprints.Empty();
	bHasScannedProject = false;

	// Get the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Build filter for Blueprint assets
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	// Add all scan folders
	for (const FString& Folder : ScanFolders)
	{
		Filter.PackagePaths.Add(FName(*Folder));
	}

	// Get all matching assets
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	LogMessage(FString::Printf(TEXT("Asset Registry returned %d Blueprint assets"), AssetDataList.Num()));

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Get the Blueprint's parent class
		FString ParentClassName;
		FAssetTagValueRef ParentClassTag = AssetData.TagsAndValues.FindTag(FBlueprintTags::ParentClassPath);

		if (ParentClassTag.IsSet())
		{
			FString ParentClassPath = ParentClassTag.AsString();
			// Extract just the class name from the path
			ParentClassName = FPackageName::ObjectPathToObjectName(ParentClassPath);
			// Clean up the name (remove _C suffix if present)
			ParentClassName.RemoveFromEnd(TEXT("_C"));
		}

		// Check if this Blueprint inherits from one of our target classes
		bool bIsTargetClass = false;
		for (const FString& TargetClass : ScanParentClasses)
		{
			if (ParentClassName.Contains(TargetClass))
			{
				bIsTargetClass = true;
				break;
			}
		}

		// If no parent class found or it's not a target, try to load and check
		if (!bIsTargetClass && ParentClassName.IsEmpty())
		{
			// Load the Blueprint to check its parent class
			if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				if (BP->ParentClass)
				{
					ParentClassName = BP->ParentClass->GetName();

					// Check if inherits from Actor (catches most gameplay classes)
					if (BP->ParentClass->IsChildOf(AActor::StaticClass()))
					{
						bIsTargetClass = true;
					}
				}
			}
		}

		// Skip if not a target class type
		if (!bIsTargetClass)
		{
			continue;
		}

		// Create the discovered Blueprint entry
		FMCPDiscoveredBlueprint DiscoveredBP;
		DiscoveredBP.DisplayName = AssetData.AssetName.ToString();
		DiscoveredBP.AssetPath = AssetData.PackageName.ToString();
		DiscoveredBP.ParentClassName = ParentClassName;
		DiscoveredBP.Category = CategorizeBlueprint(DiscoveredBP.AssetPath, ParentClassName);

		DiscoveredBlueprints.Add(DiscoveredBP);

		UE_LOG(LogTemp, Display, TEXT("Found Blueprint: %s [%s] -> %s"),
			*DiscoveredBP.DisplayName, *DiscoveredBP.Category, *DiscoveredBP.AssetPath);
	}

	bHasScannedProject = true;
}

FString UMCPCommandPanelSubsystem::CategorizeBlueprint(const FString& AssetPath, const FString& ParentClass) const
{
	// First try to categorize by folder path
	FString LowerPath = AssetPath.ToLower();

	// Check folder-based categories
	if (LowerPath.Contains(TEXT("spawner"))) return TEXT("Spawning");
	if (LowerPath.Contains(TEXT("pickup")) || LowerPath.Contains(TEXT("collectible"))) return TEXT("Pickups");
	if (LowerPath.Contains(TEXT("weapon")) || LowerPath.Contains(TEXT("rifle")) || LowerPath.Contains(TEXT("projectile"))) return TEXT("Weapons");
	if (LowerPath.Contains(TEXT("enemy")) || LowerPath.Contains(TEXT("ai")) || LowerPath.Contains(TEXT("agent"))) return TEXT("AI/Enemies");
	if (LowerPath.Contains(TEXT("player"))) return TEXT("Player");
	if (LowerPath.Contains(TEXT("character"))) return TEXT("Characters");
	if (LowerPath.Contains(TEXT("controller"))) return TEXT("Controllers");
	if (LowerPath.Contains(TEXT("gamemode")) || LowerPath.Contains(TEXT("game_mode"))) return TEXT("GameModes");
	if (LowerPath.Contains(TEXT("wall")) || LowerPath.Contains(TEXT("obstacle")) || LowerPath.Contains(TEXT("barrier"))) return TEXT("Obstacles");
	if (LowerPath.Contains(TEXT("trigger")) || LowerPath.Contains(TEXT("trap"))) return TEXT("Triggers");
	if (LowerPath.Contains(TEXT("ui")) || LowerPath.Contains(TEXT("hud")) || LowerPath.Contains(TEXT("widget"))) return TEXT("UI");
	if (LowerPath.Contains(TEXT("effect")) || LowerPath.Contains(TEXT("vfx")) || LowerPath.Contains(TEXT("particle"))) return TEXT("Effects");

	// Fall back to parent class categorization
	FString LowerParent = ParentClass.ToLower();
	if (LowerParent.Contains(TEXT("character"))) return TEXT("Characters");
	if (LowerParent.Contains(TEXT("pawn"))) return TEXT("Pawns");
	if (LowerParent.Contains(TEXT("controller"))) return TEXT("Controllers");
	if (LowerParent.Contains(TEXT("gamemode"))) return TEXT("GameModes");

	// Default category
	return TEXT("Actors");
}

FString UMCPCommandPanelSubsystem::ConvertDiscoveredBlueprintsToJson() const
{
	// Build JSON matching the expected format for HandleSchemaReceived
	// Format: {"project": "...", "schema": {"systems": {"Category": ["BP1", "BP2"]}}}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
	TSharedPtr<FJsonObject> SchemaObject = MakeShareable(new FJsonObject());
	TSharedPtr<FJsonObject> SystemsObject = MakeShareable(new FJsonObject());

	// Group Blueprints by category
	TMap<FString, TArray<FString>> CategorizedBPs;
	for (const FMCPDiscoveredBlueprint& BP : DiscoveredBlueprints)
	{
		if (!CategorizedBPs.Contains(BP.Category))
		{
			CategorizedBPs.Add(BP.Category, TArray<FString>());
		}
		CategorizedBPs[BP.Category].Add(BP.DisplayName);
	}

	// Convert to JSON arrays
	for (const auto& Pair : CategorizedBPs)
	{
		TArray<TSharedPtr<FJsonValue>> BPArray;
		for (const FString& BPName : Pair.Value)
		{
			BPArray.Add(MakeShareable(new FJsonValueString(BPName)));
		}
		SystemsObject->SetArrayField(Pair.Key, BPArray);
	}

	SchemaObject->SetObjectField(TEXT("systems"), SystemsObject);
	RootObject->SetStringField(TEXT("project"), GetCurrentProjectName());
	RootObject->SetObjectField(TEXT("schema"), SchemaObject);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	return OutputString;
}

TArray<FMCPDiscoveredBlueprint> UMCPCommandPanelSubsystem::GetBlueprintsByCategory(const FString& Category) const
{
	TArray<FMCPDiscoveredBlueprint> FilteredBPs;
	for (const FMCPDiscoveredBlueprint& BP : DiscoveredBlueprints)
	{
		if (BP.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			FilteredBPs.Add(BP);
		}
	}
	return FilteredBPs;
}

TArray<FString> UMCPCommandPanelSubsystem::GetDiscoveredCategories() const
{
	TArray<FString> Categories;
	for (const FMCPDiscoveredBlueprint& BP : DiscoveredBlueprints)
	{
		Categories.AddUnique(BP.Category);
	}
	Categories.Sort();
	return Categories;
}

void UMCPCommandPanelSubsystem::SetScanFolders(const TArray<FString>& Folders)
{
	ScanFolders = Folders;
	LogMessage(FString::Printf(TEXT("Scan folders updated: %d folders"), Folders.Num()));
}

void UMCPCommandPanelSubsystem::SetScanParentClasses(const TArray<FString>& ClassNames)
{
	ScanParentClasses = ClassNames;
	LogMessage(FString::Printf(TEXT("Scan parent classes updated: %d classes"), ClassNames.Num()));
}

// ========== COMMAND EXECUTION ==========

bool UMCPCommandPanelSubsystem::SendCommand(const FString& Command)
{
	if (Command.IsEmpty())
	{
		LogMessage(TEXT("Cannot send empty command"), true);
		OnCommandError.Broadcast(TEXT("Command cannot be empty"));
		return false;
	}

	LogMessage(FString::Printf(TEXT("Sending command: %s"), *Command));
	WriteDebugLog(FString::Printf(TEXT("SEND: Command = %s"), *Command));

	// Create JSON payload
	TSharedPtr<FJsonObject> JsonPayload = MakeShareable(new FJsonObject());
	JsonPayload->SetStringField(TEXT("command"), Command);

	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(JsonPayload.ToSharedRef(), JsonWriter))
	{
		LogMessage(TEXT("Failed to serialize command JSON"), true);
		OnCommandError.Broadcast(TEXT("Failed to create JSON payload"));
		return false;
	}

	// Create and send HTTP request
	TSharedRef<IHttpRequest> HttpRequest = CreateHttpPostRequest(TEXT("/execute_command"), JsonString);

	FString CommandCopy = Command;
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, CommandCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			this->OnExecuteCommandResponse(Request, Response, bSuccess);
		}
	);

	if (!HttpRequest->ProcessRequest())
	{
		LogMessage(TEXT("Failed to process HTTP request"), true);
		OnCommandError.Broadcast(TEXT("Failed to send HTTP request"));
		WriteDebugLog(TEXT("SEND: HTTP request failed to process"));
		return false;
	}

	WriteDebugLog(TEXT("SEND: HTTP request sent successfully"));
	return true;
}

void UMCPCommandPanelSubsystem::OnExecuteCommandResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		LogMessage(TEXT("Execute command HTTP request failed"), true);
		OnCommandError.Broadcast(TEXT("HTTP request failed - is the MCP server running?"));
		WriteDebugLog(TEXT("RESPONSE: HTTP request failed (no response)"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();

	WriteDebugLog(FString::Printf(TEXT("RESPONSE: Code=%d, Body=%s"), ResponseCode, *ResponseBody.Left(200)));

	if (ResponseCode != 200)
	{
		LogMessage(FString::Printf(TEXT("Server returned error code: %d"), ResponseCode), true);
		OnCommandError.Broadcast(FString::Printf(TEXT("Server error: %d - %s"), ResponseCode, *ResponseBody));
		return;
	}

	// Parse JSON response
	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(JsonReader, JsonResponse) || !JsonResponse.IsValid())
	{
		LogMessage(TEXT("Failed to parse JSON response"), true);
		OnCommandError.Broadcast(TEXT("Invalid JSON response from server"));
		return;
	}

	// Extract task_id and start polling
	FString TaskId;
	if (JsonResponse->TryGetStringField(TEXT("task_id"), TaskId))
	{
		LogMessage(FString::Printf(TEXT("Received task_id: %s"), *TaskId));
		WriteDebugLog(FString::Printf(TEXT("RESPONSE: Got task_id = %s"), *TaskId));

		FString Command = Request->GetURL();
		StartPollingTask(TaskId, Command);
	}
	else
	{
		// Immediate response (no task_id means synchronous completion)
		LogMessage(TEXT("Command completed synchronously"));
		OnCommandResponse.Broadcast(ResponseBody, true);
	}
}

// ========== TASK POLLING ==========

void UMCPCommandPanelSubsystem::StartPollingTask(const FString& TaskId, const FString& Command)
{
	FMCPActiveTask NewTask;
	NewTask.TaskId = TaskId;
	NewTask.Command = Command;
	NewTask.Status = TEXT("pending");
	NewTask.Progress = 0.0f;
	NewTask.LastPollTime = 0.0f;
	NewTask.FailedPolls = 0;

	ActiveTasks.Add(TaskId, NewTask);
	bHasActiveTasks = true;

	LogMessage(FString::Printf(TEXT("Started polling for task: %s"), *TaskId));
	WriteDebugLog(FString::Printf(TEXT("POLL_START: Task %s added to active tasks"), *TaskId));

	// Immediately do first poll
	PollTaskStatus(TaskId);
}

void UMCPCommandPanelSubsystem::StopPollingTask(const FString& TaskId)
{
	if (ActiveTasks.Remove(TaskId) > 0)
	{
		LogMessage(FString::Printf(TEXT("Stopped polling for task: %s"), *TaskId));
		WriteDebugLog(FString::Printf(TEXT("POLL_STOP: Task %s removed from active tasks"), *TaskId));
	}

	bHasActiveTasks = (ActiveTasks.Num() > 0);
}

void UMCPCommandPanelSubsystem::PollTaskStatus(const FString& TaskId)
{
	FString Endpoint = FString::Printf(TEXT("/task_status/%s"), *TaskId);
	TSharedRef<IHttpRequest> HttpRequest = CreateHttpGetRequest(Endpoint);

	FString TaskIdCopy = TaskId;
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, TaskIdCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			this->OnTaskStatusResponse(Request, Response, bSuccess, TaskIdCopy);
		}
	);

	if (!HttpRequest->ProcessRequest())
	{
		LogMessage(FString::Printf(TEXT("Failed to poll task %s"), *TaskId), true);
		WriteDebugLog(FString::Printf(TEXT("POLL: Failed to send request for %s"), *TaskId));
	}
}

void UMCPCommandPanelSubsystem::OnTaskStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TaskId)
{
	FMCPActiveTask* Task = ActiveTasks.Find(TaskId);
	if (!Task)
	{
		return; // Task was cancelled/removed
	}

	if (!bWasSuccessful || !Response.IsValid())
	{
		Task->FailedPolls++;
		LogMessage(FString::Printf(TEXT("Poll failed for task %s (attempt %d/%d)"), *TaskId, Task->FailedPolls, MaxFailedPolls), true);
		WriteDebugLog(FString::Printf(TEXT("POLL: Failed for %s (%d/%d)"), *TaskId, Task->FailedPolls, MaxFailedPolls));

		if (Task->FailedPolls >= MaxFailedPolls)
		{
			OnCommandError.Broadcast(FString::Printf(TEXT("Lost connection to task %s"), *TaskId));
			OnTaskCompleted.Broadcast(TaskId, false, TEXT("{\"error\": \"Connection lost\"}"));
			StopPollingTask(TaskId);
		}
		return;
	}

	Task->FailedPolls = 0; // Reset on success

	// Parse response
	TSharedPtr<FJsonObject> JsonResponse;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

	if (!FJsonSerializer::Deserialize(JsonReader, JsonResponse) || !JsonResponse.IsValid())
	{
		LogMessage(FString::Printf(TEXT("Invalid JSON in poll response for %s"), *TaskId), true);
		return;
	}

	// Update task state
	UpdateTaskFromResponse(TaskId, JsonResponse);
}

void UMCPCommandPanelSubsystem::UpdateTaskFromResponse(const FString& TaskId, TSharedPtr<FJsonObject> JsonResponse)
{
	FMCPActiveTask* Task = ActiveTasks.Find(TaskId);
	if (!Task)
	{
		return;
	}

	FString Status = JsonResponse->GetStringField(TEXT("status"));
	Task->Status = Status;
	Task->Progress = JsonResponse->GetNumberField(TEXT("progress"));
	Task->Message = JsonResponse->HasField(TEXT("message")) ? JsonResponse->GetStringField(TEXT("message")) : TEXT("");
	Task->SpawnedActors = JsonResponse->HasField(TEXT("spawned_actors")) ? JsonResponse->GetIntegerField(TEXT("spawned_actors")) : 0;
	Task->TotalActors = JsonResponse->HasField(TEXT("total_actors")) ? JsonResponse->GetIntegerField(TEXT("total_actors")) : 0;

	WriteDebugLog(FString::Printf(TEXT("POLL_UPDATE: %s status=%s progress=%.2f actors=%d/%d"),
		*TaskId, *Status, Task->Progress, Task->SpawnedActors, Task->TotalActors));

	// Broadcast progress update
	OnTaskProgress.Broadcast(
		TaskId,
		Task->Status,
		Task->Progress,
		Task->Message,
		Task->SpawnedActors
	);

	// Check for completion
	if (Status == TEXT("completed"))
	{
		FString ResultJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
		FJsonSerializer::Serialize(JsonResponse.ToSharedRef(), Writer);

		LogMessage(FString::Printf(TEXT("Task %s completed successfully"), *TaskId));
		WriteDebugLog(FString::Printf(TEXT("TASK_COMPLETE: %s SUCCESS"), *TaskId));
		OnTaskCompleted.Broadcast(TaskId, true, ResultJson);

		StopPollingTask(TaskId);
	}
	else if (Status == TEXT("failed") || Status == TEXT("cancelled"))
	{
		FString Error = JsonResponse->HasField(TEXT("error"))
			? JsonResponse->GetStringField(TEXT("error"))
			: FString::Printf(TEXT("Task %s"), *Status);

		LogMessage(FString::Printf(TEXT("Task %s %s: %s"), *TaskId, *Status, *Error), true);
		WriteDebugLog(FString::Printf(TEXT("TASK_COMPLETE: %s FAILED - %s"), *TaskId, *Error));
		OnCommandError.Broadcast(Error);
		OnTaskCompleted.Broadcast(TaskId, false, JsonResponse->HasField(TEXT("error"))
			? FString::Printf(TEXT("{\"error\": \"%s\"}"), *Error)
			: TEXT("{}"));

		StopPollingTask(TaskId);
	}
}

// ========== DEBUG FILE LOGGING ==========

void UMCPCommandPanelSubsystem::InitDebugLog()
{
	DebugLogPath = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("MCP_Debug.txt");

	FString Header = FString::Printf(
		TEXT("========================================\n")
		TEXT("MCP Debug Log\n")
		TEXT("Project: %s\n")
		TEXT("Started: %s\n")
		TEXT("========================================\n"),
		*GetCurrentProjectName(),
		*FDateTime::Now().ToString()
	);

	FFileHelper::SaveStringToFile(
		Header,
		*DebugLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
	);

	UE_LOG(LogTemp, Display, TEXT("MCP Debug log initialized: %s"), *DebugLogPath);
}

void UMCPCommandPanelSubsystem::WriteDebugLog(const FString& Event)
{
	if (DebugLogPath.IsEmpty())
	{
		return;
	}

	FString Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s"));
	FString Line = FString::Printf(TEXT("[%s] %s\n"), *Timestamp, *Event);

	FFileHelper::SaveStringToFile(
		Line,
		*DebugLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		EFileWrite::FILEWRITE_Append
	);
}

void UMCPCommandPanelSubsystem::CloseDebugLog()
{
	if (DebugLogPath.IsEmpty())
	{
		return;
	}

	WriteDebugLog(TEXT("========================================"));
	WriteDebugLog(TEXT("MCP Debug Log Closed"));
	WriteDebugLog(TEXT("========================================"));
}

// ========== TASK CANCELLATION ==========

bool UMCPCommandPanelSubsystem::CancelTask(const FString& TaskId)
{
	if (!ActiveTasks.Contains(TaskId))
	{
		LogMessage(FString::Printf(TEXT("Cannot cancel unknown task: %s"), *TaskId), true);
		return false;
	}

	FString Endpoint = FString::Printf(TEXT("/task_cancel/%s"), *TaskId);
	FString JsonPayload = TEXT("{}");

	TSharedRef<IHttpRequest> HttpRequest = CreateHttpPostRequest(Endpoint, JsonPayload);

	FString TaskIdCopy = TaskId;
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, TaskIdCopy](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			this->OnCancelTaskResponse(Request, Response, bSuccess, TaskIdCopy);
		}
	);

	if (!HttpRequest->ProcessRequest())
	{
		LogMessage(FString::Printf(TEXT("Failed to send cancel for task %s"), *TaskId), true);
		return false;
	}

	LogMessage(FString::Printf(TEXT("Cancellation requested for task: %s"), *TaskId));
	WriteDebugLog(FString::Printf(TEXT("CANCEL: Requested for %s"), *TaskId));
	return true;
}

void UMCPCommandPanelSubsystem::OnCancelTaskResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TaskId)
{
	if (!bWasSuccessful)
	{
		LogMessage(FString::Printf(TEXT("Cancel request failed for task %s"), *TaskId), true);
		return;
	}

	LogMessage(FString::Printf(TEXT("Cancel acknowledged for task %s"), *TaskId));
	WriteDebugLog(FString::Printf(TEXT("CANCEL: Acknowledged for %s"), *TaskId));
}

// ========== EXISTING METHODS (Operations & Schema) ==========

bool UMCPCommandPanelSubsystem::RequestOperationsList()
{
	LogMessage(TEXT("Requesting operations list"));

	TSharedRef<IHttpRequest> HttpRequest = CreateHttpGetRequest(TEXT("/operations"));
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMCPCommandPanelSubsystem::OnOperationsListReceived);

	if (!HttpRequest->ProcessRequest())
	{
		LogMessage(TEXT("Failed to request operations list"), true);
		OnCommandError.Broadcast(TEXT("Failed to request operations list"));
		return false;
	}

	return true;
}

void UMCPCommandPanelSubsystem::OnOperationsListReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		OnCommandError.Broadcast(TEXT("Failed to retrieve operations list"));
		return;
	}

	if (Response->GetResponseCode() == 200)
	{
		LogMessage(TEXT("Operations list received"));
		OnOperationsReceived.Broadcast(Response->GetContentAsString());
	}
	else
	{
		OnCommandError.Broadcast(FString::Printf(TEXT("Server error: %d"), Response->GetResponseCode()));
	}
}

bool UMCPCommandPanelSubsystem::RequestProjectSchema()
{
	// UPDATED: Now triggers LOCAL scan instead of HTTP request!
	LogMessage(TEXT("RequestProjectSchema called - using LOCAL Blueprint scan"));
	WriteDebugLog(TEXT("SCHEMA: Using local scan (no HTTP)"));

	ScanProjectBlueprints();
	return true;
}

// ========== CONFIGURATION ==========

void UMCPCommandPanelSubsystem::SetServerURL(const FString& NewURL)
{
	ServerURL = NewURL;
	LogMessage(FString::Printf(TEXT("Server URL changed to: %s"), *ServerURL));
}

void UMCPCommandPanelSubsystem::SetPollingInterval(float IntervalSeconds)
{
	PollingInterval = FMath::Max(0.5f, IntervalSeconds);
	LogMessage(FString::Printf(TEXT("Polling interval set to: %.1f seconds"), PollingInterval));
}

// ========== TASK QUERIES ==========

TArray<FMCPActiveTask> UMCPCommandPanelSubsystem::GetActiveTasks() const
{
	TArray<FMCPActiveTask> Tasks;
	ActiveTasks.GenerateValueArray(Tasks);
	return Tasks;
}

bool UMCPCommandPanelSubsystem::GetTaskInfo(const FString& TaskId, FMCPActiveTask& OutTaskInfo) const
{
	const FMCPActiveTask* Task = ActiveTasks.Find(TaskId);
	if (Task)
	{
		OutTaskInfo = *Task;
		return true;
	}
	return false;
}

FString UMCPCommandPanelSubsystem::GetDebugLogPath() const
{
	return DebugLogPath;
}

// ========== HTTP HELPERS ==========

TSharedRef<IHttpRequest> UMCPCommandPanelSubsystem::CreateHttpPostRequest(const FString& Endpoint, const FString& JsonPayload)
{
	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();

	FString FullURL = ServerURL + Endpoint;
	HttpRequest->SetURL(FullURL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonPayload);

	// Short timeout - we expect immediate response for async operations
	HttpRequest->SetTimeout(10.0f);

	return HttpRequest;
}

TSharedRef<IHttpRequest> UMCPCommandPanelSubsystem::CreateHttpGetRequest(const FString& Endpoint)
{
	TSharedRef<IHttpRequest> HttpRequest = HttpModule->CreateRequest();

	FString FullURL = ServerURL + Endpoint;
	HttpRequest->SetURL(FullURL);
	HttpRequest->SetVerb(TEXT("GET"));

	// Short timeout for polling
	HttpRequest->SetTimeout(5.0f);

	return HttpRequest;
}

void UMCPCommandPanelSubsystem::LogMessage(const FString& Message, bool bIsError)
{
	if (bIsError)
	{
		UE_LOG(LogTemp, Error, TEXT("MCPCommandPanel: %s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("MCPCommandPanel: %s"), *Message);
	}
}