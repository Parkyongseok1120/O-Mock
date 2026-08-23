// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuGameState.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "Engine/World.h"

static UGomokuRuleEngine* MakeStage2Engine()
{
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(GetTransientPackage());

	FGomokuMatchConfig Config;
	Config.BoardSizeX = 15;
	Config.BoardSizeY = 15;
	Config.WinLength = 5;
	Config.MaxPlayers = 2;
	Config.TurnTimeLimit = 25.0f;
	Config.InitialEnergyPerPlayer = 1;
	Engine->InitializeMatch(Config);
	return Engine;
}

static AGomokuGameState* MakeStage2GameState(UWorld*& OutWorld)
{
	OutWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!OutWorld)
		return nullptr;

	AGomokuGameState* GameState = OutWorld->SpawnActor<AGomokuGameState>();
	if (!GameState)
	{
		OutWorld->DestroyWorld(false);
		OutWorld = nullptr;
		return nullptr;
	}

	GameState->SetRuleEngineRef(MakeStage2Engine());
	// These tests isolate base turn/win behavior; inventory-modal behavior is
	// covered by the Stage 8 server-authority tests.
	GameState->bItemsEnabled = false;
	GameState->InitializeForLocalHotseat(2);
	return GameState;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage2_InitializesLocalTwoPlayerMatch,
	TEXT("Gomoku.Stage2.InitializesLocalTwoPlayerMatch"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage2_InitializesLocalTwoPlayerMatch::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage2GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	const bool bValid =
		TestTrue(TEXT("Match is active"), GameState->IsGameActive) &&
		TestEqual(TEXT("Local player count is two"), GameState->LocalPlayerCount, 2) &&
		TestEqual(TEXT("First player starts"), GameState->CurrentPlayerIndex, 0) &&
		TestEqual(TEXT("Board replication size"), GameState->ReplicatedBoardCells.Num(), 15 * 15) &&
		TestEqual(TEXT("Match starts in playing phase"), GameState->MatchPhase, EMatchPhase::Playing);

	World->DestroyWorld(false);
	return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage2_AlternatesTurnsAndEndsOnFive,
	TEXT("Gomoku.Stage2.AlternatesTurnsAndEndsOnFive"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage2_AlternatesTurnsAndEndsOnFive::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage2GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	const TArray<FIntPoint> PlayerOneCells = {
		FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0), FIntPoint(3, 0), FIntPoint(4, 0)
	};
	const TArray<FIntPoint> PlayerTwoCells = {
		FIntPoint(0, 1), FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1)
	};

	for (int32 Turn = 0; Turn < PlayerOneCells.Num(); ++Turn)
	{
		if (!TestEqual(TEXT("Player one turn index"), GameState->CurrentPlayerIndex, 0))
		{
			World->DestroyWorld(false);
			return false;
		}
		GameState->HandlePlaceStone(0, PlayerOneCells[Turn]);

		if (Turn < PlayerTwoCells.Num())
		{
			if (!TestEqual(TEXT("Player two turn index"), GameState->CurrentPlayerIndex, 1))
			{
				World->DestroyWorld(false);
				return false;
			}
			GameState->HandlePlaceStone(1, PlayerTwoCells[Turn]);
		}
	}

	const bool bValid =
		TestFalse(TEXT("Match ends after five stones"), GameState->IsGameActive) &&
		TestEqual(TEXT("Winner is player one"), GameState->WinnerPlayerIndex, 0) &&
		TestEqual(TEXT("Match enters game over"), GameState->MatchPhase, EMatchPhase::GameOver) &&
		TestEqual(TEXT("Winning cell contains player one"), GameState->ReplicatedBoardCells[0], ECellState::Player1);

	World->DestroyWorld(false);
	return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage2_RestartClearsBoardAndRestoresTurn,
	TEXT("Gomoku.Stage2.RestartClearsBoardAndRestoresTurn"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage2_RestartClearsBoardAndRestoresTurn::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage2GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	GameState->HandlePlaceStone(0, FIntPoint(7, 7));
	if (!TestEqual(TEXT("A stone was placed before restart"), GameState->ReplicatedBoardCells[7 * 15 + 7], ECellState::Player1))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameState->RestartMatch();
	const bool bValid =
		TestTrue(TEXT("Match is active after restart"), GameState->IsGameActive) &&
		TestEqual(TEXT("First player starts after restart"), GameState->CurrentPlayerIndex, 0) &&
		TestEqual(TEXT("Restart clears board cell"), GameState->ReplicatedBoardCells[7 * 15 + 7], ECellState::Empty) &&
		TestEqual(TEXT("Restart restores playing phase"), GameState->MatchPhase, EMatchPhase::Playing);

	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
