// BasePickup.cpp
// Base pickup implementation
//
// Developer: Marcus Daley
// Date: December 21, 2025
// Project: WizardJam

#include "Code/Actors/BasePickup.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogBasePickup, Log, All);

ABasePickup::ABasePickup()
    : bUseMesh(true)
    , PickupMaterial(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;

    // Create collision box as root
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    SetRootComponent(CollisionBox);
    CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
    CollisionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionBox->SetGenerateOverlapEvents(true);

    // Create mesh component
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Default cube mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
        MeshComponent->SetRelativeScale3D(FVector(0.5f));
    }
}

void ABasePickup::BeginPlay()
{
    Super::BeginPlay();

    // Bind overlap event
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABasePickup::OnOverlapBegin);

    // Configure mesh visibility
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(bUseMesh);
        if (bUseMesh && PickupMaterial)
        {
            ApplyMaterialToAllSlots();
        }
    }

    UE_LOG(LogBasePickup, Display, TEXT("[%s] Pickup ready at %s"),
        *GetName(), *GetActorLocation().ToString());
}

void ABasePickup::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    if (CanBePickedUp(OtherActor))
    {
        UE_LOG(LogBasePickup, Display, TEXT("[%s] Picked up by %s"),
            *GetName(), *OtherActor->GetName());

        HandlePickup(OtherActor);
        PostPickup();
    }
}

bool ABasePickup::CanBePickedUp(AActor* OtherActor) const
{
    // Default: only player can pick up
    return OtherActor && OtherActor->ActorHasTag(TEXT("Player"));
}

void ABasePickup::HandlePickup(AActor* OtherActor)
{
    // Override in child classes
    UE_LOG(LogBasePickup, Warning, TEXT("[%s] HandlePickup not overridden!"), *GetName());
}

void ABasePickup::PostPickup()
{
    // Default behavior: destroy pickup
    Destroy();
}

UBoxComponent* ABasePickup::GetCollisionBox() const
{
    return CollisionBox;
}

UStaticMeshComponent* ABasePickup::GetMeshComponent() const
{
    return MeshComponent;
}

void ABasePickup::ApplyMaterialToAllSlots()
{
    if (MeshComponent && PickupMaterial)
    {
        int32 NumMaterials = MeshComponent->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; ++i)
        {
            MeshComponent->SetMaterial(i, PickupMaterial);
        }
    }
}