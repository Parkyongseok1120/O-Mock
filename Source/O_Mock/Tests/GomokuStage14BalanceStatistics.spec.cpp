// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GomokuBalanceStatistics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage14_PersistentBalanceStatistics,
	TEXT("Gomoku.Stage14.PersistentBalanceStatistics"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage14_PersistentBalanceStatistics::RunTest(const FString& Parameters)
{
	const FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
		TEXT("BalanceStats_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FGomokuBalanceMatchRecord First;
	First.MatchId = TEXT("test-match-1");
	First.StartedUtc = TEXT("2026-08-23T00:00:00Z");
	First.DurationSeconds = 100.0;
	First.CompletedRounds = 8;
	First.PlayerCount = 2;
	First.BoardSizeX = 15;
	First.BoardSizeY = 15;
	First.WinnerSeat = 1;
	First.ItemAcquiredCounts = { 2, 0, 0, 0, 0 };
	First.ItemUsedCounts = { 1, 0, 0, 0, 0 };
	FGomokuBalanceMatchRecord Second = First;
	Second.MatchId = TEXT("test-match-2");
	Second.DurationSeconds = 200.0;
	Second.CompletedRounds = 12;
	Second.WinnerSeat = 2;
	Second.ItemAcquiredCounts = { 2, 2, 0, 0, 0 };
	Second.ItemUsedCounts = { 2, 1, 0, 0, 0 };

	bool bValid = TestTrue(TEXT("First match appends and summarizes"),
		UGomokuBalanceStatisticsSubsystem::WriteRecordAndSummary(First, OutputDirectory))
		&& TestTrue(TEXT("Second match appends and refreshes aggregate"),
			UGomokuBalanceStatisticsSubsystem::WriteRecordAndSummary(Second, OutputDirectory))
		&& TestTrue(TEXT("Retrying the same MatchId is an idempotent repair"),
			UGomokuBalanceStatisticsSubsystem::WriteRecordAndSummary(Second, OutputDirectory));
	TArray<FGomokuBalanceMatchRecord> Loaded;
	const FString CsvPath = FPaths::Combine(OutputDirectory, TEXT("O_Mock_Matches.csv"));
	bValid &= TestTrue(TEXT("CSV history loads"),
		UGomokuBalanceStatisticsSubsystem::LoadRecords(CsvPath, Loaded));
	bValid &= TestEqual(TEXT("Two records persist"), Loaded.Num(), 2);

	FString JsonText;
	TSharedPtr<FJsonObject> Summary;
	const FString SummaryPath = FPaths::Combine(OutputDirectory, TEXT("O_Mock_BalanceSummary.json"));
	if (!FFileHelper::LoadFileToString(JsonText, *SummaryPath))
	{
		AddError(TEXT("Summary JSON was not written"));
		bValid = false;
	}
	else
	{
		const TSharedRef<TJsonReader<>> LoadedReader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(LoadedReader, Summary) || !Summary.IsValid())
		{
			AddError(TEXT("Summary JSON is invalid"));
			bValid = false;
		}
		else
		{
			bValid &= TestEqual(TEXT("Aggregate match count"),
				static_cast<int32>(Summary->GetNumberField(TEXT("matchCount"))), 2);
			bValid &= TestEqual(TEXT("Average game time"),
				Summary->GetNumberField(TEXT("averageGameTimeSeconds")), 150.0);
			const TSharedPtr<FJsonObject>* SeatStats = nullptr;
			const TSharedPtr<FJsonObject>* SeatOne = nullptr;
			if (!Summary->TryGetObjectField(TEXT("seatStatistics"), SeatStats) || !SeatStats
				|| !(*SeatStats)->TryGetObjectField(TEXT("seat1"), SeatOne) || !SeatOne)
			{
				AddError(TEXT("Seat statistics are missing"));
				bValid = false;
			}
			else
			{
				bValid &= TestEqual(TEXT("Seat one win rate"), (*SeatOne)->GetNumberField(TEXT("winRate")), 0.5);
			}
		}
	}

	IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
