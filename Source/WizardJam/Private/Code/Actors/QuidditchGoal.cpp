// QuidditchGoal.cpp
// Author: Marcus Daley
// Date: January 1, 2026
// Implementation of elemental goal post scoring system

#include "Actor/QuidditchGoal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BaseProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuidditchGoal, Log, All);

AQuidditchGoal::AQuidditchGoal()
    : GoalElement(NAME_None)
    , TeamID(0)
    , PointsForCorrectElement(10)
    , BonusPointsMultiplier(1.0f)
    , HitFlashDuration(0.5f)
    , CurrentColor(FLinearColor::White)
    , TeamId(FGenericTeamId(0))
    , DynamicMaterial(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root scene component
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(Root);

    // Create goal mesh
    // Designer will set mesh in Blueprint to hoop/ring/target model
    GoalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMesh"));
    GoalMesh->SetupAttachment(RootComponent);
    GoalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create scoring zone (overlap box in front of goal)
    ScoringZone = CreateDefaultSubobject<UBoxComponent>(TEXT("ScoringZone"));
    ScoringZone->SetupAttachment(GoalMesh);
    ScoringZone->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    ScoringZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ScoringZone->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    ScoringZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    ScoringZone->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1, ECollisionResponse::ECR_Overlap);

    UE_LOG(LogQuidditchGoal, Log, TEXT("[QuidditchGoal] Constructor initialized"));
}

void AQuidditchGoal::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Bind overlap event
    if (ScoringZone)
    {
        ScoringZone->OnComponentBeginOverlap.AddDynamic(this, &AQuidditchGoal::OnScoringZoneBeginOverlap);
        UE_LOG(LogQuidditchGoal, Log, TEXT("[%s] Scoring zone overlap bound"), *GetName());
    }
}

void AQuidditchGoal::BeginPlay()
{
    Super::BeginPlay();

    // Set team ID
    TeamId = FGenericTeamId(TeamID);

    // Apply element color to goal
    ApplyElementColor();

    UE_LOG(LogQuidditchGoal, Display, TEXT("[%s] Goal ready | Element: '%s' | Team: %d | Points: %d"),
        *GetName(), *GoalElement.ToString(), TeamID, PointsForCorrectElement);
}

void AQuidditchGoal::OnScoringZoneBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // Only process projectiles
    ABaseProjectile* Projectile = Cast<ABaseProjectile>(OtherActor);
    if (!Projectile)
    {
        return;
    }

    // Get projectile owner (who shot it)
    AActor* Shooter = Projectile->GetOwner();
    if (!Shooter)
    {
        UE_LOG(LogQuidditchGoal, Warning, TEXT("[%s] Projectile has no owner - cannot award points"), *GetName());
        return;
    }

    // Check if element matches
    bool bCorrectElement = IsCorrectElement(Projectile);

    // Calculate points
    int32 Points = CalculatePoints(Shooter, bCorrectElement);

    // Broadcast scoring event to GameMode
    OnGoalScored.Broadcast(Shooter, Projectile->SpellType, Points, bCorrectElement);

    // Play visual feedback
    PlayHitFeedback(bCorrectElement);

    // Log scoring
    if (bCorrectElement)
    {
        UE_LOG(LogQuidditchGoal, Display, TEXT("[%s] GOAL! '%s' scored %d points with '%s'"),
            *GetName(), *Shooter->GetName(), Points, *Projectile->SpellType.ToString());
    }
    else
    {
        UE_LOG(LogQuidditchGoal, Display, TEXT("[%s] Wrong element! '%s' used '%s' (need '%s') - 0 points"),
            *GetName(), *Shooter->GetName(), *Projectile->SpellType.ToString(), *GoalElement.ToString());
    }

    // Destroy projectile after scoring
    Projectile->Destroy();
}

bool AQuidditchGoal::IsCorrectElement(ABaseProjectile* Projectile) const
{
    if (!Projectile)
    {
        return false;
    }

    // Match spell type to goal element
    return Projectile->SpellType == GoalElement;
}

int32 AQuidditchGoal::CalculatePoints(AActor* ScoringActor, bool bCorrectElement) const
{
    if (!bCorrectElement)
    {
        return 0;
    }

    // Base points for correct element
    int32 Points = PointsForCorrectElement;

    // Future expansion: Check if ScoringActor is in matching boost zone
    // If so, multiply points by BonusPointsMultiplier
    // For now, just return base points

    return Points;
}

void AQuidditchGoal::ApplyElementColor()
{
    if (!GoalMesh)
    {
        return;
    }

    // Get color for element
    CurrentColor = GetColorForElement(GoalElement);

    // Create dynamic material instance if needed
    if (GoalMesh->GetNumMaterials() > 0)
    {
        DynamicMaterial = GoalMesh->CreateDynamicMaterialInstance(0);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), CurrentColor);
            DynamicMaterial->SetVectorParameterValue(FName("EmissiveColor"), CurrentColor * 2.0f);
            UE_LOG(LogQuidditchGoal, Log, TEXT("[%s] Applied color (R=%.2f G=%.2f B=%.2f) to material"),
                *GetName(), CurrentColor.R, CurrentColor.G, CurrentColor.B);
        }
    }
}

void AQuidditchGoal::PlayHitFeedback(bool bCorrectElement)
{
    if (!DynamicMaterial)
    {
        return;
    }

    // Flash bright color on correct hit, dark on wrong hit
    FLinearColor FlashColor = bCorrectElement ? (CurrentColor * 5.0f) : FLinearColor::Black;
    DynamicMaterial->SetVectorParameterValue(FName("EmissiveColor"), FlashColor);

    // Reset color after duration
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this]()
        {
            if (DynamicMaterial)
            {
                DynamicMaterial->SetVectorParameterValue(FName("EmissiveColor"), CurrentColor * 2.0f);
            }
        });

    GetWorldTimerManager().SetTimer(HitFlashTimer, TimerDelegate, HitFlashDuration, false);
}

FLinearColor AQuidditchGoal::GetColorForElement(FName Element) const
{
    // Match colors from SpellCollectible system
    if (Element == FName("Flame"))
    {
        return FLinearColor(0.93f, 0.11f, 0.09f, 1.0f);
    }
    else if (Element == FName("Ice"))
    {
        return FLinearColor(0.0f, 0.8f, 1.0f, 1.0f);
    }
    else if (Element == FName("Lightning"))
    {
        return FLinearColor(1.0f, 0.98f, 0.11f, 1.0f);
    }
    else if (Element == FName("Arcane"))
    {
        return FLinearColor(0.6f, 0.0f, 1.0f, 1.0f);
    }

    // Default white for unknown elements
    return FLinearColor::White;
}

FGenericTeamId AQuidditchGoal::GetGenericTeamId() const
{
    return TeamId;
}

void AQuidditchGoal::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
    TeamId = NewTeamID;
    TeamID = NewTeamID.GetId();
}