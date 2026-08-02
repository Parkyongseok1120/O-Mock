// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuRuleEngine.h"

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
	MatchConfig = Config;
	MatchConfig.BoardSizeX = FMath::Max(1, MatchConfig.BoardSizeX);
	MatchConfig.BoardSizeY = FMath::Max(1, MatchConfig.BoardSizeY);
	MatchConfig.WinLength = FMath::Max(1, MatchConfig.WinLength);
	MatchConfig.MaxPlayers = FMath::Clamp(MatchConfig.MaxPlayers, 2, 4);

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
	if (!bInitialized || !IsValidCoordinate(X, Y))
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
	return true;
}

bool UGomokuRuleEngine::ChangeCellOwnership(int32 X, int32 Y, int32 NewPlayerId)
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

	const ECellState NewState = PlayerIdToCellState(NewPlayerId);
	if (NewState == ECellState::Empty)
	{
		return false;
	}

	Board.Set(X, Y, NewState);
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
	if (!bInitialized || PlayerOrder.Num() == 0)
	{
		return;
	}

	const int32 ActiveCount = ActivePlayerIndices.Num();
	if (ActiveCount == 0)
	{
		return;
	}

	for (int32 Step = 0; Step < ActiveCount; ++Step)
	{
		CurrentPlayerIndex = AdvanceTurnIndex(CurrentPlayerIndex, TurnDirection);

		const int32 NextPlayerId = PlayerOrder[CurrentPlayerIndex];
		if (FGomokuPlayerStateData* Player = FindPlayerMutable(NextPlayerId))
		{
			if (Player->bSkipNextTurn)
			{
				Player->bSkipNextTurn = false;
				continue;
			}
		}
		return;
	}
}

int32 UGomokuRuleEngine::AdvanceTurnIndex(int32 CurrentIndex, int32 Direction) const
{
	const int32 N = PlayerOrder.Num();
	if (N <= 0) return 0;

	int64 Next = static_cast<int64>(CurrentIndex) + static_cast<int64>(Direction);
	Next = FMath::Max(Next, 0LL);
	return static_cast<int32>(Next % N);
}

void UGomokuRuleEngine::ReverseTurnDirection()
{
	if (!bInitialized || PlayerOrder.Num() == 0)
	{
		return;
	}
	TurnDirection = (TurnDirection != 0) ? -TurnDirection : 1;
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
		Player->bSkipNextTurn = bSkip;
		return true;
	}
	return false;
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
	for (FGomokuPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}
	return nullptr;
}

const FGomokuPlayerStateData* UGomokuRuleEngine::FindPlayer(int32 PlayerId) const
{
	for (const FGomokuPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}
	return nullptr;
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
