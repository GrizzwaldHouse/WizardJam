// ============================================================================
// ISpellCollector.h
// Developer: Marcus Daley
// Date: December 26, 2025
// ============================================================================
// Purpose:
// Interface that identifies actors capable of collecting spells. Any actor
// implementing this interface can pick up SpellCollectible items without
// requiring specific class inheritance.
//
// Why Use an Interface:
// The previous system required casting to ABaseCharacter, which fails if:
// - Your player inherits from a different base class
// - The inheritance chain has gaps
// - You want non-character actors to collect spells
//
// With this interface approach:
// - ANY actor class can implement ISpellCollector
// - No cast failures - just check if actor implements interface
// - Works with Characters, Pawns, or even non-pawn actors
// - Component does the heavy lifting, interface just identifies participants
//
// Usage:
// 1. Add ISpellCollector to your class inheritance list
// 2. Add UAC_SpellCollectionComponent to your actor
// 3. Implement GetSpellCollectionComponent() to return that component
// 4. SpellCollectible will automatically detect and use it
//
// Example Implementation:
// class AMyPlayer : public ACharacter, public ISpellCollector
// {
//     UAC_SpellCollectionComponent* SpellComponent;
//     virtual UAC_SpellCollectionComponent* GetSpellCollectionComponent_Implementation() const override
//     {
//         return SpellComponent;
//     }
// };
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISpellCollector.generated.h"

class UAC_SpellCollectionComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class USpellCollector : public UInterface
{
    GENERATED_BODY()
};

// ============================================================================
// ISpellCollector - Actual Interface Implementation
// Actors implement THIS class to participate in spell collection
// ============================================================================
class WIZARDJAM_API ISpellCollector
{
    GENERATED_BODY()

public:
    // ========================================================================
    // Required Implementation
    // Your actor MUST implement this function
    // ========================================================================

    // Returns the spell collection component attached to this actor
    // SpellCollectible calls this to access spell tracking functionality
    // Return nullptr if this actor cannot collect spells right now
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spells")
    UAC_SpellCollectionComponent* GetSpellCollectionComponent() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spells")
    void OnSpellCollected(FName SpellType);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spells")
    void OnSpellCollectionDenied(FName SpellType, const FString& Reason);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spells")
    bool CanCollectSpells() const;

    // Get team ID for collector type filtering
    // Default: Returns 0 (player team)
    // Override to return correct team for enemies (1) or companions (2)
    //
    // Team ID Convention:
    // 0 = Player
    // 1 = Enemy
    // 2 = Companion/Ally
    // 3+ = Custom factions
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spells")
    int32 GetCollectorTeamID() const;
};