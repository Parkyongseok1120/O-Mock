// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuRuleEngine.h"
#include "GomokuItemLibrary.h"

ECellState UGomokuRuleEngine::PlayerIdToCellState(int32 PlayerId)
{
	switch (PlayerId)
	{
	case 1: return ECellState::Player1;
	case 2: return ECellState::Player2;
	case 3: return ECellState::Player3;
	case 4: return ECellState::Player4;
	default: return ECellState::Empty;
	}
}

int32 UGomokuRuleEngine::CellStateToPlayerId(ECellState State)
{
	switch (State)
	{
	case ECellState::Player1: return 1;
	case ECellState::Player2: return 2;
	case ECellState::Player3: return 3;
	case ECellState::Player4: return 4;
	default: return 0;
	}
}

void UGomokuRuleEngine::InitializeMatch(const FGomokuMatchConfig& Config)
{
	TurnDirection = 1;
	GuardianProtectionExpireRounds.Reset();
	TemporaryBlockedCellExpireRounds.Reset();
	LastItemWinResult = FGomokuWinResult();

	MatchConfig = Config;
	MatchConfig.BoardSizeX = FMath::Max(1, MatchConfig.BoardSizeX);
	MatchConfig.BoardSizeY = FMath::Max(1, MatchConfig.BoardSizeY);
	MatchConfig.WinLength = FMath::Max(1, MatchConfig.WinLength);
	MatchConfig.MaxPlayers = FMath::Clamp(MatchConfig.MaxPlayers, 2, 4);
	MatchConfig.InitialEnergyPerPlayer = FMath::Clamp(MatchConfig.InitialEnergyPerPlayer, 0, UGomokuItemLibrary::MaxEnergy);

	Board.Reset(MatchConfig.BoardSizeX, MatchConfig.BoardSizeY);

	Players.Reset();
	PlayerOrder.Reset();
	for (int32 PlayerId = 1; PlayerId <= MatchConfig.MaxPlayers; ++PlayerId)
	{
		Players.Add(FGomokuPlayerStateData::CreateDefault(
			PlayerId,
			MatchConfig.TurnTimeLimit,
			MatchConfig.InitialEnergyPerPlayer));
		PlayerOrder.Add(PlayerId);
	}

	InitializeActivePlayerIndices();

	// Build O(1) PlayerId -> index map.
	PlayerIdToIndex.Reset();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		PlayerIdToIndex.Add(Players[i].PlayerId, i);
	}

	// Apply template blocked cells (if any) after board reset.
	for (const FIntPoint& BC : MatchConfig.BlockedCells)
	{
		if (!Board.IsInside(BC.X, BC.Y))
			continue;
		const ECellState Current = Board.Get(BC.X, BC.Y);
		if (Current == ECellState::Empty)
		{
			Board.Set(BC.X, BC.Y, ECellState::Blocked);
		}
	}

	CurrentPlayerIndex = 0;
	bInitialized = true;
}

void UGomokuRuleEngine::InitializeActivePlayerIndices()
{
	ActivePlayerIndices.Reset();
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const FGomokuPlayerStateData& P = Players[i];
		if (!P.bHasAbandoned)
		{
			ActivePlayerIndices.Add(i);
		}
	}
}

int32 UGomokuRuleEngine::GetSoleActivePlayerIndex() const
{
	if (ActivePlayerIndices.Num() == 1)
	{
		return ActivePlayerIndices[0];
	}
	return INDEX_NONE;
}

bool UGomokuRuleEngine::IsValidCoordinate(int32 X, int32 Y) const
{
	return bInitialized && Board.IsInside(X, Y);
}

ECellState UGomokuRuleEngine::GetCellState(int32 X, int32 Y) const
{
	return Board.Get(X, Y);
}

FBoardCell UGomokuRuleEngine::GetBoardCell(int32 X, int32 Y) const
{
	return FBoardCell(X, Y, GetCellState(X, Y));
}

bool UGomokuRuleEngine::TryPlaceStone(int32 PlayerId, int32 X, int32 Y)
{
	if (!bInitialized || IsGameOver() || !IsValidCoordinate(X, Y))
	{
		return false;
	}

	if (GetCurrentPlayerId() != PlayerId)
	{
		return false;
	}

	if (GetCellState(X, Y) != ECellState::Empty)
	{
		return false;
	}

	const ECellState NewState = PlayerIdToCellState(PlayerId);
	if (NewState == ECellState::Empty)
	{
		return false;
	}

	Board.Set(X, Y, NewState);
	return true;
}

