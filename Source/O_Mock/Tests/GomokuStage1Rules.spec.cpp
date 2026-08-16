// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"

static UGomokuRuleEngine* MakeStage1Engine(int32 BoardSize = 15, int32 WinLength = 5)
{
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(GetTransientPackage());

	FGomokuMatchConfig Config;
	Config.BoardSizeX = BoardSize;
	Config.BoardSizeY = BoardSize;
	Config.WinLength = WinLength;
	Config.MaxPlayers = 2;
	Config.TurnTimeLimit = 25.0f;
	Config.InitialEnergyPerPlayer = 1;
	Engine->InitializeMatch(Config);
	return Engine;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage1_InitializeAndCoordinateValidation,
	TEXT("Gomoku.Stage1.InitializeAndCoordinateValidation"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage1_InitializeAndCoordinateValidation::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* Engine = MakeStage1Engine(15, 5);

	if (!TestTrue(TEXT("Match is initialized"), Engine->IsMatchInitialized()))
		return false;
	if (!TestTrue(TEXT("Origin is valid"), Engine->IsValidCoordinate(0, 0)))
		return false;
	if (!TestTrue(TEXT("Last cell is valid"), Engine->IsValidCoordinate(14, 14)))
		return false;
	if (!TestFalse(TEXT("Negative X is invalid"), Engine->IsValidCoordinate(-1, 0)))
		return false;
	if (!TestFalse(TEXT("Y past board is invalid"), Engine->IsValidCoordinate(0, 15)))
		return false;
	if (!TestTrue(TEXT("Origin starts empty"), Engine->GetCellState(0, 0) == ECellState::Empty))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage1_PlaceRejectsInvalidAndOccupiedCells,
	TEXT("Gomoku.Stage1.PlaceRejectsInvalidAndOccupiedCells"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage1_PlaceRejectsInvalidAndOccupiedCells::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* Engine = MakeStage1Engine();

	if (!TestTrue(TEXT("Player 1 can place on an empty cell"), Engine->TryPlaceStone(1, 3, 4)))
		return false;
	if (!TestFalse(TEXT("Player 2 cannot place during player 1 turn"), Engine->TryPlaceStone(2, 4, 4)))
		return false;
	if (!TestFalse(TEXT("Occupied cell is rejected"), Engine->TryPlaceStone(1, 3, 4)))
		return false;
	if (!TestFalse(TEXT("Out-of-bounds cell is rejected"), Engine->TryPlaceStone(1, 15, 4)))
		return false;
	if (!TestEqual(TEXT("Placed cell keeps player ownership"), Engine->GetCellState(3, 4), ECellState::Player1))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage1_ChecksHorizontalVerticalAndDiagonalWins,
	TEXT("Gomoku.Stage1.ChecksHorizontalVerticalAndDiagonalWins"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage1_ChecksHorizontalVerticalAndDiagonalWins::RunTest(const FString& Parameters)
{
	const TArray<TArray<FIntPoint>> WinningLines = {
		{FIntPoint(1, 4), FIntPoint(2, 4), FIntPoint(3, 4), FIntPoint(4, 4), FIntPoint(5, 4)},
		{FIntPoint(7, 1), FIntPoint(7, 2), FIntPoint(7, 3), FIntPoint(7, 4), FIntPoint(7, 5)},
		{FIntPoint(1, 1), FIntPoint(2, 2), FIntPoint(3, 3), FIntPoint(4, 4), FIntPoint(5, 5)},
		{FIntPoint(5, 1), FIntPoint(4, 2), FIntPoint(3, 3), FIntPoint(2, 4), FIntPoint(1, 5)}
	};

	for (int32 LineIndex = 0; LineIndex < WinningLines.Num(); ++LineIndex)
	{
		UGomokuRuleEngine* Engine = MakeStage1Engine();
		for (const FIntPoint& Cell : WinningLines[LineIndex])
		{
			if (!TestTrue(FString::Printf(TEXT("Force place line %d"), LineIndex), Engine->ForcePlaceStone(1, Cell.X, Cell.Y)))
				return false;
		}

		const FGomokuWinResult Result = Engine->CheckWinAt(WinningLines[LineIndex].Last());
		if (!TestTrue(FString::Printf(TEXT("Line %d is a win"), LineIndex), Result.IsWin))
			return false;
		if (!TestEqual(FString::Printf(TEXT("Line %d winner index"), LineIndex), Result.WinnerPlayerIndex, 0))
			return false;

		int32 WinnerId = 0;
		if (!TestTrue(FString::Printf(TEXT("Line %d reports game winner"), LineIndex), Engine->IsGameWon(WinnerId)))
			return false;
		if (!TestEqual(FString::Printf(TEXT("Line %d winner id"), LineIndex), WinnerId, 1))
			return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage1_BlockedCellsAndBoardFull,
	TEXT("Gomoku.Stage1.BlockedCellsAndBoardFull"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage1_BlockedCellsAndBoardFull::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* Engine = MakeStage1Engine(3, 5);

	if (!TestTrue(TEXT("Empty cell can be blocked"), Engine->SetCellBlocked(1, 1, true)))
		return false;
	if (!TestFalse(TEXT("Blocked cell is not a valid empty cell"), Engine->IsValidEmpty(FIntPoint(1, 1))))
		return false;
	if (!TestFalse(TEXT("Stone cannot be placed on blocked cell"), Engine->TryPlaceStone(1, 1, 1)))
		return false;

	// Fill every other cell without creating a five-in-a-row on the 3x3 board.
	for (int32 Y = 0; Y < 3; ++Y)
	{
		for (int32 X = 0; X < 3; ++X)
		{
			if (X == 1 && Y == 1)
				continue;
			if (!TestTrue(TEXT("Fill remaining cell"), Engine->ForcePlaceStone(1, X, Y)))
				return false;
		}
	}

	if (!TestTrue(TEXT("Board is full when no empty cells remain"), Engine->IsBoardFull()))
		return false;
	if (!TestTrue(TEXT("Full board is game over"), Engine->IsGameOver()))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage1_RejectsPlacementAfterGameOver,
	TEXT("Gomoku.Stage1.RejectsPlacementAfterGameOver"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage1_RejectsPlacementAfterGameOver::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* Engine = MakeStage1Engine();

	for (int32 X = 0; X < 5; ++X)
	{
		if (!TestTrue(TEXT("Build winning line"), Engine->ForcePlaceStone(1, X, 0)))
			return false;
	}

	if (!TestTrue(TEXT("Game is over after a win"), Engine->IsGameOver()))
		return false;
	if (!TestFalse(TEXT("Normal placement is rejected after game over"), Engine->TryPlaceStone(1, 0, 1)))
		return false;

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
