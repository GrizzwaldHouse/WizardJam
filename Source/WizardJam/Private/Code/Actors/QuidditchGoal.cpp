// QuidditchGoal.cpp
// Author: Marcus Daley
// Date: January 4, 2026
// Implementation of elemental goal post scoring system
//
// Key Implementation Details:
//   - Uses BaseProjectile::GetSpellElement() accessor (not direct property access)
//   - Creates dynamic material instances for color feedback
//   - Broadcasts scoring events via delegate (Observer pattern)
//   - Collision uses ECC_GameTraceChannel1 (must match projectile channel)

#include "Code/Actors/QuidditchGoal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Code/Actors/BaseProjectile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogQuidditchGoal);

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

    // Create root scene component for proper transform hierarchy
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(Root);

    // Create goal mesh component
    // Designer assigns actual mesh in Blueprint (hoop, ring, target, etc.)
    GoalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalMesh"));
    if (GoalMesh)
    {
        GoalMesh->SetupAttachment(RootComponent);
        GoalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Create scoring zone (overlap box in front of goal)
    // Projectiles must overlap this zone to score
    ScoringZone = CreateDefaultSubobject<UBoxComponent>(TEXT("ScoringZone"));
    if (ScoringZone)
    {
        ScoringZone->SetupAttachment(GoalMesh);
        ScoringZone->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
        ScoringZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        ScoringZone->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
        ScoringZone->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
        // Respond to WorldDynamic objects (projectiles use "OverlapAllDynamic" profile)
        ScoringZone->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
        // Also respond to pawns in case we want character-based scoring later
        ScoringZone->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
    }

    UE_LOG(LogQuidditchGoal, Log, TEXT("[QuidditchGoal] Constructor initialized"));
}

void AQuidditchGoal::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Bind overlap event (must happen after components are initialized)
    if (ScoringZone)
    {
        ScoringZone->OnComponentBeginOverlap.AddDynamic(this, &AQuidditchGoal::OnScoringZoneBeginOverlap);
        UE_LOG(LogQuidditchGoal, Log, TEXT("[%s] Scoring zone overlap delegate bound"), *GetName());
    }
}

void AQuidditchGoal::BeginPlay()
{
    Super::BeginPlay();

    // Sync TeamId with TeamID property for IGenericTeamAgentInterface
    TeamId = FGenericTeamId(TeamID);

    // Apply element color to goal mesh material
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
    // Only process BaseProjectile actors
    ABaseProjectile* Projectile = Cast<ABaseProjectile>(OtherActor);
    if (!Projectile)
    {
        return;
    }

    // Get projectile owner (who fired it) for scoring attribution
    AActor* Shooter = Projectile->GetOwner();
    if (!Shooter)
    {
        UE_LOG(LogQuidditchGoal, Warning, TEXT("[%s] Projectile '%s' has no owner - cannot award points"),
            *GetName(), *Projectile->GetName());
        return;
    }

    // Check if projectile element matches goal element
    bool bCorrectElement = IsCorrectElement(Projectile);

    // Calculate points (0 if wrong element)
    int32 Points = CalculatePoints(Shooter, bCorrectElement);

    // Get spell element name for logging and delegate
    // Using accessor method instead of direct property access
    FName ProjectileElement = Projectile->GetSpellElement();

    // Broadcast scoring event to GameMode (Observer pattern)
    OnGoalScored.Broadcast(Shooter, ProjectileElement, Points, bCorrectElement);

    // Play visual feedback
    PlayHitFeedback(bCorrectElement);

    // Log scoring result
    if (bCorrectElement)
    {
        UE_LOG(LogQuidditchGoal, Display, TEXT("[%s] === GOAL! === '%s' scored %d points with '%s'"),
            *GetName(), *Shooter->GetName(), Points, *ProjectileElement.ToString());
    }
    else
    {
        UE_LOG(LogQuidditchGoal, Display, TEXT("[%s] Wrong element! '%s' used '%s' (need '%s') - 0 points"),
            *GetName(), *Shooter->GetName(), *ProjectileElement.ToString(), *GoalElement.ToString());
    }

    // Destroy projectile after scoring attempt
    Projectile->Destroy();
}

bool AQuidditchGoal::IsCorrectElement(ABaseProjectile* Projectile) const
{
    if (!Projectile)
    {
        return false;
    }

    // Use accessor method to get projectile element
    // BaseProjectile::GetSpellElement() returns FName
    FName ProjectileElement = Projectile->GetSpellElement();

    // Match projectile element to goal element
    return ProjectileElement == GoalElement;
}

int32 AQuidditchGoal::CalculatePoints(AActor* ScoringActor, bool bCorrectElement) const
{
    if (!bCorrectElement)
    {
        return 0;
    }

    // Base points for correct element match
    int32 Points = PointsForCorrectElement;

    // Future expansion: Check if ScoringActor is in matching ElementalBoostZone
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

    // Get color for this goal's element
    CurrentColor = GetColorForElement(GoalElement);

    // Create dynamic material instance for runtime color changes
    if (GoalMesh->GetNumMaterials() > 0)
    {
        DynamicMaterial = GoalMesh->CreateDynamicMaterialInstance(0);
        if (DynamicMaterial)
        {
            // Set base and emissive colors
            // Material must have these parameter names for this to work
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

    // Flash bright on correct hit, dark on wrong hit
    FLinearColor FlashColor = bCorrectElement ? (CurrentColor * 5.0f) : FLinearColor::Black;
    DynamicMaterial->SetVectorParameterValue(FName("EmissiveColor"), FlashColor);

    // Reset color after HitFlashDuration
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
    // Colors match SpellCollectible system for visual consistency
    // Using FName comparison for designer-expandable elements

    if (Element == FName("Flame"))
    {
        return FLinearColor(0.93f, 0.11f, 0.09f, 1.0f); // Red-orange
    }
    else if (Element == FName("Ice"))
    {
        return FLinearColor(0.0f, 0.8f, 1.0f, 1.0f); // Cyan-blue
    }
    else if (Element == FName("Lightning"))
    {
        return FLinearColor(1.0f, 0.98f, 0.11f, 1.0f); // Yellow
    }
    else if (Element == FName("Arcane"))
    {
        return FLinearColor(0.6f, 0.0f, 1.0f, 1.0f); // Purple
    }

    // Default white for unknown elements
    return FLinearColor::White;
}

// ============================================================================
// IGenericTeamAgentInterface Implementation
// Allows AI to identify enemy goals using UE5 team system
// ============================================================================

FGenericTeamId AQuidditchGoal::GetGenericTeamId() const
{
    return TeamId;
}

void AQuidditchGoal::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
    TeamId = NewTeamID;
    TeamID = NewTeamID.GetId();
}