bool UGomokuRuleEngine::RemoveStoneAt(int32 X, int32 Y)
{
	if (!IsValidCoordinate(X, Y))
	{
		return false;
	}

	const ECellState Current = GetCellState(X, Y);
	if (Current == ECellState::Empty || Current == ECellState::Blocked)
	{
		return false;
	}

	Board.Set(X, Y, ECellState::Empty);
	GuardianProtectionExpireRounds.Remove(FIntPoint(X, Y));
	return true;
}

bool UGomokuRuleEngine::ChangeCellOwnership(int32 X, int32 Y, int32 NewPlayerId)
{
	if (!IsActiveMatchPlayer(NewPlayerId))
	{
		return false;
	}

	if (!IsValidCoordinate(X, Y))
	{
		return false;
	}

	const ECellState Current = GetCellState(X, Y);
	if (Current == ECellState::Empty || Current == ECellState::Blocked)
	{
		return false;
	}

	const ECellState NewState = PlayerIdToCellState(NewPlayerId);
	if (NewState == ECellState::Empty)
	{
		return false;
	}

	Board.Set(X, Y, NewState);
	return true;
}

bool UGomokuRuleEngine::ForcePlaceStone(int32 PlayerId, int32 X, int32 Y)
{
	if (!IsActiveMatchPlayer(PlayerId))
	{
		return false;
	}

	if (!bInitialized || !IsValidCoordinate(X, Y))
	{
		return false;
	}
	if (GetCellState(X, Y) != ECellState::Empty)
	{
		return false;
	}
	const ECellState NewState = PlayerIdToCellState(PlayerId);
	if (NewState == ECellState::Empty)
	{
		return false;
	}
	Board.Set(X, Y, NewState);
	return true;
}

bool UGomokuRuleEngine::SetCellBlocked(int32 X, int32 Y, bool bBlocked)
{
	if (!IsValidCoordinate(X, Y))
	{
		return false;
	}
	if (bBlocked)
	{
		if (GetCellState(X, Y) != ECellState::Empty)
		{
			return false;
		}
		Board.Set(X, Y, ECellState::Blocked);
		return true;
	}
	if (GetCellState(X, Y) == ECellState::Blocked)
	{
		Board.Set(X, Y, ECellState::Empty);
		TemporaryBlockedCellExpireRounds.Remove(FIntPoint(X, Y));
		return true;
	}
	return false;
}

bool UGomokuRuleEngine::MoveStone(int32 PlayerId, const FIntPoint& Source, const FIntPoint& Destination)
{
	if (!IsActiveMatchPlayer(PlayerId) || !IsValidCoordinate(Source.X, Source.Y) ||
		!IsValidCoordinate(Destination.X, Destination.Y) || IsCellGuardianProtected(Source.X, Source.Y))
	{
		return false;
	}
	if (CellStateToPlayerId(GetCellState(Source.X, Source.Y)) != PlayerId ||
		GetCellState(Destination.X, Destination.Y) != ECellState::Empty)
	{
		return false;
	}

	const ECellState StoneState = GetCellState(Source.X, Source.Y);
	Board.Set(Destination.X, Destination.Y, StoneState);
	Board.Set(Source.X, Source.Y, ECellState::Empty);
	GuardianProtectionExpireRounds.Remove(Source);
	return true;
}

bool UGomokuRuleEngine::IsStoneIsolated(const FIntPoint& Cell) const
{
	const ECellState Center = GetCellState(Cell.X, Cell.Y);
	if (Center < ECellState::Player1 || Center > ECellState::Player4)
	{
		return false;
	}
	for (int32 DeltaY = -1; DeltaY <= 1; ++DeltaY)
	{
		for (int32 DeltaX = -1; DeltaX <= 1; ++DeltaX)
		{
			if (DeltaX == 0 && DeltaY == 0)
			{
				continue;
			}
			const ECellState Neighbour = GetCellState(Cell.X + DeltaX, Cell.Y + DeltaY);
			if (Neighbour == Center)
			{
				return false;
			}
		}
	}
	return true;
}

bool UGomokuRuleEngine::ConsumePlayerEnergy(int32 PlayerId, int32 Cost)
{
	if (Cost < 0)
	{
		return false;
	}
	if (FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId))
	{
		if (Player->Energy < Cost)
		{
			return false;
		}
		Player->Energy -= Cost;
		return true;
	}
	return false;
}

