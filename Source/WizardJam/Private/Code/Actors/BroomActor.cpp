// BroomActor.cpp
// World-placeable broom trigger for flight unlock
//
// Developer: Marcus Daley
// Date: January 1, 2026
//
// This actor is just a trigger/unlock mechanism (like Unity weapon pickup collider).
// It does NOT move, does NOT attach to player, does NOT become the flying broom.
// It simply enables AC_BroomComponent on the player, which handles all flight logic.

#include "Code/Actors/BroomActor.h"
#include "Code/Utility/AC_BroomComponent.h"
#include "Code/Utility/AC_SpellCollectionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogBroomActor, Log, All);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ABroomActor::ABroomActor()
    : BroomName(FText::FromString("Combat Broom"))
    , RequiredChannel(FName("BroomFlight"))
    , InteractionRange(300.0f)
    , MountSocketName(FName("MountSocket"))
{
    PrimaryActorTick.bCanEverTick = false;

    // Create broom mesh component (visual only - stays in world)
    BroomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BroomMesh"));
    RootComponent = BroomMesh;

    // QueryOnly collision (allows interaction raycast, no physics)
    BroomMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BroomMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    BroomMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    BroomMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void ABroomActor::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogBroomActor, Log,
        TEXT("[BroomActor] %s initialized at location %s - Required Channel: %s"),
        *GetName(),
        *GetActorLocation().ToString(),
        *RequiredChannel.ToString());
}

// ============================================================================
// IINTERACTABLE INTERFACE IMPLEMENTATION
// ============================================================================

FText ABroomActor::GetTooltipText_Implementation() const
{
    return BroomName;
}

FText ABroomActor::GetInteractionPrompt_Implementation() const
{
    return FText::FromString("Press E to Mount Broom");
}

FText ABroomActor::GetDetailedInfo_Implementation() const
{
    return FText::GetEmpty();
}

bool ABroomActor::CanInteract_Implementation() const
{
    // Real check happens in OnInteract
    return true;
}

void ABroomActor::OnInteract_Implementation(AActor* Interactor)
{
    if (!Interactor)
    {
        UE_LOG(LogBroomActor, Warning,
            TEXT("[BroomActor] OnInteract called with null interactor!"));
        return;
    }

    // Check channel requirement
    if (!HasRequiredChannel(Interactor))
    {
        UE_LOG(LogBroomActor, Log,
            TEXT("[BroomActor] Player %s lacks required channel: %s"),
            *Interactor->GetName(), *RequiredChannel.ToString());
        return;
    }

    // Enable player flight
    EnablePlayerFlight(Interactor);

    UE_LOG(LogBroomActor, Log,
        TEXT("[BroomActor] Player %s activated flight trigger at %s"),
        *Interactor->GetName(), *GetName());
}

float ABroomActor::GetInteractionRange_Implementation() const
{
    return InteractionRange;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool ABroomActor::HasRequiredChannel(AActor* Interactor) const
{
    if (!Interactor)
    {
        return false;
    }

    UAC_SpellCollectionComponent* SpellCollection =
        Interactor->FindComponentByClass<UAC_SpellCollectionComponent>();

    if (!SpellCollection)
    {
        UE_LOG(LogBroomActor, Warning,
            TEXT("[BroomActor] Player %s has no SpellCollectionComponent"),
            *Interactor->GetName());
        return false;
    }

    bool bHasChannel = SpellCollection->HasChannel(RequiredChannel);

    if (!bHasChannel)
    {
        UE_LOG(LogBroomActor, Log,
            TEXT("[BroomActor] Player %s missing channel: %s"),
            *Interactor->GetName(), *RequiredChannel.ToString());
    }

    return bHasChannel;
}

void ABroomActor::EnablePlayerFlight(AActor* Player)
{
    if (!Player)
    {
        return;
    }

    // Find broom component on player
    UAC_BroomComponent* BroomComponent = Player->FindComponentByClass<UAC_BroomComponent>();

    if (!BroomComponent)
    {
        UE_LOG(LogBroomActor, Error,
            TEXT("[BroomActor] Player %s has no AC_BroomComponent!"),
            *Player->GetName());
        return;
    }

    // Enable flight (component handles spawn, attach, movement mode, input context)
    BroomComponent->SetFlightEnabled(true);

    UE_LOG(LogBroomActor, Display,
        TEXT("[BroomActor] Flight enabled for player %s"),
        *Player->GetName());
}

void ABroomActor::MountPlayer(AActor* Player)
{
    // ========================================================================
    // THIS FUNCTION DOES NOTHING
    // ========================================================================

    // World broom actor is just a trigger (like Unity weapon pickup collider).
    // It stays in the world at its original location.
    // AC_BroomComponent handles:
    //   - Spawning broom visual actor
    //   - Attaching broom to player's MountSocket
    //   - Switching movement mode to MOVE_Flying
    //   - Adding flight input context
    //   - Managing stamina drain
    //   - Destroying broom on dismount

    // This function exists for interface compatibility but does nothing.
    // Designer workflow: Place this actor in level, configure RequiredChannel.
    // Player interacts -> EnablePlayerFlight() -> AC_BroomComponent does the rest.
}