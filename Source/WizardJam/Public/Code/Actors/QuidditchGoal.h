// QuidditchGoal.h
// Author: Marcus Daley
// Date: January 1, 2026
// Purpose: Elemental goal post that awards points when hit by matching spell projectile
// 
// Designer Usage:
//   1. Place BP_QuidditchGoal_Flame/Ice/Lightning/Arcane in level
//   2. Set GoalElement to match desired spell type
//   3. Set TeamID: 0 = Player goals (AI attacks these), 1 = AI goals (Player attacks these)
//   4. Configure PointsForCorrectElement (default 10)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utility/SpellChannelTypes.h"
#include "GenericTeamAgentInterface.h"
#include "QuidditchGoal.generated.h"

// Forward declarations
class UBoxComponent;
class UStaticMeshComponent;
class ABaseProjectile;

DECLARE_LOG_CATEGORY_EXTERN(LogQuidditchGoal, Log, All);

// Delegate broadcast when goal is scored
// Parameters: ScoringActor (who scored), Element (spell type), PointsAwarded, bWasCorrectElement
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnGoalScored,
    AActor*, ScoringActor,
    FName, Element,
    int32, PointsAwarded,
    bool, bWasCorrectElement
);

UCLASS()
class WIZARDJAM_API AQuidditchGoal : public AActor, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AQuidditchGoal();

    //////////////////////////////////////////////////////////////////////////
    // Designer Configuration
    //////////////////////////////////////////////////////////////////////////

    // Which element this goal accepts for points
    UPROPERTY(EditInstanceOnly, Category = "Goal|Element")
    FName GoalElement;

    // Team this goal belongs to
    // 0 = Player team (AI will attack these goals)
    // 1 = AI team (Player will attack these goals)
    UPROPERTY(EditInstanceOnly, Category = "Goal|Team")
    int32 TeamID;

    // Points awarded when correct element hits
    UPROPERTY(EditDefaultsOnly, Category = "Goal|Scoring")
    int32 PointsForCorrectElement;

    // Bonus multiplier if player is in matching boost zone
    UPROPERTY(EditDefaultsOnly, Category = "Goal|Scoring")
    float BonusPointsMultiplier;

    // Visual feedback duration when goal is hit
    UPROPERTY(EditDefaultsOnly, Category = "Goal|Feedback")
    float HitFlashDuration;

    //////////////////////////////////////////////////////////////////////////
    // Components
    //////////////////////////////////////////////////////////////////////////

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* GoalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* ScoringZone;

    //////////////////////////////////////////////////////////////////////////
    // Runtime State
    //////////////////////////////////////////////////////////////////////////

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Runtime")
    FLinearColor CurrentColor;

    //////////////////////////////////////////////////////////////////////////
    // Events
    //////////////////////////////////////////////////////////////////////////

    UPROPERTY(BlueprintAssignable, Category = "Goal|Events")
    FOnGoalScored OnGoalScored;

    //////////////////////////////////////////////////////////////////////////
    // IGenericTeamAgentInterface
    //////////////////////////////////////////////////////////////////////////

    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;

protected:
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;

private:
    UFUNCTION()
    void OnScoringZoneBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    bool IsCorrectElement(ABaseProjectile* Projectile) const;
    int32 CalculatePoints(AActor* ScoringActor, bool bCorrectElement) const;
    void ApplyElementColor();
    void PlayHitFeedback(bool bCorrectElement);
    FLinearColor GetColorForElement(FName Element) const;

    FGenericTeamId TeamId;

    UPROPERTY()
    UMaterialInstanceDynamic* DynamicMaterial;

    FTimerHandle HitFlashTimer;
};