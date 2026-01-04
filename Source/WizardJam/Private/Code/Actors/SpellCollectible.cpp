// ============================================================================
// SpellCollectible.cpp
// Developer: Marcus Daley
// Date: December 25, 2025
// ============================================================================
// Purpose:
// Implementation of spell collectible using interface-based detection.
// This solves the cast failure issue by using ISpellCollector interface
// instead of casting to a specific base class.
//
// Pickup Flow:
// 1. Actor overlaps with collectible
// 2. CanBePickedUp() checks:
//    a. Does actor implement ISpellCollector? (interface check, NOT cast)
//    b. Can we get SpellCollectionComponent from interface?
//    c. Is collection enabled on that component?
//    d. Does actor pass team filter? (player/enemy/companion)
//    e. Does actor meet channel requirements?
// 3. If ANY check fails: HandleDenied() fires with specific reason
// 4. If ALL checks pass: HandlePickup() runs:
//    a. Grants channels to component
//    b. Adds spell to component
//    c. Broadcasts to static delegate (GameMode)
//    d. Broadcasts to instance delegate (Blueprint VFX)
//
// Why This Works When Casting Failed:
// - UObject::Implements<ISpellCollector>() works on ANY class
// - No inheritance hierarchy dependency
// - Component lookup is class-agnostic
// - Works even if Blueprint reparented to different base class
// ============================================================================

#include "Code/Actors/SpellCollectible.h"
#include "Code/Utility/ISpellCollector.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/AudioComponent.h"

DEFINE_LOG_CATEGORY(LogSpellCollectible);

