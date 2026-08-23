// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBalanceStatistics.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuBalance, Log, All);

static const TCHAR* GomokuBalanceCsvHeader =
	TEXT("SchemaVersion,MatchId,StartedUtc,DurationSeconds,CompletedRounds,PlayerCount,BoardSizeX,BoardSizeY,WinnerSeat,ItemsEnabled,MiniGameEnabled,Timeouts,Abandons,SkipUses,MiniGames,LastMiniWinnerSeat,Item1Acquired,Item2Acquired,Item3Acquired,Item4Acquired,Item5Acquired,Item1Used,Item2Used,Item3Used,Item4Used,Item5Used\n");

void FGomokuBalanceMatchRecord::Normalize()
{
	PlayerCount = FMath::Clamp(PlayerCount, 2, 4);
	BoardSizeX = FMath::Max(1, BoardSizeX);
	BoardSizeY = FMath::Max(1, BoardSizeY);
	WinnerSeat = FMath::Clamp(WinnerSeat, 0, PlayerCount);
	DurationSeconds = FMath::Max(0.0, DurationSeconds);
	CompletedRounds = FMath::Max(0, CompletedRounds);
	TimeoutCount = FMath::Max(0, TimeoutCount);
	AbandonCount = FMath::Max(0, AbandonCount);
	SkipUseCount = FMath::Max(0, SkipUseCount);
	MiniGameCount = FMath::Max(0, MiniGameCount);
	LastMiniGameWinnerSeat = FMath::Clamp(LastMiniGameWinnerSeat, 0, PlayerCount);
	if (ItemAcquiredCounts.Num() < 5) ItemAcquiredCounts.AddZeroed(5 - ItemAcquiredCounts.Num());
	else if (ItemAcquiredCounts.Num() > 5) ItemAcquiredCounts.SetNum(5, EAllowShrinking::No);
	if (ItemUsedCounts.Num() < 5) ItemUsedCounts.AddZeroed(5 - ItemUsedCounts.Num());
	else if (ItemUsedCounts.Num() > 5) ItemUsedCounts.SetNum(5, EAllowShrinking::No);
	for (int32& Count : ItemAcquiredCounts) Count = FMath::Max(0, Count);
	for (int32& Count : ItemUsedCounts) Count = FMath::Max(0, Count);
}

void UGomokuBalanceStatisticsSubsystem::BeginMatch(
	const FGomokuMatchConfig& Config, bool bInItemsEnabled, bool bInMiniGameEnabled)
{
	ActiveRecord = FGomokuBalanceMatchRecord();
	ActiveRecord.MatchId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	ActiveRecord.StartedUtc = FDateTime::UtcNow().ToIso8601();
	ActiveRecord.PlayerCount = Config.MaxPlayers;
	ActiveRecord.BoardSizeX = Config.BoardSizeX;
	ActiveRecord.BoardSizeY = Config.BoardSizeY;
	ActiveRecord.bItemsEnabled = bInItemsEnabled;
	ActiveRecord.bMiniGameEnabled = bInMiniGameEnabled;
	ActiveRecord.Normalize();
	ActiveStartSeconds = FPlatformTime::Seconds();
	bHasActiveRecord = true;
	bMiniGameWinnerRecordedForCurrentChallenge = false;
}

void UGomokuBalanceStatisticsSubsystem::RecordItemAcquired(int32 ItemId)
{
	if (bHasActiveRecord && ActiveRecord.ItemAcquiredCounts.IsValidIndex(ItemId - 1))
	{
		++ActiveRecord.ItemAcquiredCounts[ItemId - 1];
	}
}

void UGomokuBalanceStatisticsSubsystem::RecordItemUsed(int32 ItemId)
{
	if (bHasActiveRecord && ActiveRecord.ItemUsedCounts.IsValidIndex(ItemId - 1))
	{
		++ActiveRecord.ItemUsedCounts[ItemId - 1];
	}
}

