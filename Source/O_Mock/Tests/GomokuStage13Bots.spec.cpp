// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuBotLibrary.h"
#include "GomokuItemLibrary.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"

static UGomokuRuleEngine* MakeStage13Engine(int32 MaxPlayers)
{
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(GetTransientPackage());
	FGomokuMatchConfig Config;
	Config.BoardSizeX = 9;
	Config.BoardSizeY = 9;
	Config.WinLength = 5;
	Config.MaxPlayers = MaxPlayers;
	Config.InitialEnergyPerPlayer = 0;
	Engine->InitializeMatch(Config);
	return Engine;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage13_BotPrioritizesWinAndBlock,
	TEXT("Gomoku.Stage13.BotPrioritizesWinAndBlock"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage13_BotPrioritizesWinAndBlock::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* WinningEngine = MakeStage13Engine(2);
	for (int32 X = 0; X < 4; ++X)
	{
		WinningEngine->ForcePlaceStone(1, X, 0);
	}
	FIntPoint Move;
	if (!TestTrue(TEXT("Bot finds immediate winning move"), UGomokuBotLibrary::ChooseMove(WinningEngine, 1, Move)) ||
		!TestEqual(TEXT("Bot completes own line"), Move, FIntPoint(4, 0)))
	{
		return false;
	}

	UGomokuRuleEngine* BlockingEngine = MakeStage13Engine(2);
	for (int32 X = 0; X < 4; ++X)
	{
		BlockingEngine->ForcePlaceStone(2, X, 1);
	}
	if (!TestTrue(TEXT("Bot finds immediate block"), UGomokuBotLibrary::ChooseMove(BlockingEngine, 1, Move)) ||
		!TestEqual(TEXT("Bot blocks opponent line"), Move, FIntPoint(4, 1)))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage13_BotsCompleteRepeatedTwoThreeFourPlayerGames,
	TEXT("Gomoku.Stage13.BotsCompleteRepeatedTwoThreeFourPlayerGames"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage13_BotsCompleteRepeatedTwoThreeFourPlayerGames::RunTest(const FString& Parameters)
{
	for (int32 PlayerCount = 2; PlayerCount <= 4; ++PlayerCount)
	{
		for (int32 GameIndex = 0; GameIndex < 100; ++GameIndex)
		{
			UGomokuRuleEngine* Engine = MakeStage13Engine(PlayerCount);
			const int32 MaxTurns = 9 * 9 + 1;
			int32 Turns = 0;
			while (!Engine->IsGameOver() && Turns < MaxTurns)
			{
				const int32 PlayerId = Engine->GetCurrentPlayerId();
				FIntPoint Move;
				if (!TestTrue(FString::Printf(TEXT("Bot chooses a move P%d game %d turn %d"), PlayerCount, GameIndex, Turns),
					UGomokuBotLibrary::TakeTurn(Engine, PlayerId, Move)))
				{
					return false;
				}
				++Turns;
				if (!Engine->IsGameOver())
				{
					Engine->AdvanceTurn();
				}
			}

			if (!TestTrue(FString::Printf(TEXT("Bot game %d with %d players terminates"), GameIndex, PlayerCount), Engine->IsGameOver()))
			{
				return false;
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage13_BotUsesItemsByProbability,
	TEXT("Gomoku.Stage13.BotUsesItemsByProbability"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage13_BotUsesItemsByProbability::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* UsesItemEngine = MakeStage13Engine(2);
	UsesItemEngine->AddPlayerEnergy(1, 5, UGomokuItemLibrary::MaxEnergy);
	UsesItemEngine->AddItemToInventory(1, 1);
	FRandomStream AlwaysUseRandom(1337);
	FGomokuBotItemAction Action;
	FIntPoint Move;
	if (!TestTrue(TEXT("Bot turn completes with item policy"), UGomokuBotLibrary::TakeTurnWithItems(
		UsesItemEngine, 1, 1.0f, AlwaysUseRandom, 1, Action, Move))
		|| !TestTrue(TEXT("Probability one uses a valid item"), Action.bUsedItem)
		|| !TestEqual(TEXT("Owned Seal item is selected"), Action.ItemId, 1)
		|| !TestFalse(TEXT("Used item is consumed"), UsesItemEngine->PlayerHasItem(1, 1)))
	{
		return false;
	}

	UGomokuRuleEngine* KeepsItemEngine = MakeStage13Engine(2);
	KeepsItemEngine->AddPlayerEnergy(1, 5, UGomokuItemLibrary::MaxEnergy);
	KeepsItemEngine->AddItemToInventory(1, 1);
	FRandomStream NeverUseRandom(1337);
	Action = FGomokuBotItemAction();
	return TestTrue(TEXT("Bot still places when probability is zero"), UGomokuBotLibrary::TakeTurnWithItems(
		KeepsItemEngine, 1, 0.0f, NeverUseRandom, 1, Action, Move))
		&& TestFalse(TEXT("Probability zero skips item use"), Action.bUsedItem)
		&& TestTrue(TEXT("Skipped item remains owned"), KeepsItemEngine->PlayerHasItem(1, 1));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage13_BotItemPolicyRejectsNonCurrentPlayerWithoutMutation,
	TEXT("Gomoku.Stage13.BotItemPolicyRejectsNonCurrentPlayerWithoutMutation"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage13_BotItemPolicyRejectsNonCurrentPlayerWithoutMutation::RunTest(const FString& Parameters)
{
	UGomokuRuleEngine* Engine = MakeStage13Engine(2);
	Engine->AddPlayerEnergy(2, 5, UGomokuItemLibrary::MaxEnergy);
	Engine->AddItemToInventory(2, 1);
	Engine->ClearTurnItemLocksForPlayer(2);
	const int32 EnergyBefore = Engine->GetPlayerStateData(2).Energy;
	FRandomStream Random(7);
	FGomokuBotItemAction Action;
	FIntPoint Move;
	const bool bTurnResult = UGomokuBotLibrary::TakeTurnWithItems(
		Engine, 2, 1.0f, Random, 1, Action, Move);
	return TestFalse(TEXT("Non-current bot turn is rejected"), bTurnResult)
		&& TestFalse(TEXT("Rejected bot does not report item use"), Action.bUsedItem)
		&& TestEqual(TEXT("Rejected bot preserves energy"), Engine->GetPlayerStateData(2).Energy, EnergyBefore)
		&& TestTrue(TEXT("Rejected bot preserves inventory"), Engine->PlayerHasItem(2, 1))
		&& TestEqual(TEXT("Rejected bot leaves target cells unchanged"),
			Engine->GetCellState(0, 0), ECellState::Empty);
}

#endif // WITH_DEV_AUTOMATION_TESTS
