// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GomokuRuleEngine.h"
#include "GomokuItemTypes.h"
#include "GomokuItemLibrary.generated.h"

UCLASS()
class O_MOCK_API UGomokuItemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxEnergy = 5;

	/**
	 * Returns true if ItemId is a known/registered item in the static registry (1..5).
	 */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	static bool IsRegisteredItemId(int32 ItemId);

	/**
	 * Check if a player can use an item (has it in inventory and is registered).
	 * Uses RuleEngine's IsItemUsableNow plus registry check.
	 */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	static bool CanUseItem(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId);

	/**
	 * Validate that a target is acceptable for an item type.
	 * For now: basic checks based on EItemTargetType; Stage 7 refines per-item logic.
	 */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	static bool ValidateTarget(UGomokuRuleEngine* Engine, int32 ItemId, const FIntPoint& TargetCell, int32 TargetPlayerIndex);

	/**
	 * Execute a validated item effect.
	 * Validates ownership, removes item from inventory via RuleEngine.
	 * Returns false if unknown or not yet implemented.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	static bool ExecuteItem(UGomokuRuleEngine* Engine, int32 ItemId, int32 PlayerId, const FIntPoint& TargetCell, int32 TargetPlayerIndex);
};
