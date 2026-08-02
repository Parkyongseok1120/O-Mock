// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GomokuTypes.h"
#include "GomokuRuleEngine.h"
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

UCLASS()
class O_MOCK_API AGomokuGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AGomokuGameState();

    // Time system settings (Step 3)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
    float MaxPersonalTime = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
    float MaxTurnTime = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Time")
    float PersonalRecoveryRate = 0.35f;

    // Match / turn state (Step 2)
    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
    int32 LocalPlayerCount = 2;

    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
    int32 CurrentRoundIndex = 0;

    /** Tracks which player indices have acted this round (for multi-player). */
    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
    TSet<int32> PlayersCompletedThisRound;

    /** How many valid turns occurred in current round. */
    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
    int32 RoundTurnCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Turn")
    int32 CurrentPlayerIndex = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
    bool IsGameActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Match")
    int32 WinnerPlayerIndex = INDEX_NONE;

    // Time system state (Step 3)
    UPROPERTY(BlueprintReadOnly, Category = "Time")
    bool bHasTimeSystem = true;

    UPROPERTY(BlueprintReadOnly, Category = "Time")
    bool bTimePaused = false;

    UPROPERTY(BlueprintReadOnly, Category = "Time")
    TArray<FGomokuPlayerTimeState> PlayerTimes;

    // Last hovered cell for timeout auto-move (Step 3)
    UPROPERTY(BlueprintReadOnly, Category = "Time")
    FIntPoint HoveredCell = FIntPoint(-1, -1);

    // Delegates for HUD / BoardActor
    UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
    FOnTurnChanged OnTurnChanged;

    UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
    FOnStonePlacedDelegate OnStonePlaced;

    UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
    FOnMatchRestartedDelegate OnMatchRestarted;

    UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
    FOnMatchEndedDelegate OnMatchEnded;

    // Public API
    void SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine);

    // Initialize for local hotseat (default 2 players)
    UFUNCTION(Server, Reliable)
    void InitializeForLocalHotseat(int32 MaxPlayers);

    // Called when a stone is placed. Handles win check and turn advancement.
    UFUNCTION(BlueprintCallable, Category = "Gomoku|Turn")
    void HandlePlaceStone(int32 PlayerIndex, const FIntPoint& Cell);

    // Update hovered cell (for timeout auto-move)
    UFUNCTION(BlueprintCallable, Category = "Time")
    void SetHoveredCell(const FIntPoint& InCell);

    // Restart match: reset turn, times, and notify listeners.
    UFUNCTION(Server, Reliable)
    void RestartMatch();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    TWeakObjectPtr<UGomokuRuleEngine> RuleEngine;

    float TurnStartTime = 0.0f;

    UPROPERTY(BlueprintAssignable, Category = "Gomoku|Events")
    FOnPlayerTimeTick OnTickPlayerTime;

    void StartNewTurn();
    void EndCurrentTurn(bool bForceEnd);
    void TickTimeSystem(float DeltaSeconds);
    void ApplyEndOfRoundRecovery();
    void AutoMoveOnTimeout();
};
