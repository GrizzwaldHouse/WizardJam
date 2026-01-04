// Copyright (c) 2025 Marcus Daley. All Rights Reserved.
// Island Escape - Game Architecture Capstone (GAR2510/END2507)
// File: MCPCommandPanelSubsystem.h
// Purpose: Editor subsystem for MCP UI communication with async task polling

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Http.h"
#include "Tickable.h"
#include "MCPCommandPanelSubsystem.generated.h"

// Delegate for when a command response is received
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMCPCommandResponse, const FString&, Response, bool, bSuccess);

// Delegate for command errors
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMCPCommandError, const FString&, ErrorMessage);

// Delegate for operations list
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMCPOperationsReceived, const FString&, OperationsJson);

// Delegate for project schema
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMCPSchemaReceived, const FString&, SchemaJson);


//  Delegate for task progress updates (fires every poll cycle)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnMCPTaskProgress,
	const FString&, TaskId,
	const FString&, Status,
	float, Progress,
	const FString&, Message,
	int32, SpawnedActors
);

//  Delegate for task completion (fires once when task finishes)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnMCPTaskCompleted,
	const FString&, TaskId,
	bool, bSuccess,
	const FString&, ResultJson
);
// Struct for discovered Blueprint info
USTRUCT(BlueprintType)
struct FMCPDiscoveredBlueprint
{
	GENERATED_BODY()

	// Friendly display name (e.g., "BP_Spawner")
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	// Full asset path (e.g., "/Game/Code/Actors/BP_Spawner")
	UPROPERTY(BlueprintReadOnly)
	FString AssetPath;

	// Category based on folder or parent class (e.g., "Spawning", "Pickups")
	UPROPERTY(BlueprintReadOnly)
	FString Category;

	// Parent class name for filtering (e.g., "Actor", "Pawn", "Character")
	UPROPERTY(BlueprintReadOnly)
	FString ParentClassName;
};
// Task info struct for tracking active polls
USTRUCT(BlueprintType)
struct FMCPActiveTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TaskId;

	UPROPERTY(BlueprintReadOnly)
	FString Command;

	UPROPERTY(BlueprintReadOnly)
	FString Status;

	UPROPERTY(BlueprintReadOnly)
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FString Message;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalActors = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SpawnedActors = 0;

	// Internal tracking
	float LastPollTime = 0.0f;
	int32 FailedPolls = 0;
};

/**
 * Editor subsystem for MCP Command Panel UI communication.
 *
 * Handles HTTP communication with the Python MCP server including:
 * - Async command execution with task_id return
 * - Automatic progress polling
 * - Operations list retrieval
 * - Project schema retrieval
 * - LOCAL Blueprint scanning
 *
 * Usage from Blueprint:
 *   1. Bind to OnTaskProgress for real-time updates
 *   2. Bind to OnTaskCompleted for final results
 *   3. Call SendCommand() with natural language
 *   4. Progress updates fire automatically every 1.5 seconds
 */