void UGomokuBalanceStatisticsSubsystem::RecordTimeout() { if (bHasActiveRecord) ++ActiveRecord.TimeoutCount; }
void UGomokuBalanceStatisticsSubsystem::RecordAbandon() { if (bHasActiveRecord) ++ActiveRecord.AbandonCount; }
void UGomokuBalanceStatisticsSubsystem::RecordSkipUse() { if (bHasActiveRecord) ++ActiveRecord.SkipUseCount; }
void UGomokuBalanceStatisticsSubsystem::RecordMiniGameStarted()
{
	if (bHasActiveRecord)
	{
		++ActiveRecord.MiniGameCount;
		bMiniGameWinnerRecordedForCurrentChallenge = false;
	}
}
void UGomokuBalanceStatisticsSubsystem::RecordMiniGameWinner(int32 OneBasedSeat)
{
	if (bHasActiveRecord && !bMiniGameWinnerRecordedForCurrentChallenge)
	{
		ActiveRecord.LastMiniGameWinnerSeat = FMath::Clamp(OneBasedSeat, 0, ActiveRecord.PlayerCount);
		bMiniGameWinnerRecordedForCurrentChallenge = true;
	}
}

bool UGomokuBalanceStatisticsSubsystem::CompleteMatch(int32 WinnerPlayerIndex, int32 CompletedRounds)
{
	if (!bHasActiveRecord)
	{
		return false;
	}
	ActiveRecord.DurationSeconds = FPlatformTime::Seconds() - ActiveStartSeconds;
	ActiveRecord.CompletedRounds = CompletedRounds;
	ActiveRecord.WinnerSeat = WinnerPlayerIndex == INDEX_NONE ? 0 : WinnerPlayerIndex + 1;
	ActiveRecord.Normalize();
	const bool bWritten = WriteRecordAndSummary(ActiveRecord, FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Balance")));
	// Keep the record live after an I/O failure.  A later retry is idempotent by
	// MatchId and cannot append the same completed match twice.
	if (bWritten)
	{
		bHasActiveRecord = false;
		bMiniGameWinnerRecordedForCurrentChallenge = false;
	}
	return bWritten;
}

void UGomokuBalanceStatisticsSubsystem::CancelActiveMatch()
{
	bHasActiveRecord = false;
	bMiniGameWinnerRecordedForCurrentChallenge = false;
	ActiveRecord = FGomokuBalanceMatchRecord();
}

FString UGomokuBalanceStatisticsSubsystem::GetCsvPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Balance"), TEXT("O_Mock_Matches.csv"));
}

FString UGomokuBalanceStatisticsSubsystem::GetSummaryPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Balance"), TEXT("O_Mock_BalanceSummary.json"));
}