bool UGomokuRuleEngine::AddPlayerEnergy(int32 PlayerId, int32 Amount, int32 MaxEnergy)
{
	if (Amount < 0 || MaxEnergy < 0)
	{
		return false;
	}
	if (FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId))
	{
		Player->Energy = FMath::Min(MaxEnergy, Player->Energy + Amount);
		return true;
	}
	return false;
}

bool UGomokuRuleEngine::PlayerHasItem(int32 PlayerId, int32 ItemId) const
{
	if (ItemId <= 0 || !bInitialized)
		return false;

	const FGomokuPlayerStateData* Player = FindPlayer(PlayerId);
	if (!Player)
		return false;

	return Player->ItemIds.Contains(ItemId);
}

bool UGomokuRuleEngine::RemoveItemFromInventory(int32 PlayerId, int32 ItemId)
{
	if (ItemId <= 0 || !bInitialized)
		return false;

	FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId);
	if (!Player)
		return false;

	if (!Player->ItemIds.Contains(ItemId))
		return false;

	Player->ItemIds.RemoveSingle(ItemId);
	return true;
}

bool UGomokuRuleEngine::AddItemToInventory(int32 PlayerId, int32 ItemId)
{
	if (ItemId <= 0 || !bInitialized)
		return false;

	// Contract: only registered item IDs may enter inventory.
	if (!UGomokuItemLibrary::IsRegisteredItemId(ItemId))
		return false;

	FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId);
	if (!Player)
		return false;

	if (Player->ItemIds.Num() >= 2 || Player->ItemIds.Contains(ItemId))
	{
		return false;
	}
	Player->ItemIds.Add(ItemId);
	return true;
}

bool UGomokuRuleEngine::SetPendingInventoryItem(int32 PlayerId, int32 ItemId)
{
	if (!bInitialized || !UGomokuItemLibrary::IsRegisteredItemId(ItemId))
	{
		return false;
	}
	FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId);
	if (!Player || Player->PendingInventoryItemId > 0 || Player->ItemIds.Contains(ItemId))
	{
		return false;
	}
	Player->PendingInventoryItemId = ItemId;
	return true;
}

bool UGomokuRuleEngine::ReplaceInventoryItemWithPending(int32 PlayerId, int32 DiscardItemId)
{
	FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId);
	if (!bInitialized || !Player || Player->PendingInventoryItemId <= 0
		|| !UGomokuItemLibrary::IsRegisteredItemId(Player->PendingInventoryItemId)
		|| !Player->ItemIds.Contains(DiscardItemId))
	{
		return false;
	}

	const int32 NewItemId = Player->PendingInventoryItemId;
	if (Player->ItemIds.Contains(NewItemId))
	{
		return false;
	}
	Player->ItemIds.RemoveSingle(DiscardItemId);
	Player->ItemIdsGainedThisTurn.Remove(DiscardItemId);
	Player->ItemIds.Add(NewItemId);
	Player->ItemIdsGainedThisTurn.Add(NewItemId);
	Player->PendingInventoryItemId = 0;
	return true;
}

bool UGomokuRuleEngine::ClaimPendingInventoryItemIntoFreeSlot(int32 PlayerId)
{
	FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId);
	if (!bInitialized || !Player || Player->PendingInventoryItemId <= 0 || Player->ItemIds.Num() >= 2)
	{
		return false;
	}
	const int32 NewItemId = Player->PendingInventoryItemId;
	if (!UGomokuItemLibrary::IsRegisteredItemId(NewItemId) || Player->ItemIds.Contains(NewItemId))
	{
		return false;
	}
	Player->ItemIds.Add(NewItemId);
	Player->ItemIdsGainedThisTurn.Add(NewItemId);
	Player->PendingInventoryItemId = 0;
	return true;
}

int32 UGomokuRuleEngine::GetCurrentPlayerId() const
{
	if (!bInitialized || !PlayerOrder.IsValidIndex(CurrentPlayerIndex))
	{
		return 0;
	}
	return PlayerOrder[CurrentPlayerIndex];
}

