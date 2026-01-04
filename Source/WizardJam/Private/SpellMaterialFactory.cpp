// SpellMaterialFactory.cpp
// Developer: Marcus Daley
// Date: December 25, 2025
// Purpose: Creates M_SpellCollectible_Colorable material asset programmatically
//
// This uses Editor-only APIs to create a material with:
// - Vector Parameter named "Color" connected to Base Color
// - Default color of white (1,1,1,1)
// - Saved to /Game/Materials/M_SpellCollectible_Colorable
//
// Designer runs this ONCE (via Editor Utility or console command)
// Material then exists forever in the project

#include "SpellMaterialFactory.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#endif

bool USpellMaterialFactory::DoesColorableMaterialExist()
{
#if WITH_EDITOR
    // Check if material already exists at expected path
    const FString MaterialPath = TEXT("/Game/Materials/M_SpellCollectible_Colorable");

    FSoftObjectPath SoftPath(MaterialPath);
    UObject* ExistingAsset = SoftPath.TryLoad();

    return ExistingAsset != nullptr;
#else
    // In packaged game, assume material exists if project was set up correctly
    return true;
#endif
}

bool USpellMaterialFactory::CreateColorableMaterial()
{
#if WITH_EDITOR
    // Check if already exists
    if (DoesColorableMaterialExist())
    {
        UE_LOG(LogTemp, Display, TEXT("[SpellMaterialFactory] Material already exists - nothing to do"));
        return true;
    }

    // Define paths
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString MaterialName = TEXT("M_SpellCollectible_Colorable");
    const FString FullPackagePath = PackagePath + MaterialName;

    // Ensure Materials folder exists
    const FString ContentPath = FPaths::ProjectContentDir() + TEXT("Materials/");
    if (!IFileManager::Get().DirectoryExists(*ContentPath))
    {
        IFileManager::Get().MakeDirectory(*ContentPath, true);
        UE_LOG(LogTemp, Display, TEXT("[SpellMaterialFactory] Created Materials folder"));
    }

    // Create the package
    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("[SpellMaterialFactory] Failed to create package"));
        return false;
    }

    Package->FullyLoad();

    // Create the material
    UMaterial* NewMaterial = NewObject<UMaterial>(Package, *MaterialName, RF_Public | RF_Standalone);
    if (!NewMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("[SpellMaterialFactory] Failed to create material object"));
        return false;
    }

    // Create Vector Parameter expression for "Color"
    UMaterialExpressionVectorParameter* ColorParam = NewObject<UMaterialExpressionVectorParameter>(NewMaterial);
    if (ColorParam)
    {
        ColorParam->ParameterName = FName("Color");
        ColorParam->DefaultValue = FLinearColor::White;  // Default to white so designer's color shows correctly

        // Position in material editor graph (just for visual organization)
        ColorParam->MaterialExpressionEditorX = -300;
        ColorParam->MaterialExpressionEditorY = 0;

        // Add to material's expressions
        NewMaterial->GetExpressionCollection().AddExpression(ColorParam);

        // Connect to Base Color output
        // In UE5, BaseColor is the primary color input
        NewMaterial->GetEditorOnlyData()->BaseColor.Connect(0, ColorParam);

        UE_LOG(LogTemp, Display, TEXT("[SpellMaterialFactory] Created 'Color' Vector Parameter"));
    }

    // Mark material as needing recompilation
    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();

    // Register with asset registry
    FAssetRegistryModule::AssetCreated(NewMaterial);

    // Mark package dirty so it can be saved
    Package->MarkPackageDirty();

    // Save the package
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        FullPackagePath,
        FPackageName::GetAssetPackageExtension()
    );

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GError;

    const bool bSaved = UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);

    if (bSaved)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[SpellMaterialFactory] SUCCESS - Created and saved M_SpellCollectible_Colorable"));
        UE_LOG(LogTemp, Display,
            TEXT("[SpellMaterialFactory] Material has 'Color' Vector Parameter connected to Base Color"));
        UE_LOG(LogTemp, Display,
            TEXT("[SpellMaterialFactory] SpellCollectible will auto-detect and use this material"));
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SpellMaterialFactory] Failed to save material package"));
        return false;
    }

#else
    // Not in editor - can't create materials at runtime
    UE_LOG(LogTemp, Warning,
        TEXT("[SpellMaterialFactory] Material creation only available in Editor"));
    return false;
#endif
}