// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuGameState.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "Engine/World.h"

static AGomokuGameState* MakeStage11GameState(UWorld*& OutWorld)
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

	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(GetTransientPackage());
	FGomokuMatchConfig Config;
	Config.BoardSizeX = 15;
	Config.BoardSizeY = 15;
	Config.WinLength = 5;
	Config.MaxPlayers = 2;
	Config.InitialEnergyPerPlayer = 5;
	Engine->InitializeMatch(Config);
	GameState->SetRuleEngineRef(Engine);
	GameState->InitializeForLocalHotseat(2);
	return GameState;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage11_MiniGameLocksAndResumesMatch,
	TEXT("Gomoku.Stage11.MiniGameLocksAndResumesMatch"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage11_MiniGameLocksAndResumesMatch::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage11GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	UGomokuRuleEngine* Engine = GameState->GetRuleEngine();
	Engine->ConsumePlayerEnergy(1, 5);
	const int32 EnergyBefore = Engine->GetPlayerStateData(1).Energy;

	GameState->StartMiniGame();
	const FIntPoint Target(5, 3);
	if (!TestEqual(TEXT("Mini game phase"), GameState->MatchPhase, EMatchPhase::MiniGamePlaying) ||
		!TestTrue(TEXT("Main turn timer is paused"), GameState->bTimePaused) ||
		!TestTrue(TEXT("Mini game is active"), GameState->bMiniGameActive) ||
		!TestEqual(TEXT("Public puzzle contains 49 cells"), GameState->MiniGamePuzzleCells.Num(), 49) ||
		!TestEqual(TEXT("Answer location is presented as an empty playable cell"),
			GameState->MiniGamePuzzleCells[Target.Y * 7 + Target.X], ECellState::Empty))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!TestTrue(TEXT("Correct answer is accepted"), GameState->SubmitMiniGameAnswer(0, Target)) ||
		!TestEqual(TEXT("First correct answer grants rank-one energy"), Engine->GetPlayerStateData(1).Energy, EnergyBefore + 3) ||
		!TestFalse(TEXT("Duplicate answer is rejected"), GameState->SubmitMiniGameAnswer(0, Target)))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameState->SubmitMiniGameAnswer(1, FIntPoint(-1, -1));
	const bool bResultValid =
		TestFalse(TEXT("Mini game ends after all submissions"), GameState->bMiniGameActive) &&
		TestEqual(TEXT("Mini game enters result phase"), GameState->MatchPhase, EMatchPhase::MiniGameResult) &&
		TestTrue(TEXT("Player one is recorded as correct"), GameState->MiniGameCorrectPlayerIndices.Contains(0));

	GameState->ResumeFromMiniGame();
	const bool bResumeValid =
		TestEqual(TEXT("Match resumes in playing phase"), GameState->MatchPhase, EMatchPhase::Playing) &&
		TestFalse(TEXT("Main timer resumes"), GameState->bTimePaused) &&
		TestEqual(TEXT("Next player starts after mini game"), GameState->CurrentPlayerIndex, 1);

	World->DestroyWorld(false);
	return bResultValid && bResumeValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage11_MiniGameStartsAfterFiveRounds,
	TEXT("Gomoku.Stage11.MiniGameStartsAfterFiveRounds"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage11_MiniGameStartsAfterFiveRounds::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage11GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	const TArray<FIntPoint> Cells = {
		FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(2, 1), FIntPoint(2, 2),
		FIntPoint(4, 2), FIntPoint(4, 3), FIntPoint(6, 3), FIntPoint(6, 4),
		FIntPoint(8, 4), FIntPoint(8, 5)
	};

	for (int32 Turn = 0; Turn < Cells.Num(); ++Turn)
	{
		const int32 PlayerIndex = GameState->CurrentPlayerIndex;
		GameState->HandlePlaceStone(PlayerIndex, Cells[Turn]);
	}

	const bool bValid =
		TestEqual(TEXT("Sixth round is pending after five completed rounds"), GameState->CurrentRoundIndex, 6) &&
		TestEqual(TEXT("Mini game starts after five rounds"), GameState->MatchPhase, EMatchPhase::MiniGamePlaying) &&
		TestTrue(TEXT("Mini game timer starts at eight seconds"), GameState->MiniGameRemainingTime > 7.9f);

	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
