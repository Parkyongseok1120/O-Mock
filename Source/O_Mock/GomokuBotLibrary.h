// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GomokuRuleEngine.h"
#include "GomokuBotLibrary.generated.h"

USTRUCT(BlueprintType)
struct FGomokuBotItemAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Bot")
	bool bUsedItem = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Bot")
	int32 ItemId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Bot")
	FIntPoint TargetCell = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Bot")
	int32 TargetPlayerIndex = INDEX_NONE;
};

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

	/** Uses at most one inventory item according to the supplied probability, then places a stone. */
	static bool TakeTurnWithItems(UGomokuRuleEngine* Engine, int32 PlayerId, float ItemUseProbability,
		FRandomStream& RandomStream, int32 CurrentRoundIndex, FGomokuBotItemAction& OutItemAction,
		FIntPoint& OutCell);

	/** Selects an item and legal target without mutating the engine. Runtime bots execute it through GameState. */
	static bool ChooseItemAction(UGomokuRuleEngine* Engine, int32 PlayerId, float ItemUseProbability,
		FRandomStream& RandomStream, FGomokuBotItemAction& OutAction);

	static bool TryUseItem(UGomokuRuleEngine* Engine, int32 PlayerId, float ItemUseProbability,
		FRandomStream& RandomStream, int32 CurrentRoundIndex, FGomokuBotItemAction& OutAction);

private:
	static bool FindImmediateWin(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell);
	static bool HasAdjacentStone(UGomokuRuleEngine* Engine, const FIntPoint& Cell, int32 PlayerId, bool bOwnOnly);
	static bool FindItemTarget(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId,
		FIntPoint& OutCell, int32& OutTargetPlayerIndex);
};
