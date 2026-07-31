// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GomokuTypes.h"
#include "GomokuRuleEngine.generated.h"

/**
 * Pure Gomoku rule engine with no UI or network dependency.
 * Create with NewObject<UGomokuRuleEngine>() and call InitializeMatch.
 */
UCLASS(BlueprintType)
class UGomokuRuleEngine : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void InitializeMatch(const FGomokuMatchConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	FGomokuMatchConfig GetMatchConfig() const { return MatchConfig; }

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool IsValidCoordinate(int32 X, int32 Y) const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	ECellState GetCellState(int32 X, int32 Y) const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	FBoardCell GetBoardCell(int32 X, int32 Y) const;

	/** Place a stone for PlayerId on an empty in-bounds cell. Does not advance the turn. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	bool TryPlaceStone(int32 PlayerId, int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	bool RemoveStoneAt(int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	bool ChangeCellOwnership(int32 X, int32 Y, int32 NewPlayerId);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	int32 GetCurrentPlayerId() const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void AdvanceTurn();

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	FGomokuPlayerStateData GetPlayerStateData(int32 PlayerId) const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	bool SetPlayerSkipNextTurn(int32 PlayerId, bool bSkip);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Win")
	bool IsGameWon(int32& OutWinnerId) const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Match")
	bool IsGameOver() const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Match")
	bool IsMatchInitialized() const { return bInitialized; }

	static ECellState PlayerIdToCellState(int32 PlayerId);
	static int32 CellStateToPlayerId(ECellState State);

private:
	int32 CountRay(int32 X, int32 Y, int32 DX, int32 DY, ECellState State) const;
	bool HasWinAt(int32 X, int32 Y) const;
	FGomokuPlayerStateData* FindPlayerMutable(int32 PlayerId);
	const FGomokuPlayerStateData* FindPlayer(int32 PlayerId) const;

	UPROPERTY()
	FGomokuMatchConfig MatchConfig;

	UPROPERTY()
	FGomokuBoardState Board;

	UPROPERTY()
	TArray<FGomokuPlayerStateData> Players;

	UPROPERTY()
	TArray<int32> PlayerOrder;

	UPROPERTY()
	int32 CurrentPlayerIndex = 0;

	UPROPERTY()
	bool bInitialized = false;
};
