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

	/** Check if the board is completely filled (no empty cells). */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool IsBoardFull() const;

	/** Returns true if cell is inside bounds and currently Empty. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool IsValidEmpty(const FIntPoint& Cell) const;

	/** Override current player index (0-based into PlayerOrder). Used by GameState for turn control. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void SetCurrentPlayerIndex(int32 Index);

	/** Check win at a specific cell after placement. Returns FGomokuWinResult with IsWin and WinnerPlayerIndex (0-based). */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Win")
	FGomokuWinResult CheckWinAt(const FIntPoint& Cell) const;

	static ECellState PlayerIdToCellState(int32 PlayerId);
	static int32 CellStateToPlayerId(ECellState State);

	/** Build ActivePlayerIndices from current Players (excluding abandoned). Call after InitializeMatch or when abandon state changes. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void InitializeActivePlayerIndices();

	/** Current turn direction: +1 (forward) or -1 (reverse). Used by AdvanceTurn for circular rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 TurnDirection = 1;

	/** Safely reverse the turn direction without changing current player. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void ReverseTurnDirection();

private:
	int32 CountRay(int32 X, int32 Y, int32 DX, int32 DY, ECellState State) const;
	bool HasWinAt(int32 X, int32 Y) const;
	FGomokuPlayerStateData* FindPlayerMutable(int32 PlayerId);
	const FGomokuPlayerStateData* FindPlayer(int32 PlayerId) const;

	/** Compute next active player index using ActivePlayerIndices and Direction. */
	int32 AdvanceTurnIndex(int32 CurrentIndex, int32 Direction) const;

	UPROPERTY()
	FGomokuMatchConfig MatchConfig;

	UPROPERTY()
	FGomokuBoardState Board;

	UPROPERTY()
	TArray<FGomokuPlayerStateData> Players;

	UPROPERTY()
	TArray<int32> PlayerOrder;

	/** Indices into Players of currently active (non-abandoned) players. */
	UPROPERTY()
	TArray<int32> ActivePlayerIndices;

	UPROPERTY()
	int32 CurrentPlayerIndex = 0;

	UPROPERTY()
	bool bInitialized = false;
};