static FString GomokuBalanceRecordToCsv(FGomokuBalanceMatchRecord Record)
{
	Record.Normalize();
	return FString::Printf(
		TEXT("2,%s,%s,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
		*Record.MatchId, *Record.StartedUtc, Record.DurationSeconds, Record.CompletedRounds,
		Record.PlayerCount, Record.BoardSizeX, Record.BoardSizeY, Record.WinnerSeat,
		Record.bItemsEnabled ? 1 : 0, Record.bMiniGameEnabled ? 1 : 0,
		Record.TimeoutCount, Record.AbandonCount, Record.SkipUseCount, Record.MiniGameCount,
		Record.LastMiniGameWinnerSeat,
		Record.ItemAcquiredCounts[0], Record.ItemAcquiredCounts[1], Record.ItemAcquiredCounts[2],
		Record.ItemAcquiredCounts[3], Record.ItemAcquiredCounts[4],
		Record.ItemUsedCounts[0], Record.ItemUsedCounts[1], Record.ItemUsedCounts[2],
		Record.ItemUsedCounts[3], Record.ItemUsedCounts[4]);
}

bool UGomokuBalanceStatisticsSubsystem::LoadRecords(
	const FString& CsvPath, TArray<FGomokuBalanceMatchRecord>& OutRecords)
{
	OutRecords.Reset();
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
	{
		return false;
	}
	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines, true);
	TSet<FString> SeenMatchIds;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Fields;
		Lines[LineIndex].ParseIntoArray(Fields, TEXT(","), false);
		if (Fields.Num() != 26 || FCString::Atoi(*Fields[0]) != 2)
		{
			continue;
		}
		FGomokuBalanceMatchRecord Record;
		Record.MatchId = Fields[1];
		Record.StartedUtc = Fields[2];
		Record.DurationSeconds = FCString::Atod(*Fields[3]);
		Record.CompletedRounds = FCString::Atoi(*Fields[4]);
		Record.PlayerCount = FCString::Atoi(*Fields[5]);
		Record.BoardSizeX = FCString::Atoi(*Fields[6]);
		Record.BoardSizeY = FCString::Atoi(*Fields[7]);
		Record.WinnerSeat = FCString::Atoi(*Fields[8]);
		Record.bItemsEnabled = FCString::Atoi(*Fields[9]) != 0;
		Record.bMiniGameEnabled = FCString::Atoi(*Fields[10]) != 0;
		Record.TimeoutCount = FCString::Atoi(*Fields[11]);
		Record.AbandonCount = FCString::Atoi(*Fields[12]);
		Record.SkipUseCount = FCString::Atoi(*Fields[13]);
		Record.MiniGameCount = FCString::Atoi(*Fields[14]);
		Record.LastMiniGameWinnerSeat = FCString::Atoi(*Fields[15]);
		Record.ItemAcquiredCounts.SetNumZeroed(5);
		Record.ItemUsedCounts.SetNumZeroed(5);
		for (int32 ItemIndex = 0; ItemIndex < 5; ++ItemIndex)
		{
			Record.ItemAcquiredCounts[ItemIndex] = FCString::Atoi(*Fields[16 + ItemIndex]);
			Record.ItemUsedCounts[ItemIndex] = FCString::Atoi(*Fields[21 + ItemIndex]);
		}
		Record.Normalize();
		if (!Record.MatchId.IsEmpty() && !SeenMatchIds.Contains(Record.MatchId))
		{
			SeenMatchIds.Add(Record.MatchId);
			OutRecords.Add(MoveTemp(Record));
		}
	}
	return true;
}

