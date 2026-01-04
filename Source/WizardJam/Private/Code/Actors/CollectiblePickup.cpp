// ============================================================================
// CollectiblePickup.cpp
// Developer: Marcus Daley
// Date: December 31, 2025
// Project: WizardJam
// ============================================================================
// Implementation of spell channel granting collectible system
// ============================================================================

#include "Code/Actors/CollectiblePickup.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogCollectible, Log, All);

ACollectiblePickup::ACollectiblePickup()
    : ItemName(FName("Collectible"))
    , bShowChannelGrantLog(true)
    , bPlayerCanCollect(true)
    , bEnemyCanCollect(false)
    , bCompanionCanCollect(false)
{
    // Default: Players can collect, enemies and companions cannot
    // Override in Blueprint for different behavior
}

bool ACollectiblePickup::CanBePickedUp(AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    // Check if actor is a pawn (characters can collect, static meshes cannot)
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        UE_LOG(LogCollectible, Verbose,
            TEXT("[%s] Overlapping actor '%s' is not a Pawn - cannot collect"),
            *ItemName.ToString(), *OtherActor->GetName());
        return false;
    }

    // Check collection permissions based on team affiliation
    if (!HasCollectionPermission(OtherActor))
    {
        UE_LOG(LogCollectible, Verbose,
            TEXT("[%s] Actor '%s' lacks permission to collect this item"),
            *ItemName.ToString(), *OtherActor->GetName());
        return false;
    }

    return true;
}

void ACollectiblePickup::HandlePickup(AActor* OtherActor)
{
    if (!OtherActor)
    {
        UE_LOG(LogCollectible, Error,
            TEXT("[%s] HandlePickup called with null OtherActor!"),
            *ItemName.ToString());
        return;
    }

    // Broadcast pickup event to subscribers (GameMode, UI systems, etc.)
    OnPickedUp.Broadcast(OtherActor, this);

    // Grant spell channels if configured
    GrantChannels(OtherActor);

    UE_LOG(LogCollectible, Display,
        TEXT("[%s] Picked up by '%s'"),
        *ItemName.ToString(), *OtherActor->GetName());
}

void ACollectiblePickup::PostPickup()
{
    // Destroy the pickup actor after successful collection
    Destroy();
}

void ACollectiblePickup::GrantChannels(AActor* OtherActor)
{
    if (!OtherActor)
    {
        return;
    }

    // No channels to grant - skip processing
    if (GrantsSpellChannels.Num() == 0)
    {
        return;
    }

    // Find spell collection component on the collecting actor
    UAC_SpellCollectionComponent* SpellComp =
        OtherActor->FindComponentByClass<UAC_SpellCollectionComponent>();

    if (!SpellComp)
    {
        UE_LOG(LogCollectible, Warning,
            TEXT("[%s] Actor '%s' has no AC_SpellCollectionComponent - cannot grant channels"),
            *ItemName.ToString(), *OtherActor->GetName());
        return;
    }

    // Filter out invalid channels (NAME_None entries)
    TArray<FName> ValidChannels;
    for (const FName& Channel : GrantsSpellChannels)
    {
        if (Channel != NAME_None)
        {
            ValidChannels.Add(Channel);
        }
        else
        {
            UE_LOG(LogCollectible, Warning,
                TEXT("[%s] Skipping invalid NAME_None channel in GrantsSpellChannels array"),
                *ItemName.ToString());
        }
    }

    // Grant all valid channels
    if (ValidChannels.Num() > 0)
    {
        for (const FName& Channel : ValidChannels)
        {
            SpellComp->AddChannel(Channel);
        }

        // Log what channels were granted (if debug logging enabled)
        if (bShowChannelGrantLog)
        {
            FString ChannelList;
            for (int32 i = 0; i < ValidChannels.Num(); ++i)
            {
                ChannelList += ValidChannels[i].ToString();
                if (i < ValidChannels.Num() - 1)
                {
                    ChannelList += TEXT(", ");
                }
            }

            UE_LOG(LogCollectible, Display,
                TEXT("[%s] Granted spell channels to '%s': [%s]"),
                *ItemName.ToString(), *OtherActor->GetName(), *ChannelList);
        }
    }
    else
    {
        UE_LOG(LogCollectible, Warning,
            TEXT("[%s] No valid channels to grant (all were NAME_None)"),
            *ItemName.ToString());
    }
}

bool ACollectiblePickup::HasCollectionPermission(AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    // Check if actor implements team interface for faction identification
    IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(OtherActor);
    if (!TeamAgent)
    {
        // No team interface - default to player permission
        UE_LOG(LogCollectible, Verbose,
            TEXT("[%s] Actor '%s' has no team interface - using player permission"),
            *ItemName.ToString(), *OtherActor->GetName());
        return bPlayerCanCollect;
    }

    // Get team ID
    FGenericTeamId TeamID = TeamAgent->GetGenericTeamId();
    uint8 TeamIDValue = TeamID.GetId();

    // Check permissions based on team affiliation
    // Team 0 = Player
    // Team 1 = Enemy
    // Team 2 = Companion/Friendly
    switch (TeamIDValue)
    {
    case 0: // Player team
        return bPlayerCanCollect;

    case 1: // Enemy team
        return bEnemyCanCollect;

    case 2: // Companion team
        return bCompanionCanCollect;

    default:
        // Unknown team - deny collection
        UE_LOG(LogCollectible, Warning,
            TEXT("[%s] Unknown team ID %d for actor '%s' - denying collection"),
            *ItemName.ToString(), TeamIDValue, *OtherActor->GetName());
        return false;
    }
}