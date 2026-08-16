// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuGameState.h"
#include "GomokuItemLibrary.h"
#include "GomokuMatchEventLog.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "Engine/World.h"

static UGomokuRuleEngine* MakeStage8Engine()
{
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(GetTransientPackage());

	FGomokuMatchConfig Config;
	Config.BoardSizeX = 15;
	Config.BoardSizeY = 15;
	Config.WinLength = 5;
	Config.MaxPlayers = 2;
	Config.TurnTimeLimit = 25.0f;
	Config.InitialEnergyPerPlayer = 5;
	Engine->InitializeMatch(Config);
	return Engine;
}

static AGomokuGameState* MakeStage8GameState(UWorld*& OutWorld)
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

	GameState->SetRuleEngineRef(MakeStage8Engine());
	GameState->InitializeForLocalHotseat(2);
	return GameState;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage8_EventLogHasSequenceAndStoneEvent,
	TEXT("Gomoku.Stage8.EventLogHasSequenceAndStoneEvent"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage8_EventLogHasSequenceAndStoneEvent::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage8GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	if (!TestTrue(TEXT("Event log created"), IsValid(GameState->EventLog)))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameState->HandlePlaceStone(0, FIntPoint(4, 4));
	const TArray<FMatchEvent>& Events = GameState->EventLog->GetEvents();
	const FMatchEvent* StoneEvent = nullptr;
	for (const FMatchEvent& Event : Events)
	{
		if (Event.Type == EMatchEventType::StonePlaced)
		{
			StoneEvent = &Event;
			break;
		}
	}

	const bool bValid =
		TestTrue(TEXT("Turn and stone events were recorded"), Events.Num() >= 3) &&
		TestNotNull(TEXT("Stone event exists"), StoneEvent) &&
		TestEqual(TEXT("First event sequence is one"), Events[0].SequenceId, 1) &&
		TestEqual(TEXT("First event is turn start"), Events[0].Type, EMatchEventType::TurnStarted) &&
		TestEqual(TEXT("Stone event instigator is player one"), StoneEvent->InstigatorPlayerId, 1) &&
		TestEqual(TEXT("Stone event target cell X"), StoneEvent->TargetCell.X, 4) &&
		TestEqual(TEXT("Stone event sequence is monotonic"), Events.Last().SequenceId, Events.Num());

	World->DestroyWorld(false);
	return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage8_ItemRequestIsServerValidatedAndLogged,
	TEXT("Gomoku.Stage8.ItemRequestIsServerValidatedAndLogged"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage8_ItemRequestIsServerValidatedAndLogged::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage8GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	UGomokuRuleEngine* Engine = GameState->GetRuleEngine();
	const int32 ItemId = 1;
	Engine->AddItemToInventory(1, ItemId);
	Engine->ClearTurnItemLocksForPlayer(1);
	const int32 EnergyBefore = Engine->GetPlayerStateData(1).Energy;

	// Player 2 cannot submit an item request during player 1's turn.
	GameState->HandleUseItem(1, ItemId, FIntPoint(3, 3), -1);
	if (!TestEqual(TEXT("Unauthorized turn leaves energy unchanged"), Engine->GetPlayerStateData(1).Energy, EnergyBefore))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameState->HandleUseItem(0, ItemId, FIntPoint(3, 3), -1);
	const TArray<FMatchEvent>& Events = GameState->EventLog->GetEvents();
	const FMatchEvent& LastEvent = Events.Last();
	const bool bValid =
		TestEqual(TEXT("Item consumes energy on valid request"), Engine->GetPlayerStateData(1).Energy, EnergyBefore - 1) &&
		TestEqual(TEXT("Target cell is sealed"), Engine->GetCellState(3, 3), ECellState::Blocked) &&
		TestEqual(TEXT("Last event is item used"), LastEvent.Type, EMatchEventType::ItemUsed) &&
		TestEqual(TEXT("Logged item id"), LastEvent.ItemId, FName(TEXT("1"))) &&
		TestEqual(TEXT("Logged instigator"), LastEvent.InstigatorPlayerId, 1);

	World->DestroyWorld(false);
	return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage8_ItemWinEndsMatch,
	TEXT("Gomoku.Stage8.ItemWinEndsMatch"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage8_ItemWinEndsMatch::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	AGomokuGameState* GameState = MakeStage8GameState(World);
	if (!TestNotNull(TEXT("GameState created"), GameState))
		return false;

	UGomokuRuleEngine* Engine = GameState->GetRuleEngine();
	Engine->ForcePlaceStone(1, 0, 0);
	Engine->ForcePlaceStone(1, 0, 1);
	Engine->ForcePlaceStone(1, 0, 2);
	Engine->ForcePlaceStone(1, 0, 3);
	Engine->ForcePlaceStone(2, 0, 4);
	Engine->AddItemToInventory(1, 3);
	Engine->ClearTurnItemLocksForPlayer(1);
	GameState->SyncReplicatedBoard();

	GameState->HandleUseItem(0, 3, FIntPoint(0, 4), -1);
	const bool bValid =
		TestFalse(TEXT("Item win stops the match"), GameState->IsGameActive) &&
		TestEqual(TEXT("Item win winner is player one"), GameState->WinnerPlayerIndex, 0) &&
		TestEqual(TEXT("Item win enters game over"), GameState->MatchPhase, EMatchPhase::GameOver) &&
		TestEqual(TEXT("Converted cell belongs to player one"), Engine->GetCellState(0, 4), ECellState::Player1);

	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
