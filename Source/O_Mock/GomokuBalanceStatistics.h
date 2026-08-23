// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GomokuTypes.h"
#include "GomokuBalanceStatistics.generated.h"

USTRUCT(BlueprintType)
struct FGomokuBalanceMatchRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	FString MatchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	FString StartedUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 CompletedRounds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 PlayerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 BoardSizeX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 BoardSizeY = 0;

	/** One-based seat; zero represents a draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 WinnerSeat = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	bool bItemsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	bool bMiniGameEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 TimeoutCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 AbandonCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 SkipUseCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 MiniGameCount = 0;

	/** First correct mini-game seat in the last challenge, or zero if nobody solved it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	int32 LastMiniGameWinnerSeat = 0;

	/** Index 0..4 maps to item IDs 1..5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	TArray<int32> ItemAcquiredCounts;

	/** Index 0..4 maps to item IDs 1..5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Balance")
	TArray<int32> ItemUsedCounts;

	void Normalize();
};

/** Host-only persistent Stage 14 balance telemetry. No player-identifying or password data is stored. */
UCLASS()
class O_MOCK_API UGomokuBalanceStatisticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void BeginMatch(const FGomokuMatchConfig& Config, bool bItemsEnabled, bool bMiniGameEnabled);
	void RecordItemAcquired(int32 ItemId);
	void RecordItemUsed(int32 ItemId);
	void RecordTimeout();
	void RecordAbandon();
	void RecordSkipUse();
	void RecordMiniGameStarted();
	void RecordMiniGameWinner(int32 OneBasedSeat);
	bool CompleteMatch(int32 WinnerPlayerIndex, int32 CompletedRounds);
	void CancelActiveMatch();

	UFUNCTION(BlueprintPure, Category = "Gomoku|Balance")
	FString GetCsvPath() const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Balance")
	FString GetSummaryPath() const;

	static bool WriteRecordAndSummary(const FGomokuBalanceMatchRecord& Record, const FString& OutputDirectory);
	static bool LoadRecords(const FString& CsvPath, TArray<FGomokuBalanceMatchRecord>& OutRecords);

private:
	FGomokuBalanceMatchRecord ActiveRecord;
	double ActiveStartSeconds = 0.0;
	bool bHasActiveRecord = false;
	bool bMiniGameWinnerRecordedForCurrentChallenge = false;
};
