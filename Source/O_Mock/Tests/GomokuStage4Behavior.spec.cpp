// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuGameState.h"
#include "GomokuTypes.h"
#include "Engine/World.h"

// Helper: create a rule engine with given MaxPlayers and BoardSize (square).
static UGomokuRuleEngine* MakeStage4Engine(int32 MaxPlayers, int32 BoardSize = 15)
{
	UObject* Outer = GetTransientPackage();

	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(Outer);

	FGomokuMatchConfig Config;
	Config.BoardSizeX = BoardSize;
	Config.BoardSizeY = BoardSize;
	Config.WinLength = 5;
	Config.MaxPlayers = MaxPlayers;
	Config.TurnTimeLimit = 30.f;
	Config.InitialEnergyPerPlayer = 10;

	Engine->InitializeMatch(Config);
	return Engine;
}

// Helper: simulate N turns by calling AdvanceTurn and recording GetCurrentPlayerIndex().
static TArray<int32> CollectTurnCycle(UGomokuRuleEngine* Engine, int32 NumTurns)
{
	TArray<int32> Sequence;
	Sequence.Reserve(NumTurns);

	for (int32 i = 0; i < NumTurns; ++i)
	{
		int32 CurrentIndex = Engine->GetCurrentPlayerIndex();
		Sequence.Add(CurrentIndex);
		Engine->AdvanceTurn();
	}

	return Sequence;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_TurnCycle2Players, TEXT("Gomoku.Stage4.TurnCycle2Players"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_TurnCycle2Players::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(2, 15);
	TArray<int32> Cycle = CollectTurnCycle(Engine, 4);

	TArray<int32> Expected;
	Expected.Add(0);
	Expected.Add(1);
	Expected.Add(0);
	Expected.Add(1);

	if (!TestEqual(TEXT("Turn cycle for 2 players"), Cycle, Expected))
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_TurnCycle3Players, TEXT("Gomoku.Stage4.TurnCycle3Players"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_TurnCycle3Players::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(3, 15);
	TArray<int32> Cycle = CollectTurnCycle(Engine, 6);

	TArray<int32> Expected;
	Expected.Add(0);
	Expected.Add(1);
	Expected.Add(2);
	Expected.Add(0);
	Expected.Add(1);
	Expected.Add(2);

	if (!TestEqual(TEXT("Turn cycle for 3 players"), Cycle, Expected))
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_TurnCycle4Players, TEXT("Gomoku.Stage4.TurnCycle4Players"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_TurnCycle4Players::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(4, 15);
	TArray<int32> Cycle = CollectTurnCycle(Engine, 8);

	TArray<int32> Expected;
	Expected.Add(0);
	Expected.Add(1);
	Expected.Add(2);
	Expected.Add(3);
	Expected.Add(0);
	Expected.Add(1);
	Expected.Add(2);
	Expected.Add(3);

	if (!TestEqual(TEXT("Turn cycle for 4 players"), Cycle, Expected))
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_AbandonExcludesPlayer, TEXT("Gomoku.Stage4.AbandonExcludesPlayer"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_AbandonExcludesPlayer::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(4);

	// Abandon player with id 2 (index 1).
	Engine->AbandonPlayer(2);

	TArray<int32> ActiveIndices = Engine->GetActivePlayerIndices();
	bool ContainsIndex1 = ActiveIndices.Contains(1);
	if (!TestFalse(TEXT("Abandoned index 1 must not be in active indices"), ContainsIndex1))
		return false;

	// Set current to index 0 and verify turn skips abandoned slot.
	Engine->SetCurrentPlayerIndex(0);

	TArray<int32> Sequence;
	Sequence.Reserve(3);
	for (int32 i = 0; i < 3; ++i)
	{
		int32 CurrentIndex = Engine->GetCurrentPlayerIndex();
		Sequence.Add(CurrentIndex);
		Engine->AdvanceTurn();
	}

	TArray<int32> Expected;
	Expected.Add(0);
	Expected.Add(2);
	Expected.Add(3);

	if (!TestEqual(TEXT("Turn order after abandon skips index 1"), Sequence, Expected))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_SkipNextPlayer, TEXT("Gomoku.Stage4.SkipNextPlayer"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_SkipNextPlayer::RunTest(const FString& Parameters)
{
	// PlayerId = index + 1. So SetPlayerSkipNextTurn(2) marks player at index 1 to be skipped next turn.
	auto* Engine = MakeStage4Engine(4);

	if (!TestEqual(TEXT("Initial current player index"), Engine->GetCurrentPlayerIndex(), 0))
		return false;

	// Mark player id 2 (index 1) to skip next turn.
	Engine->SetPlayerSkipNextTurn(2, true);

	TArray<int32> Sequence;
	Sequence.Reserve(2);
	for (int32 i = 0; i < 2; ++i)
	{
		int32 CurrentIndex = Engine->GetCurrentPlayerIndex();
		Sequence.Add(CurrentIndex);
		Engine->AdvanceTurn();
	}

	TArray<int32> Expected;
	Expected.Add(0);
	Expected.Add(2); // index 1 skipped due to SetPlayerSkipNextTurn(2, true)

	if (!TestEqual(TEXT("Skip next turn bypasses player id 2 (index 1)"), Sequence, Expected))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_ReverseTurnOrder, TEXT("Gomoku.Stage4.ReverseTurnOrder"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_ReverseTurnOrder::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(4);

	// Set current to index 1.
	Engine->SetCurrentPlayerIndex(1);

	if (!TestEqual(TEXT("Initial current player index"), Engine->GetCurrentPlayerIndex(), 1))
		return false;

	// Reverse turn direction.
	Engine->ReverseTurnDirection();

	int32 Dir = Engine->TurnDirection;
	if (!TestEqual(TEXT("Turn direction after reverse"), Dir, -1))
		return false;

	TArray<int32> Sequence;
	Sequence.Reserve(2);
	for (int32 i = 0; i < 2; ++i)
	{
		int32 CurrentIndex = Engine->GetCurrentPlayerIndex();
		Sequence.Add(CurrentIndex);
		Engine->AdvanceTurn();
	}

	TArray<int32> Expected;
	Expected.Add(1);
	Expected.Add(0); // reversed: 1 -> 0

	if (!TestEqual(TEXT("Reverse turn order from index 1"), Sequence, Expected))
		return false;

	// Continue to confirm wrap-around with reverse direction.
	int32 NextIndex = Engine->GetCurrentPlayerIndex();
	if (!TestEqual(TEXT("Next after 0 in reversed order (wrap)"), NextIndex, 3))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_AbandonLastPlayerAdvancesToFirst, TEXT("Gomoku.Stage4.AbandonLastPlayerAdvancesToFirst"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_AbandonLastPlayerAdvancesToFirst::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(4);
	Engine->SetCurrentPlayerIndex(3);
	Engine->AbandonPlayer(4);
	Engine->AdvanceTurn();

	if (!TestEqual(TEXT("Turn advances from abandoned last player to first active player"), Engine->GetCurrentPlayerIndex(), 0))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_RoundNoDuplicateCompletion, TEXT("Gomoku.Stage4.RoundNoDuplicateCompletion"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_RoundNoDuplicateCompletion::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(3);

	TArray<int32> ActiveIndices = Engine->GetActivePlayerIndices();
	if (!TestEqual(TEXT("Active indices count"), ActiveIndices.Num(), 3))
		return false;

	TSet<int32> Completed;

	// Add index 0 twice: must not duplicate.
	Completed.Add(0);
	Completed.Add(0);
	if (!TestEqual(TEXT("Duplicate add keeps set size 1"), Completed.Num(), 1))
		return false;

	// Not complete until all active indices are present.
	bool CompleteEarly = true;
	for (int32 Idx : ActiveIndices)
	{
		if (!Completed.Contains(Idx))
		{
			CompleteEarly = false;
			break;
		}
	}
	if (!TestFalse(TEXT("Not complete with only index 0"), CompleteEarly))
		return false;

	// Add remaining active indices.
	for (int32 Idx : ActiveIndices)
	{
		Completed.Add(Idx);
	}

	bool CompleteNow = true;
	for (int32 Idx : ActiveIndices)
	{
		if (!Completed.Contains(Idx))
		{
			CompleteNow = false;
			break;
		}
	}
	if (!TestTrue(TEXT("Complete when all active indices present"), CompleteNow))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_RoundIncrementsOnlyAfterAllActive, TEXT("Gomoku.Stage4.RoundIncrementsOnlyAfterAllActive"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_RoundIncrementsOnlyAfterAllActive::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
		return TestFalse(TEXT("CreateWorld failed"), true);

	AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
	if (!GS || !GS->IsValidLowLevel())
	{
		World->DestroyWorld(false);
		return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true);
	}

	auto* Engine = MakeStage4Engine(3, 15);
	GS->SetRuleEngineRef(Engine);

	GS->InitializeForLocalHotseat(3);

	int32 StartRound = GS->CurrentRoundIndex;

	// Place for first active player via CurrentPlayerIndex.
	{
		int32 Cur = GS->CurrentPlayerIndex;
		if (!TestEqual(TEXT("First turn index is 0"), Cur, 0)) { World->DestroyWorld(false); return false; }
		GS->HandlePlaceStone(Cur, FIntPoint(5, 5));
	}
	if (!TestEqual(TEXT("Round after 1st place"), GS->CurrentRoundIndex, StartRound))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Place for second active player via CurrentPlayerIndex.
	{
		int32 Cur = GS->CurrentPlayerIndex;
		if (!TestEqual(TEXT("Second turn index is 1"), Cur, 1)) { World->DestroyWorld(false); return false; }
		GS->HandlePlaceStone(Cur, FIntPoint(6, 5));
	}
	if (!TestEqual(TEXT("Round after 2nd place"), GS->CurrentRoundIndex, StartRound))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Place for third active player via CurrentPlayerIndex -> round increments.
	{
		int32 Cur = GS->CurrentPlayerIndex;
		if (!TestEqual(TEXT("Third turn index is 2"), Cur, 2)) { World->DestroyWorld(false); return false; }
		GS->HandlePlaceStone(Cur, FIntPoint(7, 5));
	}
	if (!TestEqual(TEXT("Round after all active placed"), GS->CurrentRoundIndex, StartRound + 1))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!TestEqual(TEXT("PlayersCompletedThisRound cleared"), GS->PlayersCompletedThisRound.Num(), 0))
	{
		World->DestroyWorld(false);
		return false;
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_InactiveExcludedFromRound, TEXT("Gomoku.Stage4.InactiveExcludedFromRound"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_InactiveExcludedFromRound::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
		return TestFalse(TEXT("CreateWorld failed"), true);

	AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
	if (!GS || !GS->IsValidLowLevel())
	{
		World->DestroyWorld(false);
		return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true);
	}

	auto* Engine = MakeStage4Engine(4, 15);
	GS->SetRuleEngineRef(Engine);

	GS->InitializeForLocalHotseat(4);

	int32 StartRound = GS->CurrentRoundIndex;

	// P0 places.
	{
		int32 Cur = GS->CurrentPlayerIndex;
		if (!TestEqual(TEXT("First turn index is 0"), Cur, 0)) { World->DestroyWorld(false); return false; }
		GS->HandlePlaceStone(Cur, FIntPoint(5, 5));
	}

	// Abandon current player (P1) without counting completion.
	GS->RequestAbandonCurrentPlayer();

	TArray<int32> ActiveIndices = Engine->GetActivePlayerIndices();
	if (!TestFalse(TEXT("Index 1 not in active after abandon"), ActiveIndices.Contains(1)))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!TestFalse(TEXT("Index 1 not in PlayersCompletedThisRound"), GS->PlayersCompletedThisRound.Contains(1)))
	{
		World->DestroyWorld(false);
		return false;
	}

	// Remaining active players place until round increments, using CurrentPlayerIndex.
	int32 Guard = 0;
	while (GS->CurrentRoundIndex == StartRound && Guard < 8)
	{
		int32 Cur = GS->CurrentPlayerIndex;
		GS->HandlePlaceStone(Cur, FIntPoint(Guard, 1));
		++Guard;
	}

	if (!TestEqual(TEXT("Round after remaining active placed"), GS->CurrentRoundIndex, StartRound + 1))
	{
		World->DestroyWorld(false);
		return false;
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_TimeSystem3And4Players, TEXT("Gomoku.Stage4.TimeSystem3And4Players"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_TimeSystem3And4Players::RunTest(const FString& Parameters)
{
	TArray<int32> PlayerCounts;
	PlayerCounts.Add(3);
	PlayerCounts.Add(4);

	for (int32 Players : PlayerCounts)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World) return TestFalse(TEXT("CreateWorld failed"), true);

		AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
		if (!GS || !GS->IsValidLowLevel()) { World->DestroyWorld(false); return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true); }

		auto* Engine = MakeStage4Engine(Players, 15);
		GS->SetRuleEngineRef(Engine);
		GS->InitializeForLocalHotseat(Players);

		if (!TestEqual(TEXT("LocalPlayerCount matches Players"), GS->LocalPlayerCount, Players)) { World->DestroyWorld(false); return false; }
		if (!TestEqual(TEXT("PlayerTimes count matches Players"), GS->PlayerTimes.Num(), Players)) { World->DestroyWorld(false); return false; }
		if (!TestEqual(TEXT("MaxPersonalTime is 120"), GS->MaxPersonalTime, 120.0f)) { World->DestroyWorld(false); return false; }
		if (!TestEqual(TEXT("MaxTurnTime is 25"), GS->MaxTurnTime, 25.0f)) { World->DestroyWorld(false); return false; }
		if (!TestTrue(TEXT("bHasTimeSystem is true"), GS->bHasTimeSystem)) { World->DestroyWorld(false); return false; }

		for (int32 i = 0; i < Players; ++i)
			if (!TestEqual(TEXT("PersonalRemaining is 120"), GS->PlayerTimes[i].PersonalRemaining, 120.0f)) { World->DestroyWorld(false); return false; }

		GS->SetTimePaused(true);
		if (!TestTrue(TEXT("bTimePaused after SetTimePaused(true)"), GS->bTimePaused)) { World->DestroyWorld(false); return false; }
		GS->SetTimePaused(false);
		if (!TestFalse(TEXT("bTimePaused after SetTimePaused(false)"), GS->bTimePaused)) { World->DestroyWorld(false); return false; }

		World->DestroyWorld(false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage4_Board21x21FourPlayers, TEXT("Gomoku.Stage4.Board21x21FourPlayers"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage4_Board21x21FourPlayers::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage4Engine(4, 21);

	FGomokuMatchConfig Config = Engine->GetMatchConfig();
	if (!TestEqual(TEXT("BoardSizeX is 21"), Config.BoardSizeX, 21)) return false;
	if (!TestEqual(TEXT("BoardSizeY is 21"), Config.BoardSizeY, 21)) return false;
	if (!TestEqual(TEXT("MaxPlayers is 4"), Config.MaxPlayers, 4)) return false;

	if (!TestTrue(TEXT("IsValidEmpty(0,0)"), Engine->IsValidEmpty(FIntPoint(0, 0)))) return false;
	if (!TestTrue(TEXT("IsValidEmpty(20,20)"), Engine->IsValidEmpty(FIntPoint(20, 20)))) return false;
	if (!TestFalse(TEXT("IsValidEmpty(21,0) out of bounds"), Engine->IsValidEmpty(FIntPoint(21, 0)))) return false;

	// Place stones for PlayerIds 1..4 on consecutive turns with AdvanceTurn.
	TArray<int32> Cycle;
	Cycle.Reserve(5);
	for (int32 i = 0; i < 5; ++i)
	{
		int32 Idx = Engine->GetCurrentPlayerIndex();
		Cycle.Add(Idx);

		int32 PlayerId = Idx + 1; // PlayerIds are index+1.
		bool Placed = Engine->TryPlaceStone(PlayerId, i, i);
		if (!TestTrue(FString::Printf(TEXT("TryPlaceStone for PlayerId %d"), PlayerId), Placed)) return false;

		Engine->AdvanceTurn();
	}

	TArray<int32> ExpectedCycle;
	ExpectedCycle.Add(0);
	ExpectedCycle.Add(1);
	ExpectedCycle.Add(2);
	ExpectedCycle.Add(3);
	ExpectedCycle.Add(0); // wrap to 0.

	if (!TestEqual(TEXT("Turn cycle with 4 players on 21x21"), Cycle, ExpectedCycle)) return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage4_SkippedPlayerCompletesRound,
	TEXT("Gomoku.Stage4.SkippedPlayerCompletesRound"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage4_SkippedPlayerCompletesRound::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World created"), World))
	{
		return false;
	}

	AGomokuGameState* GameState = World->SpawnActor<AGomokuGameState>();
	UGomokuRuleEngine* Engine = MakeStage4Engine(3, 15);
	GameState->SetRuleEngineRef(Engine);
	GameState->InitializeForLocalHotseat(3);
	Engine->SetPlayerSkipNextTurn(2, true);

	GameState->HandlePlaceStone(0, FIntPoint(0, 0));
	const bool bSkipValid =
		TestEqual(TEXT("Player two is skipped"), GameState->CurrentPlayerIndex, 2) &&
		TestTrue(TEXT("Skipped player counts as completed this round"), GameState->PlayersCompletedThisRound.Contains(1)) &&
		TestEqual(TEXT("Round remains one until player three acts"), GameState->CurrentRoundIndex, 1);

	GameState->HandlePlaceStone(2, FIntPoint(1, 0));
	const bool bRoundValid =
		TestEqual(TEXT("Round completes after the remaining active action"), GameState->CurrentRoundIndex, 2) &&
		TestEqual(TEXT("Next round returns to player one"), GameState->CurrentPlayerIndex, 0);

	World->DestroyWorld(false);
	return bSkipValid && bRoundValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