void UGomokuRuleEngine::AdvanceTurn()
{
	if (!bInitialized || ActivePlayerIndices.Num() == 0)
	{
		return;
	}

	const int32 MaxAttempts = PlayerOrder.Num();
	for (int32 Step = 0; Step < MaxAttempts; ++Step)
	{
		int32 CurrentActivePos = INDEX_NONE;
		for (int32 i = 0; i < ActivePlayerIndices.Num(); ++i)
		{
			if (ActivePlayerIndices[i] == CurrentPlayerIndex)
			{
				CurrentActivePos = i;
				break;
			}
		}

		const int32 Dir = (TurnDirection >= 0) ? 1 : -1;
		int32 NextIndex = CurrentPlayerIndex;

		if (CurrentActivePos != INDEX_NONE)
		{
			const int32 A = ActivePlayerIndices.Num();
			int32 NextActivePos = (CurrentActivePos + Dir) % A;
			if (NextActivePos < 0)
			{
				NextActivePos += A;
			}
			NextIndex = ActivePlayerIndices[NextActivePos];
		}
		else
		{
			// CurrentPlayerIndex may refer to a player who just abandoned. Walk the
			// original player order so wrap-around still selects the adjacent active slot.
			NextIndex = (CurrentPlayerIndex + Dir) % PlayerOrder.Num();
			if (NextIndex < 0)
			{
				NextIndex += PlayerOrder.Num();
			}
		}

		CurrentPlayerIndex = NextIndex;

		const int32 NextPlayerId = PlayerOrder[CurrentPlayerIndex];
		if (NextPlayerId <= 0)
		{
			continue;
		}

		if (FGomokuPlayerStateData* Player = FindPlayerMutable(NextPlayerId))
		{
			if (Player->bHasAbandoned)
			{
				continue;
			}

			if (Player->bSkipNextTurn)
			{
				Player->bSkipNextTurn = false;
				Player->ItemIdsGainedThisTurn.Reset();
				Player->bUsedItemThisTurn = false;
				continue;
			}
		}

		return;
	}
}

int32 UGomokuRuleEngine::AdvanceTurnIndex(int32 CurrentIndex, int32 Direction) const
{
	if (ActivePlayerIndices.Num() == 0 || !bInitialized)
	{
		return 0;
	}

	int32 ActivePos = INDEX_NONE;
	for (int32 i = 0; i < ActivePlayerIndices.Num(); ++i)
	{
		if (ActivePlayerIndices[i] == CurrentIndex)
		{
			ActivePos = i;
			break;
		}
	}

	if (ActivePos == INDEX_NONE)
	{
		ActivePos = 0;
	}

	const int32 Dir = (Direction >= 0) ? 1 : -1;
	int32 NextPos = ActivePos + Dir;
	const int32 A = ActivePlayerIndices.Num();
	NextPos %= A;
	if (NextPos < 0)
	{
		NextPos += A;
	}

	return ActivePlayerIndices[NextPos];
}

void UGomokuRuleEngine::ReverseTurnDirection()
{
	if (!bInitialized || PlayerOrder.Num() == 0)
	{
		return;
	}
	TurnDirection = (TurnDirection != 0) ? -TurnDirection : 1;
}

bool UGomokuRuleEngine::IsActiveMatchPlayer(int32 PlayerId) const
{
	const FGomokuPlayerStateData* P = FindPlayer(PlayerId);
	return P != nullptr && !P->bHasAbandoned;
}

FGomokuPlayerStateData UGomokuRuleEngine::GetPlayerStateData(int32 PlayerId) const
{
	if (const FGomokuPlayerStateData* Player = FindPlayer(PlayerId))
	{
		return *Player;
	}
	return FGomokuPlayerStateData();
}

bool UGomokuRuleEngine::SetPlayerSkipNextTurn(int32 PlayerId, bool bSkip)
{
	if (FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId))
	{
		if (bSkip && (!IsActiveMatchPlayer(PlayerId) || Player->bSkipNextTurn))
		{
			return false;
		}
		Player->bSkipNextTurn = bSkip;
		return true;
	}
	return false;
}

int32 UGomokuRuleEngine::AbandonPlayer(int32 PlayerId)
{
	if (FGomokuPlayerStateData* Player = FindPlayerMutable(PlayerId))
	{
		Player->bHasAbandoned = true;
	}
	InitializeActivePlayerIndices();
	return ActivePlayerIndices.Num();
}

