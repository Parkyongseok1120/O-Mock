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
class O_MOCK_API UGomokuRuleEngine : public UObject
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

	/** Place stone ignoring current-turn gate (items / timeout helpers). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	bool ForcePlaceStone(int32 PlayerId, int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	bool SetCellBlocked(int32 X, int32 Y, bool bBlocked);

	/** Atomically moves one of PlayerId's stones to an empty destination. */
	bool MoveStone(int32 PlayerId, const FIntPoint& Source, const FIntPoint& Destination);

	/** A stone is isolated when none of its owner's stones occupy the eight neighbouring cells. */
	bool IsStoneIsolated(const FIntPoint& Cell) const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool ConsumePlayerEnergy(int32 PlayerId, int32 Cost);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool AddPlayerEnergy(int32 PlayerId, int32 Amount, int32 MaxEnergy = 5);

	/** Check if a player has an item in their inventory. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	bool PlayerHasItem(int32 PlayerId, int32 ItemId) const;

	/** Remove a single item from player's inventory (Stage 6 helper). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool RemoveItemFromInventory(int32 PlayerId, int32 ItemId);

	/** Add an item to player's inventory (unique IDs; no duplicates of same ItemId). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool AddItemToInventory(int32 PlayerId, int32 ItemId);

	/** Stores an owner-only replacement offer. The item must be registered and not already owned. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool SetPendingInventoryItem(int32 PlayerId, int32 ItemId);

	/** Atomically discards one owned item and inserts the pending offer, locked until the next turn. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool ReplaceInventoryItemWithPending(int32 PlayerId, int32 DiscardItemId);

	/** If a slot became free while an offer was pending, inserts it without discarding anything. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool ClaimPendingInventoryItemIntoFreeSlot(int32 PlayerId);

	/** Mark this item as gained this turn: not usable until next turn. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void MarkItemGainedThisTurn(int32 PlayerId, int32 ItemId);

	/** Clear per-turn item locks for a player (called at turn start). Moves GainedThisTurn items to usable. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void ClearTurnItemLocksForPlayer(int32 PlayerId);

	/** Reset one-item-per-turn flag for a player (called at turn start). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void ResetUsedItemThisTurn(int32 PlayerId);

	/** Mark that this player has used their item slot this turn. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void SetUsedItemThisTurn(int32 PlayerId);

	/** Check if a specific item is currently usable by the player (inventory + not locked + one-per-turn). */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	bool IsItemUsableNow(int32 PlayerId, int32 ItemId) const;

	/** GuardianBarrier: mark cell as protected so Steal/Pull fail against it. Returns false if invalid or not an occupied cell. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	bool SetCellGuardianProtected(int32 X, int32 Y, bool bProtected);

	/** GuardianBarrier: query if a cell is currently protected. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	bool IsCellGuardianProtected(int32 X, int32 Y) const;

	/** Public presentation snapshot for guarded-cell replication. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	TArray<FIntPoint> GetGuardianProtectedCells() const;

	bool ApplyTemporaryBlock(const FIntPoint& Cell, int32 ExpireAfterRound);
	bool ApplyGuardianProtection(const FIntPoint& Cell, int32 ExpireAfterRound);
	void ExpireRoundEffects(int32 CompletedRoundIndex);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	int32 GetCurrentPlayerId() const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void AdvanceTurn();

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	FGomokuPlayerStateData GetPlayerStateData(int32 PlayerId) const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	bool SetPlayerSkipNextTurn(int32 PlayerId, bool bSkip);

	/** Mark player abandoned and rebuild ActivePlayerIndices. Returns remaining active count. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	int32 AbandonPlayer(int32 PlayerId);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	TArray<int32> GetActivePlayerIndices() const { return ActivePlayerIndices; }

	int32 ResolveActivePlayerId(int32 ActivePlayerListIndex) const;
	bool IsPlayerActive(int32 PlayerId) const { return IsActiveMatchPlayer(PlayerId); }

	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	int32 GetCurrentPlayerIndex() const { return CurrentPlayerIndex; }

	/** Returns the sole active player index if exactly one remains (e.g. after abandon), else INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Turn")
	int32 GetSoleActivePlayerIndex() const;

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

	/** LastItemWinResult: cached result from the most recent item-driven board change (Steal/Pull).
	 *  Set by GomokuItemLibrary after those effects so GameState/tests can read it. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Items")
	FGomokuWinResult GetLastItemWinResult() const { return LastItemWinResult; }

	void SetLastItemWinResult(const FGomokuWinResult& InResult) { LastItemWinResult = InResult; }

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
	bool IsActiveMatchPlayer(int32 PlayerId) const;

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

	/** Round-indexed temporary cell effects. MAX_int32 represents an explicit permanent test/editor effect. */
	UPROPERTY()
	TMap<FIntPoint, int32> GuardianProtectionExpireRounds;

	UPROPERTY()
	TMap<FIntPoint, int32> TemporaryBlockedCellExpireRounds;

	/** Cached win result from last item-driven board change (Steal/Pull). Set by GomokuItemLibrary. */
	UPROPERTY()
	FGomokuWinResult LastItemWinResult;

	/** O(1) lookup: PlayerId -> index into Players. Kept in sync with InitializeMatch/AbandonPlayer.
	 *  mutable so const FindPlayer can call TMap::Find without const_cast. */
	mutable TMap<int32, int32> PlayerIdToIndex;
};
