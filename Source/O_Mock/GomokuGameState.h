// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "GomokuTypes.h"
#include "GomokuRuleEngine.h"
#include "GomokuMinigameTypes.h"
#include "GomokuMatchEventLog.h"
#include "GomokuGameState.generated.h"

USTRUCT(BlueprintType)
struct FGomokuPlayerTimeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Time")
	float PersonalRemaining = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Time")
	float TurnElapsedThisTurn = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnChanged, int32, PlayerIndex, int32, RoundIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStonePlacedDelegate, const FIntPoint&, Cell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchRestartedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEndedDelegate, const FGomokuWinResult&, WinResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerTimeTick, int32, PlayerIndex, const FGomokuPlayerTimeState&, TimeState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEventDelegate, const FMatchEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReplicatedBoardChangedDelegate);

UCLASS()
class O_MOCK_API AGomokuGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGomokuGameState();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
	float MaxPersonalTime = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
	float MaxTurnTime = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
	float PersonalRecoveryRate = 0.35f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Match")
	int32 LocalPlayerCount = 2;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 CurrentRoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	TSet<int32> PlayersCompletedThisRound;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 RoundTurnCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_TurnState, BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 CurrentPlayerIndex = -1;

	UPROPERTY(ReplicatedUsing=OnRep_IsGameActive, BlueprintReadOnly, Category = "Gomoku|Match")
	bool IsGameActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Match")
	int32 WinnerPlayerIndex = INDEX_NONE;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Time")
	bool bHasTimeSystem = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Time")
	bool bTimePaused = false;

	UPROPERTY(ReplicatedUsing=OnRep_PlayerTimes, BlueprintReadOnly, Category = "Time")
	TArray<FGomokuPlayerTimeState> PlayerTimes;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Players")
	TArray<FLinearColor> PlayerColors;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Time")
	FIntPoint HoveredCell = FIntPoint(-1, -1);

	UPROPERTY(ReplicatedUsing=OnRep_MatchPresentation, BlueprintReadOnly, Category = "Gomoku|Match")
	EMatchPhase MatchPhase = EMatchPhase::Waiting;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	bool bMiniGameActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	float MiniGameRemainingTime = 0.0f;

	/** Public 7x7 puzzle data. The correct answer remains server-only. */
	UPROPERTY(ReplicatedUsing=OnRep_MatchPresentation, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	TArray<ECellState> MiniGamePuzzleCells;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	float MiniGameResultRemainingTime = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	TArray<int32> MiniGameSubmittedPlayerIndices;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	TArray<int32> MiniGameCorrectPlayerIndices;

	/** Player currently holding the shared input in local hotseat mini-games. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|MiniGame")
	int32 MiniGameInputPlayerIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedBoardCells, BlueprintReadOnly, Category = "Gomoku|Board")
	TArray<ECellState> ReplicatedBoardCells;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Board")
	int32 ReplicatedBoardSizeX = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Board")
	int32 ReplicatedBoardSizeY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Events")
	TObjectPtr<UGomokuMatchEventLog> EventLog;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnTurnChanged OnTurnChanged;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnStonePlacedDelegate OnStonePlaced;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnMatchRestartedDelegate OnMatchRestarted;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnMatchEndedDelegate OnMatchEnded;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnPlayerTimeTick OnTickPlayerTime;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
	FOnMatchEventDelegate OnMatchEvent;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|Board")
	FOnReplicatedBoardChangedDelegate OnReplicatedBoardChanged;

	void SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine);
	UGomokuRuleEngine* GetRuleEngine() const { return RuleEngine.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void InitializeForLocalHotseat(int32 MaxPlayers);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void HandlePlaceStone(int32 PlayerIndex, const FIntPoint& Cell);

	/** Server-side item execution entry point. PlayerIndex is resolved from the requesting controller on network paths. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void HandleUseItem(int32 PlayerIndex, int32 ItemId, const FIntPoint& TargetCell, int32 TargetPlayerIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetHoveredCell(const FIntPoint& InCell);

	UFUNCTION(BlueprintCallable, Category = "Time")
	void ClearHoveredCell();

	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetTimePaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void SetMatchPhase(EMatchPhase NewPhase);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void RestartMatch();

	/** Current player abandons their turn/position. If only one active player remains, they win. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void RequestAbandonCurrentPlayer();

	/** Server-only transition used by Logout and the owning controller. PlayerId is 1-based. */
	void RequestAbandonPlayer(int32 PlayerId);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|MiniGame")
	void StartMiniGame();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|MiniGame")
	bool SubmitMiniGameAnswer(int32 PlayerIndex, const FIntPoint& AnswerCell);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|MiniGame")
	void ResumeFromMiniGame();

	/** Returns the player who should use the shared mouse for the next hotseat submission. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|MiniGame")
	int32 GetMiniGameInputPlayerIndex() const { return MiniGameInputPlayerIndex; }

	UFUNCTION(BlueprintPure, Category = "Gomoku|UI")
	int32 GetNextMinigameRound() const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Players")
	int32 GetNumActivePlayers() const;

	void SyncReplicatedBoard();
	void SyncPlayerStates();

	// Exposed for Stage 3 time-system tests.
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Time")
	void TickTimeSystem(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Time")
	void ApplyEndOfRoundRecovery();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	TWeakObjectPtr<UGomokuRuleEngine> RuleEngine;
	FIntPoint MiniGameAnswerCell = FIntPoint(-1, -1);

	void StartNewTurn();
	// bSkipCompletionTracking: when true, do not count current player as completed for this round (used by abandon).
	void EndCurrentTurn(bool bForceEnd, bool bSkipCompletionTracking = false);
	void AutoMoveOnTimeout();
	void TickMiniGame(float DeltaSeconds);
	int32 FindNextMiniGameInputPlayer(int32 AfterPlayerIndex) const;

	// Replication callbacks
	UFUNCTION()
	void OnRep_TurnState();

	UFUNCTION()
	void OnRep_PlayerTimes();

	UFUNCTION()
	void OnRep_IsGameActive(bool bPreviousValue);

	UFUNCTION()
	void OnRep_ReplicatedBoardCells(const TArray<ECellState>& PreviousCells);

	UFUNCTION()
	void OnRep_MatchPresentation();
};
