#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

// Server defaults - can be changed in config later
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
}

// Helper: Create JSON error response
FString UEpicUnrealMCPBridge::CreateErrorResponse(const FString& ErrorMessage)
{
    TSharedPtr<FJsonObject> ResponseObj = MakeShareable(new FJsonObject());
    ResponseObj->SetStringField(TEXT("status"), TEXT("error"));
    ResponseObj->SetStringField(TEXT("error"), ErrorMessage);
    return SerializeJsonObject(ResponseObj);
}

// Helper: Create JSON success response with result data
FString UEpicUnrealMCPBridge::CreateSuccessResponse(const TSharedPtr<FJsonObject>& ResultData)
{
    TSharedPtr<FJsonObject> ResponseObj = MakeShareable(new FJsonObject());
    ResponseObj->SetStringField(TEXT("status"), TEXT("success"));
    if (ResultData.IsValid())
    {
        ResponseObj->SetObjectField(TEXT("result"), ResultData);
    }
    return SerializeJsonObject(ResponseObj);
}

// Helper: Serialize JSON object to string
FString UEpicUnrealMCPBridge::SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    return OutputString;
}

// Helper: Spawn a single actor (delegates to EditorCommands)
FString UEpicUnrealMCPBridge::SpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = EditorCommands->HandleCommand(TEXT("spawn_actor"), Params);

    if (Result.IsValid())
    {
        // Check if command was successful
        if (Result->HasField(TEXT("success")) && Result->GetBoolField(TEXT("success")))
        {
            return CreateSuccessResponse(Result);
        }
        else if (Result->HasField(TEXT("error")))
        {
            return CreateErrorResponse(Result->GetStringField(TEXT("error")));
        }
    }

    return CreateErrorResponse(TEXT("Unknown error in SpawnActor"));
}

// Batch spawn multiple actors in one command - reduces TCP overhead
FString UEpicUnrealMCPBridge::SpawnActorsBatch(const TSharedPtr<FJsonObject>& Params)
{
    // Get the array of actors to spawn
    const TArray<TSharedPtr<FJsonValue>>* ActorsArrayPtr = nullptr;

    // Correct API usage: pass pointer reference, not direct array
    if (!Params->TryGetArrayField(TEXT("actors"), ActorsArrayPtr) || !ActorsArrayPtr)
    {
        return CreateErrorResponse(TEXT("Missing 'actors' array parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>& ActorsArray = *ActorsArrayPtr;

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    // Spawn each actor in the batch
    for (const TSharedPtr<FJsonValue>& ActorValue : ActorsArray)
    {
        if (ActorValue->Type != EJson::Object)
        {
            FailCount++;
            // Add error result for non-object entries
            TSharedPtr<FJsonObject> ErrorObj = MakeShareable(new FJsonObject());
            ErrorObj->SetStringField(TEXT("status"), TEXT("error"));
            ErrorObj->SetStringField(TEXT("error"), TEXT("Invalid actor data (not an object)"));
            Results.Add(MakeShareable(new FJsonValueObject(ErrorObj)));
            continue;
        }

        TSharedPtr<FJsonObject> ActorParams = ActorValue->AsObject();

        // Spawn the actor using existing single-spawn logic
        FString Result = SpawnActor(ActorParams);

        // Parse result and add to results array
        TSharedPtr<FJsonObject> ResultObj;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result);
        if (FJsonSerializer::Deserialize(Reader, ResultObj))
        {
            if (ResultObj->GetStringField(TEXT("status")) == TEXT("success"))
            {
                SuccessCount++;
            }
            else
            {
                FailCount++;
            }
            Results.Add(MakeShareable(new FJsonValueObject(ResultObj)));
        }
        else
        {
            // Failed to parse result
            FailCount++;
            TSharedPtr<FJsonObject> ErrorObj = MakeShareable(new FJsonObject());
            ErrorObj->SetStringField(TEXT("status"), TEXT("error"));
            ErrorObj->SetStringField(TEXT("error"), TEXT("Failed to parse spawn result"));
            Results.Add(MakeShareable(new FJsonValueObject(ErrorObj)));
        }
    }

    // Create batch response with statistics
    TSharedPtr<FJsonObject> ResponseObj = MakeShareable(new FJsonObject());
    ResponseObj->SetStringField(TEXT("status"), TEXT("success"));
    ResponseObj->SetNumberField(TEXT("success_count"), SuccessCount);
    ResponseObj->SetNumberField(TEXT("fail_count"), FailCount);
    ResponseObj->SetNumberField(TEXT("total"), ActorsArray.Num());
    ResponseObj->SetArrayField(TEXT("results"), Results);

    return SerializeJsonObject(ResponseObj);
}

// Initialize subsystem - auto-starts server
void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Initializing"));

    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server on configured port
void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server and clean up resources
void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

// Execute a command received from Python MCP client
FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);

    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    // Queue execution on Game Thread (Unreal requires editor operations on main thread)
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
        {
            TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

            try
            {
                TSharedPtr<FJsonObject> ResultJson;

                // Simple ping command
                if (CommandType == TEXT("ping"))
                {
                    ResultJson = MakeShareable(new FJsonObject);
                    ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
                }
                // Batch spawning - NEW COMMAND
                else if (CommandType == TEXT("spawn_actors_batch"))
                {
                    FString BatchResult = SpawnActorsBatch(Params);
                    Promise.SetValue(BatchResult);
                    return; // Early return for batch spawning
                }
                // Editor Commands (including actor manipulation)
                else if (CommandType == TEXT("get_actors_in_level") ||
                    CommandType == TEXT("find_actors_by_name") ||
                    CommandType == TEXT("spawn_actor") ||
                    CommandType == TEXT("delete_actor") ||
                    CommandType == TEXT("set_actor_transform") ||
                    CommandType == TEXT("spawn_blueprint_actor"))
                {
                    ResultJson = EditorCommands->HandleCommand(CommandType, Params);
                }
                // Blueprint Commands
                else if (CommandType == TEXT("create_blueprint") ||
                    CommandType == TEXT("add_component_to_blueprint") ||
                    CommandType == TEXT("set_physics_properties") ||
                    CommandType == TEXT("compile_blueprint") ||
                    CommandType == TEXT("set_static_mesh_properties") ||
                    CommandType == TEXT("set_mesh_material_color") ||
                    CommandType == TEXT("get_available_materials") ||
                    CommandType == TEXT("apply_material_to_actor") ||
                    CommandType == TEXT("apply_material_to_blueprint") ||
                    CommandType == TEXT("get_actor_material_info") ||
                    CommandType == TEXT("get_blueprint_material_info"))
                {
                    ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
                }
                else
                {
                    // Unknown command
                    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                    ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));

                    FString ResultString;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                    Promise.SetValue(ResultString);
                    return;
                }

                // Check if the result contains an error
                bool bSuccess = true;
                FString ErrorMessage;

                if (ResultJson->HasField(TEXT("success")))
                {
                    bSuccess = ResultJson->GetBoolField(TEXT("success"));
                    if (!bSuccess && ResultJson->HasField(TEXT("error")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                    }
                }

                if (bSuccess)
                {
                    // Set success status and include the result
                    ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                    ResponseJson->SetObjectField(TEXT("result"), ResultJson);
                }
                else
                {
                    // Set error status and include the error message
                    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                    ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
                }
            }
            catch (const std::exception& e)
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
            }

            FString ResultString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
            FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
            Promise.SetValue(ResultString);
        });

    return Future.Get();
}