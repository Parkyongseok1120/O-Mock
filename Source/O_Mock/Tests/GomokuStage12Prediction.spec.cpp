// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuPredictionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage12_AdvancedPredictionsMatchItemRules,
	TEXT("Gomoku.Stage12.AdvancedPredictionsMatchItemRules"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage12_AdvancedPredictionsMatchItemRules::RunTest(const FString& Parameters)
{
	constexpr int32 Size = 9;
	TArray<ECellState> Cells;
	Cells.Init(ECellState::Empty, Size * Size);
	auto Set = [&Cells](int32 X, int32 Y, ECellState State) { Cells[Y * Size + X] = State; };
	for (int32 X = 1; X <= 4; ++X) Set(X, 2, ECellState::Player1);
	Set(4, 4, ECellState::Player1);
	Set(5, 4, ECellState::Player2);
	Set(7, 7, ECellState::Player2);

	const TArray<FIntPoint> Wins = UGomokuPredictionLibrary::FindImmediateWinCells(Cells, Size, Size, 1);
	if (!TestTrue(TEXT("Both ends of an open four are predicted"),
		Wins.Contains(FIntPoint(0, 2)) && Wins.Contains(FIntPoint(5, 2))))
	{
		return false;
	}
	TArray<FIntPoint> Guardians = { FIntPoint(7, 7) };
	const TArray<FIntPoint> StealTargets = UGomokuPredictionLibrary::FindStealTargets(
		Cells, Size, Size, 1, Guardians);
	if (!TestTrue(TEXT("Isolated unguarded enemy is stealable"), StealTargets.Contains(FIntPoint(5, 4)))
		|| !TestFalse(TEXT("Guardian-protected enemy is excluded"), StealTargets.Contains(FIntPoint(7, 7))))
	{
		return false;
	}
	FIntPoint Source;
	const FIntPoint PullDestination(4, 3);
	return TestTrue(TEXT("Pull destination is predicted"),
		UGomokuPredictionLibrary::FindPullDestinations(Cells, Size, Size, 1, Guardians).Contains(PullDestination))
		&& TestTrue(TEXT("Pull source resolves with authoritative direction priority"),
			UGomokuPredictionLibrary::FindPullSource(Cells, Size, Size, 1, PullDestination, Guardians, Source))
		&& TestEqual(TEXT("Pull source location is exact"), Source, FIntPoint(4, 4));
}

#endif // WITH_DEV_AUTOMATION_TESTS