UCLASS()
class UNREALMCP_API UMCPCommandPanelSubsystem : public UEditorSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject interface (for polling timer)
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bHasActiveTasks; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMCPCommandPanelSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
	// ========== COMMAND EXECUTION ==========

	/**
	 * Send a natural language command to the MCP server.
	 * Returns immediately - command executes asynchronously.
	 *
	 * Listen to OnTaskProgress for updates
	 * Listen to OnTaskCompleted for final result
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	bool SendCommand(const FString& Command);

	// Request the list of available MCP operations
	
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	bool RequestOperationsList();

	// Request the project schema
	 
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	bool RequestProjectSchema();

	//Cancel an active task (best-effort)
	 
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	bool CancelTask(const FString& TaskId);

	// ========== LOCAL BLUEPRINT SCANNING ==========

	// Scan the current project for Blueprint actors
	//scans /Game/ folder using Asset Registry
	// Results broadcast via OnSchemaReceived delegate (as JSON for compatibility)
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	void ScanProjectBlueprints();

	// Get discovered Blueprints as structured data (alternative to JSON)
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	TArray<FMCPDiscoveredBlueprint> GetDiscoveredBlueprints() const { return DiscoveredBlueprints; }

	// Get discovered Blueprints filtered by category
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	TArray<FMCPDiscoveredBlueprint> GetBlueprintsByCategory(const FString& Category) const;

	// Get all discovered categories
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	TArray<FString> GetDiscoveredCategories() const;

	// Check if scan has been performed
	UFUNCTION(BlueprintPure, Category = "MCP Command Panel|Project Discovery")
	bool HasScannedProject() const { return bHasScannedProject; }

	// Get the current project name
	UFUNCTION(BlueprintPure, Category = "MCP Command Panel|Project Discovery")
	FString GetCurrentProjectName() const;

	// ========== CONFIGURATION ==========

	UFUNCTION(BlueprintPure, Category = "MCP Command Panel")
	FString GetServerURL() const { return ServerURL; }

	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void SetServerURL(const FString& NewURL);

	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel")
	void SetPollingInterval(float IntervalSeconds);
	// Set folders to scan (default: /Game/)
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	void SetScanFolders(const TArray<FString>& Folders);

	// Set parent classes to filter for (default: Actor, Pawn, Character)
	UFUNCTION(BlueprintCallable, Category = "MCP Command Panel|Project Discovery")
	void SetScanParentClasses(const TArray<FString>& ClassNames);

	// ========== TASK QUERIES ==========

	UFUNCTION(BlueprintPure, Category = "MCP Command Panel")
	bool HasActiveTasks() const { return bHasActiveTasks; }

	UFUNCTION(BlueprintPure, Category = "MCP Command Panel")
	TArray<FMCPActiveTask> GetActiveTasks() const;

	UFUNCTION(BlueprintPure, Category = "MCP Command Panel")
	bool GetTaskInfo(const FString& TaskId, FMCPActiveTask& OutTaskInfo) const;
	// Get debug log file path
	UFUNCTION(BlueprintPure, Category = "MCP Command Panel")
	FString GetDebugLogPath() const;
	// ========== DELEGATES ==========

	// Legacy delegate (still fires for backward compatibility)
	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPCommandResponse OnCommandResponse;

	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPCommandError OnCommandError;

	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPOperationsReceived OnOperationsReceived;

	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPSchemaReceived OnSchemaReceived;

	// Progress tracking delegates
	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPTaskProgress OnTaskProgress;

	UPROPERTY(BlueprintAssignable, Category = "MCP Command Panel")
	FOnMCPTaskCompleted OnTaskCompleted;

private:
	// Server configuration
	FString ServerURL;
	FHttpModule* HttpModule;

	// Polling configuration
	float PollingInterval = 1.5f;  // seconds between polls
	int32 MaxFailedPolls = 5;      // give up after this many failures
	float TimeSinceLastPoll = 0.0f;

	// Active tasks being polled
	TMap<FString, FMCPActiveTask> ActiveTasks;
	bool bHasActiveTasks = false;
	// ========== LOCAL BLUEPRINT SCAN STATE ==========

	// Cached discovered Blueprints
	TArray<FMCPDiscoveredBlueprint> DiscoveredBlueprints;

	// Whether we've scanned the project
	bool bHasScannedProject = false;

	// Folders to scan (default: /Game/)
	TArray<FString> ScanFolders;

	// Parent classes to look for (default: Actor, Pawn, Character, etc.)
	TArray<FString> ScanParentClasses;

	// Perform the actual Asset Registry scan
	void PerformBlueprintScan();

	// Categorize a Blueprint based on its path and parent class
	FString CategorizeBlueprint(const FString& AssetPath, const FString& ParentClass) const;

	// Convert discovered Blueprints to JSON for OnSchemaReceived delegate
	FString ConvertDiscoveredBlueprintsToJson() const;
	// HTTP response handlers
	void OnExecuteCommandResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnTaskStatusResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TaskId);
	void OnOperationsListReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnSchemaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void OnCancelTaskResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString TaskId);

	// Legacy handler for schema (unchanged)
	void OnCommandResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// Helper to create HTTP requests
	TSharedRef<IHttpRequest> CreateHttpPostRequest(const FString& Endpoint, const FString& JsonPayload);
	TSharedRef<IHttpRequest> CreateHttpGetRequest(const FString& Endpoint);

	// Logging
	void LogMessage(const FString& Message, bool bIsError = false);

	// Task management
	void StartPollingTask(const FString& TaskId, const FString& Command);
	void StopPollingTask(const FString& TaskId);
	void PollTaskStatus(const FString& TaskId);
	void UpdateTaskFromResponse(const FString& TaskId, TSharedPtr<FJsonObject> JsonResponse);
	// Debug file logging
	FString DebugLogPath;
	void InitDebugLog();
	void WriteDebugLog(const FString& Event);
	void CloseDebugLog();
};






