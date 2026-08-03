// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "GomokuItemTypes.h"
#include "GomokuItemLibrary.h"

static UGomokuRuleEngine* MakeStage7Engine(int32 MaxPlayers, int32 BoardSize = 15)
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

// Test: Gomoku.Stage7.SealStoneBlocksCell
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_SealStoneBlocksCell,
	TEXT("Gomoku.Stage7.SealStoneBlocksCell"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_SealStoneBlocksCell::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 SealStoneId = 1;

	bool Added = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("SealStone added to inventory"), Added))
		return false;

	FIntPoint TargetCell(5, 5);
	int32 TargetPlayerIndex = -1;

	if (!TestTrue(TEXT("CanUseItem for SealStone before execute"),
		UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
	{
		return false;
	}

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, SealStoneId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("ExecuteItem succeeds for SealStone"), Executed))
		return false;

	const auto Cell = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
	if (Cell.State == ECellState::Empty)
	{
		AddError(TEXT("SealStone should block the target cell"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.SkipTurnFlagsPlayer
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_SkipTurnFlagsPlayer,
	TEXT("Gomoku.Stage7.SkipTurnFlagsPlayer"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_SkipTurnFlagsPlayer::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 SkipTurnItemId = 4;

	bool Added = Engine->AddItemToInventory(PlayerId, SkipTurnItemId);
	if (!TestTrue(TEXT("SkipTurn added to inventory"), Added))
		return false;

	const int32 TargetPlayerIndex = 1; // maps to PlayerId 2 in ExecuteItem logic.

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, SkipTurnItemId, PlayerId, FIntPoint(0, 0), TargetPlayerIndex);
	if (!TestTrue(TEXT("ExecuteItem succeeds for SkipTurn"), Executed))
		return false;

	const auto P2State = Engine->GetPlayerStateData(2);
	if (!TestTrue(TEXT("Player 2 bSkipNextTurn is true after SkipTurn"), P2State.bSkipNextTurn))
	{
		AddError(TEXT("SkipTurn should set bSkipNextTurn on target player"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.NewItemNotUsableSameTurn
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_NewItemNotUsableSameTurn,
	TEXT("Gomoku.Stage7.NewItemNotUsableSameTurn"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_NewItemNotUsableSameTurn::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 SealStoneId = 1;

	bool Added = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("SealStone added"), Added))
		return false;

	Engine->MarkItemGainedThisTurn(PlayerId, SealStoneId);

	if (!TestFalse(TEXT("CanUseItem is false when item gained this turn"),
		UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
	{
		return false;
	}

	Engine->ClearTurnItemLocksForPlayer(PlayerId);

	if (!TestTrue(TEXT("CanUseItem is true after ClearTurnItemLocksForPlayer"),
		UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
	{
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.EnergyCostConsumed
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_EnergyCostConsumed,
	TEXT("Gomoku.Stage7.EnergyCostConsumed"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_EnergyCostConsumed::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 SealStoneId = 1;

	bool Added = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("SealStone added"), Added))
		return false;

	const auto StateBefore = Engine->GetPlayerStateData(PlayerId);
	const int32 EnergyBefore = StateBefore.Energy;

	FIntPoint TargetCell(5, 5);
	int32 TargetPlayerIndex = -1;

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, SealStoneId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("ExecuteItem succeeds"), Executed))
		return false;

	const auto StateAfter = Engine->GetPlayerStateData(PlayerId);
	const int32 EnergyAfter = StateAfter.Energy;

	if (!TestEqual<int32>(TEXT("Energy decreased by 1 after SealStone"), EnergyBefore - EnergyAfter, 1))
		return false;

	if (Engine->PlayerHasItem(PlayerId, SealStoneId))
	{
		AddError(TEXT("SealStone should be removed from inventory after execute"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.PullMovesAdjacentStone
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_PullMovesAdjacentStone,
	TEXT("Gomoku.Stage7.PullMovesAdjacentStone"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_PullMovesAdjacentStone::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 PullItemId = 2;

	// Place opponent stone at (6,5) to be pulled.
	Engine->ForcePlaceStone(2, 6, 5);

	bool Added = Engine->AddItemToInventory(PlayerId, PullItemId);
	if (!TestTrue(TEXT("Pull item added"), Added))
		return false;

	const int32 EnergyBefore = Engine->GetPlayerStateData(PlayerId).Energy;

	FIntPoint TargetCell(5, 5); // pull destination
	int32 TargetPlayerIndex = -1;

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, PullItemId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("Pull execute succeeds"), Executed))
		return false;

	const auto SrcCell = Engine->GetBoardCell(6, 5);
	if (SrcCell.State != ECellState::Empty)
	{
		AddError(TEXT("Source cell should be empty after pull"));
		return false;
	}

	const auto DstCell = Engine->GetBoardCell(5, 5);
	if (UGomokuRuleEngine::CellStateToPlayerId(DstCell.State) != 2)
	{
		AddError(TEXT("Pulled stone should belong to player 2 at destination"));
		return false;
	}

	const int32 EnergyAfter = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("Energy decreased by 1 after Pull"), EnergyBefore - EnergyAfter, 1))
		return false;

	if (Engine->PlayerHasItem(PlayerId, PullItemId))
	{
		AddError(TEXT("Pull item should be removed from inventory"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.PullFailsLeavesStateUnchanged
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_PullFailsLeavesStateUnchanged,
	TEXT("Gomoku.Stage7.PullFailsLeavesStateUnchanged"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_PullFailsLeavesStateUnchanged::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 PullItemId = 2;

	bool Added = Engine->AddItemToInventory(PlayerId, PullItemId);
	if (!TestTrue(TEXT("Pull item added"), Added))
		return false;

	const int32 EnergyBefore = Engine->GetPlayerStateData(PlayerId).Energy;

	FIntPoint TargetCell(7, 7); // empty area with no adjacent stone to pull
	int32 TargetPlayerIndex = -1;

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, PullItemId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestFalse(TEXT("Pull should fail when no valid target"), Executed))
		return false;

	const int32 EnergyAfter = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("Energy unchanged after failed Pull"), EnergyBefore, EnergyAfter))
		return false;

	if (!Engine->PlayerHasItem(PlayerId, PullItemId))
	{
		AddError(TEXT("Pull item should still be in inventory after failure"));
		return false;
	}

	const auto Cell = Engine->GetBoardCell(7, 7);
	if (Cell.State != ECellState::Empty)
	{
		AddError(TEXT("Target cell must remain empty when Pull fails"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.StealBlockedByGuardian
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_StealBlockedByGuardian,
	TEXT("Gomoku.Stage7.StealBlockedByGuardian"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_StealBlockedByGuardian::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 StealItemId = 3;

	// Place opponent stone at (4,4).
	Engine->ForcePlaceStone(2, 4, 4);

	// Mark it as guardian-protected.
	Engine->SetCellGuardianProtected(4, 4, true);

	bool Added = Engine->AddItemToInventory(PlayerId, StealItemId);
	if (!TestTrue(TEXT("Steal item added"), Added))
		return false;

	const int32 EnergyBefore = Engine->GetPlayerStateData(PlayerId).Energy;

	FIntPoint TargetCell(4, 4);
	int32 TargetPlayerIndex = -1;

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, StealItemId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestFalse(TEXT("Steal should fail on guardian-protected cell"), Executed))
		return false;

	const auto Cell = Engine->GetBoardCell(4, 4);
	if (UGomokuRuleEngine::CellStateToPlayerId(Cell.State) != 2)
	{
		AddError(TEXT("Guardian-protected stone must remain owned by player 2"));
		return false;
	}

	const int32 EnergyAfter = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("Energy unchanged after failed Steal"), EnergyBefore, EnergyAfter))
		return false;

	if (!Engine->PlayerHasItem(PlayerId, StealItemId))
	{
		AddError(TEXT("Steal item should still be in inventory after failure"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.StealRechecksWin
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_StealRechecksWin,
	TEXT("Gomoku.Stage7.StealRechecksWin"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_StealRechecksWin::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 StealItemId = 3;

	// Set up: player1 has 4 in a row at (0,0)-(0,3), player2 blocks at (0,4).
	Engine->ForcePlaceStone(1, 0, 0);
	Engine->ForcePlaceStone(1, 0, 1);
	Engine->ForcePlaceStone(1, 0, 2);
	Engine->ForcePlaceStone(1, 0, 3);
	Engine->ForcePlaceStone(2, 0, 4);

	bool Added = Engine->AddItemToInventory(PlayerId, StealItemId);
	if (!TestTrue(TEXT("Steal item added"), Added))
		return false;

	FIntPoint TargetCell(0, 4); // steal opponent's blocking stone
	int32 TargetPlayerIndex = -1;

	bool Executed = UGomokuItemLibrary::ExecuteItem(Engine, StealItemId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("Steal execute succeeds on (0,4)"), Executed))
		return false;

	const auto WinResult = Engine->GetLastItemWinResult();
	const bool IsWinViaResult = WinResult.IsWin;

	const FIntPoint CheckPos(0, 4);
	const auto LocalCheck = Engine->CheckWinAt(CheckPos);
	const bool IsWinViaLocal = LocalCheck.IsWin;

	if (!IsWinViaResult && !IsWinViaLocal)
	{
		AddError(TEXT("Steal should trigger win recheck and detect a win at (0,4)"));
		return false;
	}

	return true;
}

// Test: Gomoku.Stage7.SecondItemUseRejectedSameTurn
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage7_SecondItemUseRejectedSameTurn,
	TEXT("Gomoku.Stage7.SecondItemUseRejectedSameTurn"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage7_SecondItemUseRejectedSameTurn::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage7Engine(2, 15);
	const int32 PlayerId = 1;
	const int32 SealStoneId = 1;

	// First use: add and execute a SealStone.
	bool Added1 = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("First SealStone added"), Added1))
		return false;

	FIntPoint TargetCell(8, 8);
	int32 TargetPlayerIndex = -1;

	const int32 EnergyBefore = Engine->GetPlayerStateData(PlayerId).Energy;

	bool Executed1 = UGomokuItemLibrary::ExecuteItem(Engine, SealStoneId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("First SealStone execute succeeds"), Executed1))
		return false;

	const int32 EnergyAfterFirst = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("Energy decreased by 1 after first use"), EnergyBefore - EnergyAfterFirst, 1))
		return false;

	// Second item: add another SealStone in same turn.
	bool Added2 = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("Second SealStone added"), Added2))
		return false;

	// Must not be usable immediately (same-turn restriction).
	if (!TestFalse(TEXT("CanUseItem is false for second item same turn"),
		UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
	{
		return false;
	}

	bool Executed2 = UGomokuItemLibrary::ExecuteItem(Engine, SealStoneId, PlayerId, TargetCell, TargetPlayerIndex);
	if (Executed2)
	{
		AddError(TEXT("Second item use should be rejected in same turn"));
		return false;
	}

	const int32 EnergyAfterSecond = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("Energy only decreased by 1 total (no second cost)"), EnergyBefore - EnergyAfterSecond, 1))
		return false;

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