int32 UGomokuRuleEngine::CountRay(int32 X, int32 Y, int32 DX, int32 DY, ECellState State) const
{
	int32 Count = 0;
	int32 CX = X + DX;
	int32 CY = Y + DY;
	while (IsValidCoordinate(CX, CY) && GetCellState(CX, CY) == State)
	{
		++Count;
		CX += DX;
		CY += DY;
	}
	return Count;
}

bool UGomokuRuleEngine::HasWinAt(int32 X, int32 Y) const
{
	const ECellState State = GetCellState(X, Y);
	if (State == ECellState::Empty || State == ECellState::Blocked)
	{
		return false;
	}

	static const FIntPoint Directions[4] = {
		FIntPoint(1, 0),
		FIntPoint(0, 1),
		FIntPoint(1, 1),
		FIntPoint(1, -1)
	};

	for (const FIntPoint& Dir : Directions)
	{
		const int32 Total = 1
			+ CountRay(X, Y, Dir.X, Dir.Y, State)
			+ CountRay(X, Y, -Dir.X, -Dir.Y, State);
		if (Total >= MatchConfig.WinLength)
		{
			return true;
		}
	}
	return false;
}

bool UGomokuRuleEngine::IsGameWon(int32& OutWinnerId) const
{
	OutWinnerId = 0;
	if (!bInitialized)
	{
		return false;
	}

	for (int32 Y = 0; Y < Board.SizeY; ++Y)
	{
		for (int32 X = 0; X < Board.SizeX; ++X)
		{
			if (!HasWinAt(X, Y))
			{
				continue;
			}

			OutWinnerId = CellStateToPlayerId(GetCellState(X, Y));
			return OutWinnerId != 0;
		}
	}
	return false;
}

bool UGomokuRuleEngine::IsGameOver() const
{
	int32 WinnerId = 0;
	if (IsGameWon(WinnerId))
	{
		return true;
	}
	return bInitialized && !Board.HasEmptyCell();
}

FGomokuPlayerStateData* UGomokuRuleEngine::FindPlayerMutable(int32 PlayerId)
{
	int32* IndexPtr = PlayerIdToIndex.Find(PlayerId);
	if (!IndexPtr) return nullptr;
	int32 Index = *IndexPtr;
	if (!Players.IsValidIndex(Index)) return nullptr;
	return &Players[Index];
}

const FGomokuPlayerStateData* UGomokuRuleEngine::FindPlayer(int32 PlayerId) const
{
	int32* IndexPtr = PlayerIdToIndex.Find(PlayerId);
	if (!IndexPtr) return nullptr;
	int32 Index = *IndexPtr;
	if (!Players.IsValidIndex(Index)) return nullptr;
	return &Players[Index];
}

bool UGomokuRuleEngine::IsBoardFull() const
{
	return bInitialized && !Board.HasEmptyCell();
}

bool UGomokuRuleEngine::IsValidEmpty(const FIntPoint& Cell) const
{
	if (!bInitialized || !Board.IsInside(Cell.X, Cell.Y))
		return false;
	return Board.Get(Cell.X, Cell.Y) == ECellState::Empty;
}

void UGomokuRuleEngine::SetCurrentPlayerIndex(int32 Index)
{
	CurrentPlayerIndex = FMath::Clamp(Index, 0, PlayerOrder.Num() - 1);
}

FGomokuWinResult UGomokuRuleEngine::CheckWinAt(const FIntPoint& Cell) const
{
	FGomokuWinResult Result;
	if (!bInitialized || !HasWinAt(Cell.X, Cell.Y))
		return Result;

	int32 WinnerId = CellStateToPlayerId(Board.Get(Cell.X, Cell.Y));
	for (int32 i = 0; i < PlayerOrder.Num(); ++i)
	{
		if (PlayerOrder[i] == WinnerId)
		{
			Result.IsWin = true;
			Result.WinnerPlayerIndex = i;
			Result.WinCell = Cell;
			break;
		}
	}
	return Result;
}

void UGomokuRuleEngine::MarkItemGainedThisTurn(int32 PlayerId, int32 ItemId)
{
	if (!bInitialized || ItemId <= 0) return;
	if (FGomokuPlayerStateData* P = FindPlayerMutable(PlayerId))
		P->ItemIdsGainedThisTurn.Add(ItemId);
}

void UGomokuRuleEngine::ClearTurnItemLocksForPlayer(int32 PlayerId)
{
	if (!bInitialized) return;
	if (FGomokuPlayerStateData* P = FindPlayerMutable(PlayerId))
		P->ItemIdsGainedThisTurn.Reset();
}

