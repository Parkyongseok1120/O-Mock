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

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
	int32 LocalPlayerCount = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 CurrentRoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	TSet<int32> PlayersCompletedThisRound;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 RoundTurnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
	int32 CurrentPlayerIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
	bool IsGameActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
	int32 WinnerPlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Time")
	bool bHasTimeSystem = true;

	UPROPERTY(BlueprintReadOnly, Category = "Time")
	bool bTimePaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "Time")
	TArray<FGomokuPlayerTimeState> PlayerTimes;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Players")
	TArray<FLinearColor> PlayerColors;

	UPROPERTY(BlueprintReadOnly, Category = "Time")
	FIntPoint HoveredCell = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
	EMatchPhase MatchPhase = EMatchPhase::Waiting;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Board")
	TArray<ECellState> ReplicatedBoardCells;

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

	void SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine);
	UGomokuRuleEngine* GetRuleEngine() const { return RuleEngine.Get(); }

	UFUNCTION(Server, Reliable)
	void InitializeForLocalHotseat(int32 MaxPlayers);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
	void HandlePlaceStone(int32 PlayerIndex, const FIntPoint& Cell);

	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetHoveredCell(const FIntPoint& InCell);

	UFUNCTION(BlueprintCallable, Category = "Time")
	void ClearHoveredCell();

	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetTimePaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void SetMatchPhase(EMatchPhase NewPhase);

	UFUNCTION(Server, Reliable)
	void RestartMatch();

	/** Current player abandons their turn/position. If only one active player remains, they win. */
	UFUNCTION(Server, Reliable)
	void RequestAbandonCurrentPlayer();

	UFUNCTION(BlueprintPure, Category = "Gomoku|UI")
	int32 GetNextMinigameRound() const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Players")
	int32 GetNumActivePlayers() const;

	void SyncReplicatedBoard();

	// Exposed for Stage 3 time-system tests.
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Time")
	void TickTimeSystem(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Time")
	void ApplyEndOfRoundRecovery();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	TWeakObjectPtr<UGomokuRuleEngine> RuleEngine;
	float TurnStartTime = 0.0f;

	void StartNewTurn();
	// bSkipCompletionTracking: when true, do not count current player as completed for this round (used by abandon).
	void EndCurrentTurn(bool bForceEnd, bool bSkipCompletionTracking = false);
	void AutoMoveOnTimeout();
};
