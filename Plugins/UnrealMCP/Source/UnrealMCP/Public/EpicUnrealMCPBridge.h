#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "EpicUnrealMCPBridge.generated.h"

class FMCPServerRunnable;

// Editor subsystem for MCP Bridge - handles communication with external Python tools
UCLASS()
class UNREALMCP_API UEpicUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UEpicUnrealMCPBridge();
	virtual ~UEpicUnrealMCPBridge();

	// Subsystem lifecycle
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server control
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution - main entry point for all MCP commands
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Batch spawning support - spawns multiple actors in one call
	FString SpawnActorsBatch(const TSharedPtr<FJsonObject>& Params);

	// Helper functions for JSON and spawning
	FString CreateErrorResponse(const FString& ErrorMessage);
	FString CreateSuccessResponse(const TSharedPtr<FJsonObject>& ResultData);
	FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	FString SpawnActor(const TSharedPtr<FJsonObject>& Params);

	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handlers - delegate to specialized command classes
	TSharedPtr<FEpicUnrealMCPEditorCommands> EditorCommands;
	TSharedPtr<FEpicUnrealMCPBlueprintCommands> BlueprintCommands;
};