// Static delegate definition
FOnSpellPickedUpGlobal ASpellCollectible::OnAnySpellPickedUp;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ASpellCollectible::ASpellCollectible()
    : SpellTypeName(NAME_None)
    , SpellColor(FLinearColor::White)
    , bRequireAllChannels(true)
    , DeniedSound(nullptr)
    , DeniedMessage(TEXT("Cannot collect: {reason}"))
    , ProjectColorableMaterial(nullptr)
    , EngineColorableMaterial(nullptr)
{
    // Color parameter names to try (in priority order)
    ColorParameterNames.Add(FName("Color"));
    ColorParameterNames.Add(FName("BaseColor"));
    ColorParameterNames.Add(FName("Base Color"));
    ColorParameterNames.Add(FName("Tint"));
    ColorParameterNames.Add(FName("TintColor"));
    ColorParameterNames.Add(FName("Emissive"));
    ColorParameterNames.Add(FName("EmissiveColor"));
    ColorParameterNames.Add(FName("Albedo"));

    // NOTE: ProjectColorableMaterial and EngineColorableMaterial initialized to nullptr
    // Designer can optionally set these in Blueprint Class Defaults if needed
    // The material system will try mesh materials first, then fall back to these
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ASpellCollectible::BeginPlay()
{
    Super::BeginPlay();

    SetupSpellAppearance();

    // Log configuration for debugging
    UE_LOG(LogSpellCollectible, Display,
        TEXT("[%s] Spell '%s' ready | Collectors: Player=%s Enemy=%s Companion=%s"),
        *GetName(),
        *SpellTypeName.ToString(),
        bPlayerCanCollect ? TEXT("YES") : TEXT("NO"),
        bEnemyCanCollect ? TEXT("YES") : TEXT("NO"),
        bCompanionCanCollect ? TEXT("YES") : TEXT("NO"));

    if (RequiredChannels.Num() > 0)
    {
        FString ChannelStr;
        for (const FName& Ch : RequiredChannels)
        {
            if (!ChannelStr.IsEmpty()) ChannelStr += TEXT(", ");
            ChannelStr += Ch.ToString();
        }
        UE_LOG(LogSpellCollectible, Display,
            TEXT("[%s] Required channels: [%s] (%s logic)"),
            *GetName(), *ChannelStr,
            bRequireAllChannels ? TEXT("AND") : TEXT("OR"));
    }
}

// ============================================================================
// HELPER: Get SpellCollectionComponent via Interface
// This is the KEY function that avoids cast failures
// ============================================================================

UAC_SpellCollectionComponent* ASpellCollectible::GetCollectorComponent(AActor* Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    // Check if actor implements the ISpellCollector interface
    // This works on ANY clas
    if (!Actor->Implements<USpellCollector>())
    {
        UE_LOG(LogSpellCollectible, Verbose,
            TEXT("[%s] Actor '%s' does not implement ISpellCollector interface"),
            *GetName(), *Actor->GetName());
        return nullptr;
    }

    // Get the component through the interface
    // Execute_ is the proper way to call BlueprintNativeEvent functions
    UAC_SpellCollectionComponent* Component =
        ISpellCollector::Execute_GetSpellCollectionComponent(Actor);

    if (!Component)
    {
        UE_LOG(LogSpellCollectible, Warning,
            TEXT("[%s] Actor '%s' implements ISpellCollector but returned null component"),
            *GetName(), *Actor->GetName());
    }

    return Component;
}

// ============================================================================
// HELPER: Check Team Filter
// ============================================================================

bool ASpellCollectible::CheckTeamFilter(int32 TeamID) const
{
    switch (TeamID)
    {
    case 0: return bPlayerCanCollect;
    case 1: return bEnemyCanCollect;
    case 2: return bCompanionCanCollect;
    default:
        UE_LOG(LogSpellCollectible, Warning,
            TEXT("[%s] Unknown TeamID %d - denying by default"),
            *GetName(), TeamID);
        return false;
    }
}

// ============================================================================
// REQUIREMENT CHECKING
// ============================================================================

bool ASpellCollectible::CanActorCollect(AActor* Actor) const
{
    // Step 1: Get the component (this handles interface check)
    UAC_SpellCollectionComponent* SpellComp = GetCollectorComponent(Actor);
    if (!SpellComp)
    {
        return false;
    }

    // Step 2: Check if collection is enabled
    if (!SpellComp->IsCollectionEnabled())
    {
        return false;
    }

    // Step 3: Check team filter
    if (!IsAllowedCollectorType(Actor))
    {
        return false;
    }

    // Step 4: Check channel requirements
    if (!MeetsChannelRequirements(Actor))
    {
        return false;
    }

    return true;
}

bool ASpellCollectible::IsAllowedCollectorType(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    // Check if implements interface
    if (!Actor->Implements<USpellCollector>())
    {
        return false;
    }

    // Get team ID through interface
    int32 TeamID = ISpellCollector::Execute_GetCollectorTeamID(Actor);

    return CheckTeamFilter(TeamID);
}

bool ASpellCollectible::MeetsChannelRequirements(AActor* Actor) const
{
    // No requirements = always passes
    if (RequiredChannels.Num() == 0)
    {
        return true;
    }

    UAC_SpellCollectionComponent* SpellComp = GetCollectorComponent(Actor);
    if (!SpellComp)
    {
        return false;
    }

    if (bRequireAllChannels)
    {
        // AND logic: Must have ALL
        for (const FName& Channel : RequiredChannels)
        {
            if (Channel != NAME_None && !SpellComp->HasChannel(Channel))
            {
                return false;
            }
        }
        return true;
    }
    else
    {
        // OR logic: Need at least ONE
        for (const FName& Channel : RequiredChannels)
        {
            if (Channel != NAME_None && SpellComp->HasChannel(Channel))
            {
                return true;
            }
        }
        return false;
    }
}

TArray<FName> ASpellCollectible::GetMissingChannels(AActor* Actor) const
{
    TArray<FName> Missing;

    UAC_SpellCollectionComponent* SpellComp = GetCollectorComponent(Actor);
    if (!SpellComp)
    {
        return RequiredChannels;  // All are missing if no component
    }

    for (const FName& Channel : RequiredChannels)
    {
        if (Channel != NAME_None && !SpellComp->HasChannel(Channel))
        {
            Missing.Add(Channel);
        }
    }

    return Missing;
}

// ============================================================================
// PICKUP LOGIC
// ============================================================================

bool ASpellCollectible::CanBePickedUp(AActor* OtherActor) const
{
    // Check parent class first (overlap, basic requirements)
    if (!Super::CanBePickedUp(OtherActor))
    {
        return false;
    }

    // Step 1: Check for interface implementation
    // This is the KEY difference from the old system - no cast to BaseCharacter!
    if (!OtherActor->Implements<USpellCollector>())
    {
        UE_LOG(LogSpellCollectible, Log,
            TEXT("[%s] '%s' does not implement ISpellCollector - cannot collect"),
            *GetName(), *OtherActor->GetName());
        return false;
    }

    // Step 2: Get the component through interface
    UAC_SpellCollectionComponent* SpellComp = GetCollectorComponent(OtherActor);
    if (!SpellComp)
    {
        const_cast<ASpellCollectible*>(this)->HandleDenied(
            OtherActor,
            TEXT("No SpellCollectionComponent found"),
            NAME_None);
        return false;
    }

    // Step 3: Check if collection is enabled
    if (!SpellComp->IsCollectionEnabled())
    {
        UE_LOG(LogSpellCollectible, Log,
            TEXT("[%s] '%s' has spell collection disabled"),
            *GetName(), *OtherActor->GetName());

        const_cast<ASpellCollectible*>(this)->HandleDenied(
            OtherActor,
            TEXT("Spell collection is disabled"),
            NAME_None);
        return false;
    }

    // Step 4: Check team filter
    int32 TeamID = ISpellCollector::Execute_GetCollectorTeamID(OtherActor);
    if (!CheckTeamFilter(TeamID))
    {
        UE_LOG(LogSpellCollectible, Log,
            TEXT("[%s] '%s' (Team %d) not in allowed collector types"),
            *GetName(), *OtherActor->GetName(), TeamID);

        const_cast<ASpellCollectible*>(this)->HandleDenied(
            OtherActor,
            TEXT("This character type cannot collect this spell"),
            NAME_None);
        return false;
    }

    // Step 5: Check channel requirements
    if (!MeetsChannelRequirements(OtherActor))
    {
        TArray<FName> Missing = GetMissingChannels(OtherActor);
        FName FirstMissing = Missing.Num() > 0 ? Missing[0] : NAME_None;

        UE_LOG(LogSpellCollectible, Log,
            TEXT("[%s] '%s' missing required channel '%s'"),
            *GetName(), *OtherActor->GetName(), *FirstMissing.ToString());

        const_cast<ASpellCollectible*>(this)->HandleDenied(
            OtherActor,
            FString::Printf(TEXT("Requires: %s"), *FirstMissing.ToString()),
            FirstMissing);
        return false;
    }

    // All checks passed!
    UE_LOG(LogSpellCollectible, Log,
        TEXT("[%s] '%s' passed all requirements - pickup allowed"),
        *GetName(), *OtherActor->GetName());

    return true;
}

void ASpellCollectible::HandlePickup(AActor* OtherActor)
{
    // Call parent first
    Super::HandlePickup(OtherActor);

    // Validate spell type
    if (SpellTypeName == NAME_None)
    {
        UE_LOG(LogSpellCollectible, Warning,
            TEXT("[%s] SpellTypeName not set! Designer must configure this."),
            *GetName());
        return;
    }

    // Get component through interface
    UAC_SpellCollectionComponent* SpellComp = GetCollectorComponent(OtherActor);
    if (!SpellComp)
    {
        UE_LOG(LogSpellCollectible, Error,
            TEXT("[%s] HandlePickup called but no SpellCollectionComponent found!"),
            *GetName());
        return;
    }

    // Grant channels first (so spell can check them if needed)
    GrantChannelsToCollector(SpellComp);

    // Add the spell to the collector's component
    bool bAdded = SpellComp->AddSpell(SpellTypeName);

    // Get team for logging
    int32 TeamID = ISpellCollector::Execute_GetCollectorTeamID(OtherActor);

    UE_LOG(LogSpellCollectible, Display,
        TEXT("[%s] === SPELL COLLECTED === Type: '%s' | Collector: '%s' (Team %d) | New: %s"),
        *GetName(),
        *SpellTypeName.ToString(),
        *OtherActor->GetName(),
        TeamID,
        bAdded ? TEXT("YES") : TEXT("ALREADY HAD"));

    // Broadcast to static delegate (GameMode global tracking)
    OnAnySpellPickedUp.Broadcast(SpellTypeName, OtherActor, this);

    // Broadcast to instance delegate (Blueprint VFX/SFX)
    OnSpellPickedUp.Broadcast(SpellTypeName, OtherActor);

    // Notify the collector through interface (custom actor feedback)
    ISpellCollector::Execute_OnSpellCollected(OtherActor, SpellTypeName);
}

void ASpellCollectible::GrantChannelsToCollector(UAC_SpellCollectionComponent* SpellComp)
{
    if (!SpellComp || GrantsChannels.Num() == 0)
    {
        return;
    }

    TArray<FName> Granted;
    for (const FName& Channel : GrantsChannels)
    {
        if (Channel != NAME_None)
        {
            SpellComp->AddChannel(Channel);
            Granted.Add(Channel);
        }
    }

    if (Granted.Num() > 0)
    {
        FString GrantedStr;
        for (const FName& Ch : Granted)
        {
            if (!GrantedStr.IsEmpty()) GrantedStr += TEXT(", ");
            GrantedStr += Ch.ToString();
        }
        UE_LOG(LogSpellCollectible, Display,
            TEXT("[%s] Granted channels: [%s]"),
            *GetName(), *GrantedStr);
    }
}

void ASpellCollectible::HandleDenied(AActor* Actor, const FString& Reason, FName MissingRequirement)
{
    // Format message
    FString Message = DeniedMessage;
    Message = Message.Replace(TEXT("{reason}"), *Reason);

    // Play sound if configured
    if (DeniedSound)
    {
        // Spawn one-shot audio component (alternative to GameplayStatics)
        UAudioComponent* AudioComp = NewObject<UAudioComponent>(this);
        if (AudioComp)
        {
            AudioComp->SetSound(DeniedSound);
            AudioComp->SetWorldLocation(GetActorLocation());
            AudioComp->bAutoDestroy = true;
            AudioComp->Play();
        }
    }

    UE_LOG(LogSpellCollectible, Log,
        TEXT("[%s] Pickup DENIED for '%s' | %s"),
        *GetName(), *Actor->GetName(), *Message);

    // Broadcast to instance delegate
    OnPickupDenied.Broadcast(Actor, MissingRequirement, Message);

    // Notify actor through interface
    if (Actor->Implements<USpellCollector>())
    {
        ISpellCollector::Execute_OnSpellCollectionDenied(Actor, SpellTypeName, Message);
    }
}

// ============================================================================
// MATERIAL/COLOR SYSTEM
// ============================================================================

void ASpellCollectible::SetupSpellAppearance()
{
    UStaticMeshComponent* MeshComp = FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        UE_LOG(LogSpellCollectible, Warning,
            TEXT("[%s] No StaticMeshComponent - cannot apply color"),
            *GetName());
        return;
    }

    int32 NumMaterials = MeshComp->GetNumMaterials();
    if (NumMaterials == 0)
    {
        return;
    }

    DynamicMaterials.Empty();
    int32 SuccessCount = 0;

    for (int32 Slot = 0; Slot < NumMaterials; Slot++)
    {
        bool bApplied = false;
        FName WorkingParam = NAME_None;

        // Try mesh's current material first
        UMaterialInterface* CurrentMat = MeshComp->GetMaterial(Slot);
        if (CurrentMat)
        {
            bApplied = TryApplyColorToMaterial(CurrentMat, MeshComp, Slot, WorkingParam);
        }

        // Try project material
        if (!bApplied && ProjectColorableMaterial)
        {
            bApplied = TryApplyColorToMaterial(ProjectColorableMaterial, MeshComp, Slot, WorkingParam);
        }

        // Try engine material
        if (!bApplied && EngineColorableMaterial)
        {
            bApplied = TryApplyColorToMaterial(EngineColorableMaterial, MeshComp, Slot, WorkingParam);
        }

        if (bApplied)
        {
            SuccessCount++;
        }
    }

    UE_LOG(LogSpellCollectible, Display,
        TEXT("[%s] Applied color (R=%.2f G=%.2f B=%.2f) to %d/%d slots"),
        *GetName(), SpellColor.R, SpellColor.G, SpellColor.B,
        SuccessCount, NumMaterials);
}

bool ASpellCollectible::TryApplyColorToMaterial(UMaterialInterface* BaseMaterial,
    UStaticMeshComponent* MeshComp, int32 SlotIndex, FName& OutWorkingParam)
{
    if (!BaseMaterial || !MeshComp)
    {
        return false;
    }

    UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    if (!DynMat)
    {
        return false;
    }

    OutWorkingParam = FindWorkingColorParameter(DynMat);
    if (OutWorkingParam == NAME_None)
    {
        return false;
    }

    DynMat->SetVectorParameterValue(OutWorkingParam, SpellColor);
    MeshComp->SetMaterial(SlotIndex, DynMat);
    DynamicMaterials.Add(DynMat);

    return true;
}

FName ASpellCollectible::FindWorkingColorParameter(UMaterialInterface* Material) const
{
    if (!Material)
    {
        return NAME_None;
    }

    for (const FName& ParamName : ColorParameterNames)
    {
        FLinearColor TestColor;
        if (Material->GetVectorParameterValue(ParamName, TestColor))
        {
            return ParamName;
        }
    }

    return NAME_None;
}