static bool WriteGomokuBalanceSummary(const TArray<FGomokuBalanceMatchRecord>& Records, const FString& SummaryPath)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 2);
	Root->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("matchCount"), Records.Num());
	double TotalDuration = 0.0;
	double TotalRounds = 0.0;
	int32 Draws = 0;
	int32 SeatWins[4] = { 0, 0, 0, 0 };
	int32 Acquired[5] = { 0, 0, 0, 0, 0 };
	int32 Used[5] = { 0, 0, 0, 0, 0 };
	int32 TotalTimeouts = 0;
	int32 TotalAbandons = 0;
	TMap<FString, int32> BoardTemplateCounts;
	for (const FGomokuBalanceMatchRecord& Record : Records)
	{
		TotalDuration += Record.DurationSeconds;
		TotalRounds += Record.CompletedRounds;
		TotalTimeouts += Record.TimeoutCount;
		TotalAbandons += Record.AbandonCount;
		if (Record.WinnerSeat <= 0) ++Draws;
		else if (Record.WinnerSeat <= 4) ++SeatWins[Record.WinnerSeat - 1];
		for (int32 ItemIndex = 0; ItemIndex < 5; ++ItemIndex)
		{
			Acquired[ItemIndex] += Record.ItemAcquiredCounts[ItemIndex];
			Used[ItemIndex] += Record.ItemUsedCounts[ItemIndex];
		}
		const FString BoardKey = FString::Printf(TEXT("%dx%d"), Record.BoardSizeX, Record.BoardSizeY);
		BoardTemplateCounts.FindOrAdd(BoardKey)++;
	}
	const double MatchDivisor = Records.IsEmpty() ? 1.0 : static_cast<double>(Records.Num());
	Root->SetNumberField(TEXT("averageGameTimeSeconds"), TotalDuration / MatchDivisor);
	Root->SetNumberField(TEXT("averageCompletedRounds"), TotalRounds / MatchDivisor);
	Root->SetNumberField(TEXT("drawRate"), Draws / MatchDivisor);
	Root->SetNumberField(TEXT("averageTimeoutsPerMatch"), TotalTimeouts / MatchDivisor);
	Root->SetNumberField(TEXT("averageAbandonsPerMatch"), TotalAbandons / MatchDivisor);
	TSharedRef<FJsonObject> SeatObject = MakeShared<FJsonObject>();
	for (int32 SeatIndex = 0; SeatIndex < 4; ++SeatIndex)
	{
		TSharedRef<FJsonObject> Seat = MakeShared<FJsonObject>();
		Seat->SetNumberField(TEXT("wins"), SeatWins[SeatIndex]);
		Seat->SetNumberField(TEXT("winRate"), SeatWins[SeatIndex] / MatchDivisor);
		SeatObject->SetObjectField(FString::Printf(TEXT("seat%d"), SeatIndex + 1), Seat);
	}
	Root->SetObjectField(TEXT("seatStatistics"), SeatObject);
	TArray<TSharedPtr<FJsonValue>> ItemArray;
	for (int32 ItemIndex = 0; ItemIndex < 5; ++ItemIndex)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("itemId"), ItemIndex + 1);
		Item->SetNumberField(TEXT("acquired"), Acquired[ItemIndex]);
		Item->SetNumberField(TEXT("used"), Used[ItemIndex]);
		Item->SetNumberField(TEXT("useRatePerAcquisition"),
			Acquired[ItemIndex] > 0 ? static_cast<double>(Used[ItemIndex]) / Acquired[ItemIndex] : 0.0);
		Item->SetNumberField(TEXT("usesPerMatch"), Used[ItemIndex] / MatchDivisor);
		ItemArray.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("itemStatistics"), ItemArray);
	TSharedRef<FJsonObject> BoardObject = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Pair : BoardTemplateCounts)
	{
		BoardObject->SetNumberField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(TEXT("boardTemplateMatchCounts"), BoardObject);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Json, *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UGomokuBalanceStatisticsSubsystem::WriteRecordAndSummary(
	const FGomokuBalanceMatchRecord& Record, const FString& OutputDirectory)
{
	if (Record.MatchId.IsEmpty())
	{
		return false;
	}
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		return false;
	}
	const FString CsvPath = FPaths::Combine(OutputDirectory, TEXT("O_Mock_Matches.csv"));
	const FString SummaryPath = FPaths::Combine(OutputDirectory, TEXT("O_Mock_BalanceSummary.json"));
	TArray<FGomokuBalanceMatchRecord> Records;
	if (FPaths::FileExists(CsvPath) && !LoadRecords(CsvPath, Records))
	{
		return false;
	}
	const bool bAlreadyRecorded = Records.ContainsByPredicate([&Record](const FGomokuBalanceMatchRecord& Existing)
	{
		return Existing.MatchId == Record.MatchId;
	});
	if (!bAlreadyRecorded)
	{
		FGomokuBalanceMatchRecord NormalizedRecord = Record;
		NormalizedRecord.Normalize();
		Records.Add(MoveTemp(NormalizedRecord));
	}

	FString CsvText(GomokuBalanceCsvHeader);
	for (const FGomokuBalanceMatchRecord& Existing : Records)
	{
		CsvText += GomokuBalanceRecordToCsv(Existing);
	}
	const FString TransactionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString CsvTempPath = CsvPath + TEXT(".tmp-") + TransactionId;
	const FString SummaryTempPath = SummaryPath + TEXT(".tmp-") + TransactionId;
	IFileManager& FileManager = IFileManager::Get();
	if (!FFileHelper::SaveStringToFile(CsvText, *CsvTempPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !WriteGomokuBalanceSummary(Records, SummaryTempPath))
	{
		FileManager.Delete(*CsvTempPath, false, true);
		FileManager.Delete(*SummaryTempPath, false, true);
		return false;
	}
	// Each file replacement is atomic.  A crash between the two replacements is
	// repaired by the next idempotent write, which always rebuilds JSON from CSV.
	if (!FileManager.Move(*CsvPath, *CsvTempPath, true, true, false, true))
	{
		FileManager.Delete(*CsvTempPath, false, true);
		FileManager.Delete(*SummaryTempPath, false, true);
		return false;
	}
	if (!FileManager.Move(*SummaryPath, *SummaryTempPath, true, true, false, true))
	{
		FileManager.Delete(*SummaryTempPath, false, true);
		return false;
	}
	UE_LOG(LogGomokuBalance, Display, TEXT("Stage 14 balance record saved: matches=%d csv=%s"),
		Records.Num(), *CsvPath);
	return true;
}