void UGomokuRuleEngine::ResetUsedItemThisTurn(int32 PlayerId)
{
	if (!bInitialized) return;
	if (FGomokuPlayerStateData* P = FindPlayerMutable(PlayerId))
		P->bUsedItemThisTurn = false;
}

void UGomokuRuleEngine::SetUsedItemThisTurn(int32 PlayerId)
{
	if (!bInitialized) return;
	if (FGomokuPlayerStateData* P = FindPlayerMutable(PlayerId))
		P->bUsedItemThisTurn = true;
}

bool UGomokuRuleEngine::IsItemUsableNow(int32 PlayerId, int32 ItemId) const
{
	if (!bInitialized || ItemId <= 0) return false;
	const FGomokuPlayerStateData* P = FindPlayer(PlayerId);
	if (!P) return false;

	if (!P->ItemIds.Contains(ItemId))
		return false;

	if (P->ItemIdsGainedThisTurn.Contains(ItemId))
		return false;

	if (P->bUsedItemThisTurn)
		return false;

	return true;
}

bool UGomokuRuleEngine::SetCellGuardianProtected(int32 X, int32 Y, bool bProtect)
{
	if (!IsValidCoordinate(X, Y)) return false;

	const ECellState S = GetCellState(X, Y);
	if (S == ECellState::Empty || S == ECellState::Blocked)
		return false;

	if (bProtect)
		GuardianProtectionExpireRounds.Add(FIntPoint(X, Y), MAX_int32);
	else
		GuardianProtectionExpireRounds.Remove(FIntPoint(X, Y));

	return true;
}

bool UGomokuRuleEngine::IsCellGuardianProtected(int32 X, int32 Y) const
{
	if (!IsValidCoordinate(X, Y)) return false;
	return GuardianProtectionExpireRounds.Contains(FIntPoint(X, Y));
}

TArray<FIntPoint> UGomokuRuleEngine::GetGuardianProtectedCells() const
{
	TArray<FIntPoint> Cells;
	GuardianProtectionExpireRounds.GenerateKeyArray(Cells);
	Cells.Sort([](const FIntPoint& Left, const FIntPoint& Right)
	{
		return Left.Y == Right.Y ? Left.X < Right.X : Left.Y < Right.Y;
	});
	return Cells;
}

bool UGomokuRuleEngine::ApplyTemporaryBlock(const FIntPoint& Cell, int32 ExpireAfterRound)
{
	if (ExpireAfterRound < 1 || !SetCellBlocked(Cell.X, Cell.Y, true))
	{
		return false;
	}
	TemporaryBlockedCellExpireRounds.Add(Cell, ExpireAfterRound);
	return true;
}

bool UGomokuRuleEngine::ApplyGuardianProtection(const FIntPoint& Cell, int32 ExpireAfterRound)
{
	if (ExpireAfterRound < 1 || !SetCellGuardianProtected(Cell.X, Cell.Y, true))
	{
		return false;
	}
	GuardianProtectionExpireRounds.Add(Cell, ExpireAfterRound);
	return true;
}

void UGomokuRuleEngine::ExpireRoundEffects(int32 CompletedRoundIndex)
{
	TArray<FIntPoint> BlocksToExpire;
	for (const TPair<FIntPoint, int32>& Pair : TemporaryBlockedCellExpireRounds)
	{
		if (Pair.Value <= CompletedRoundIndex)
		{
			BlocksToExpire.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Cell : BlocksToExpire)
	{
		SetCellBlocked(Cell.X, Cell.Y, false);
	}

	TArray<FIntPoint> GuardsToExpire;
	for (const TPair<FIntPoint, int32>& Pair : GuardianProtectionExpireRounds)
	{
		if (Pair.Value <= CompletedRoundIndex)
		{
			GuardsToExpire.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Cell : GuardsToExpire)
	{
		GuardianProtectionExpireRounds.Remove(Cell);
	}
}

int32 UGomokuRuleEngine::ResolveActivePlayerId(int32 ActivePlayerListIndex) const
{
	if (!ActivePlayerIndices.IsValidIndex(ActivePlayerListIndex))
	{
		return INDEX_NONE;
	}
	const int32 PlayerIndex = ActivePlayerIndices[ActivePlayerListIndex];
	return PlayerOrder.IsValidIndex(PlayerIndex) ? PlayerOrder[PlayerIndex] : INDEX_NONE;
}
