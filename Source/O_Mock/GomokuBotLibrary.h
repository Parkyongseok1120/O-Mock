// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GomokuRuleEngine.h"
#include "GomokuBotLibrary.generated.h"

/** Small deterministic bot used for repeatable rules-engine simulations. */
UCLASS()
class O_MOCK_API UGomokuBotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Picks a winning move, then a blocking move, then a nearby/central empty cell. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Bot")
	static bool ChooseMove(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell);

	/** Places the selected move for the current player. Turn advancement remains the caller's responsibility. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Bot")
	static bool TakeTurn(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell);

private:
	static bool FindImmediateWin(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell);
	static bool HasAdjacentStone(UGomokuRuleEngine* Engine, const FIntPoint& Cell, int32 PlayerId, bool bOwnOnly);
};
