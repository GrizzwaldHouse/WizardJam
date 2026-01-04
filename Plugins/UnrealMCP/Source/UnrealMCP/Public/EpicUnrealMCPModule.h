#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
class FToolBarBuilder;
class FExtender;

class FEpicUnrealMCPModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FEpicUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FEpicUnrealMCPModule>("UnrealMCP");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCP");
	}
	private:
	// Register toolbar extension for MCP Command Panel button 
	void RegisterMenuExtension();

	// Add the MCP Panel button to the toolbar 
	void AddToolbarButton(FToolBarBuilder& Builder);

	// Open the MCP Command Panel widget 
	static void OpenMCPCommandPanel();

	// Toolbar extender for adding custom buttons 
	TSharedPtr<FExtender> ToolbarExtender;
}; 