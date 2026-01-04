#include "EpicUnrealMCPModule.h"
#include "EpicUnrealMCPBridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#define LOCTEXT_NAMESPACE "FEpicUnrealMCPModule"

void FEpicUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has started"));
		// Register menu extension to add MCP Command Panel button
	RegisterMenuExtension();
}

void FEpicUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("Epic Unreal MCP Module has shut down"));
		// Unregister menu extensions
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);
	}
}
void FEpicUnrealMCPModule::RegisterMenuExtension()
{
	// Get the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	// Create toolbar extender
	ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Settings",
		EExtensionHook::After,
		nullptr,
		FToolBarExtensionDelegate::CreateRaw(this, &FEpicUnrealMCPModule::AddToolbarButton)
	);

	// Register the extender
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
}

void FEpicUnrealMCPModule::AddToolbarButton(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateStatic(&FEpicUnrealMCPModule::OpenMCPCommandPanel)),
		NAME_None,
		LOCTEXT("MCPPanelButton", "MCP Panel"),
		LOCTEXT("MCPPanelTooltip", "Open the MCP Command Panel for natural language editor commands"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")  // Use built-in icon
	);
}

void FEpicUnrealMCPModule::OpenMCPCommandPanel()
{
	// Path to the Editor Utility Widget in the plugin
	const FString WidgetPath = TEXT("/UnrealMCP/EditorUI/WBP_MCPCommandPanel.WBP_MCPCommandPanel");

	// Load the widget blueprint
	UEditorUtilityWidgetBlueprint* WidgetBlueprint = LoadObject<UEditorUtilityWidgetBlueprint>(
		nullptr,
		*WidgetPath
	);

	if (WidgetBlueprint)
	{
		// Get the editor utility subsystem and spawn the widget
		UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
		if (EditorUtilitySubsystem)
		{
			EditorUtilitySubsystem->SpawnAndRegisterTab(WidgetBlueprint);
			UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Command Panel opened"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to get EditorUtilitySubsystem"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to load WBP_MCPCommandPanel at path: %s"), *WidgetPath);
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Make sure the widget is created in the plugin's Content/EditorUI folder"));
	}
}
#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEpicUnrealMCPModule, UnrealMCP) 