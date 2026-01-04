// SpellMaterialFactory.h
// Developer: Marcus Daley
// Date: December 25, 2025
// Purpose: Editor utility to auto-create M_SpellCollectible_Colorable material
//
// Usage:
// 1. Add this file to your project (Editor module or WITH_EDITOR blocks)
// 2. In Editor, call USpellMaterialFactory::CreateColorableMaterial()
// 3. Or use the Blueprint-callable function from any Editor Utility Widget
// 4. Material is created at /Game/Materials/M_SpellCollectible_Colorable
// 5. Only needs to run ONCE - material persists in project forever
//
// This is OPTIONAL - the SpellCollectible code works without this material
// But having this material guarantees perfect color application

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SpellMaterialFactory.generated.h"

UCLASS()
class USpellMaterialFactory : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Create the M_SpellCollectible_Colorable material asset
    // Returns true if created successfully (or already exists)
    // Call this once from Editor - material persists forever
    UFUNCTION(BlueprintCallable, Category = "WizardJam|Setup", meta = (CallInEditor = "true"))
    static bool CreateColorableMaterial();

    // Check if the colorable material already exists
    UFUNCTION(BlueprintPure, Category = "WizardJam|Setup")
    static bool DoesColorableMaterialExist();
};