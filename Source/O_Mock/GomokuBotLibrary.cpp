// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBotLibrary.h"

bool UGomokuBotLibrary::FindImmediateWin(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell)
{
	if (!Engine || !Engine->IsMatchInitialized())
	{
		return false;
	}

	const FGomokuMatchConfig Config = Engine->GetMatchConfig();
	for (int32 Y = 0; Y < Config.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Config.BoardSizeX; ++X)
		{
			if (!Engine->IsValidEmpty(FIntPoint(X, Y)))
			{
				continue;
			}

			if (!Engine->ForcePlaceStone(PlayerId, X, Y))
			{
				continue;
			}

			const bool bWins = Engine->CheckWinAt(FIntPoint(X, Y)).IsWin;
			Engine->RemoveStoneAt(X, Y);
			if (bWins)
			{
				OutCell = FIntPoint(X, Y);
				return true;
			}
		}
	}

	return false;
}

bool UGomokuBotLibrary::HasAdjacentStone(UGomokuRuleEngine* Engine, const FIntPoint& Cell, int32 PlayerId, bool bOwnOnly)
{
	if (!Engine)
	{
		return false;
	}

	static const int32 DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	static const int32 DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
	for (int32 i = 0; i < UE_ARRAY_COUNT(DX); ++i)
	{
		const int32 X = Cell.X + DX[i];
		const int32 Y = Cell.Y + DY[i];
		if (!Engine->IsValidCoordinate(X, Y))
		{
			continue;
		}

		const int32 OwnerId = UGomokuRuleEngine::CellStateToPlayerId(Engine->GetCellState(X, Y));
		if (OwnerId > 0 && (!bOwnOnly || OwnerId == PlayerId))
		{
			return true;
		}
	}
	return false;
}

bool UGomokuBotLibrary::ChooseMove(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell)
{
	if (!Engine || !Engine->IsMatchInitialized() || PlayerId <= 0 || Engine->IsGameOver())
	{
		return false;
	}

	if (FindImmediateWin(Engine, PlayerId, OutCell))
	{
		return true;
	}

	const TArray<int32> ActiveIndices = Engine->GetActivePlayerIndices();
	for (const int32 OtherIndex : ActiveIndices)
	{
		const int32 OtherPlayerId = OtherIndex + 1;
		if (OtherPlayerId == PlayerId)
		{
			continue;
		}
		if (FindImmediateWin(Engine, OtherPlayerId, OutCell))
		{
			return true;
		}
	}

	const FGomokuMatchConfig Config = Engine->GetMatchConfig();
	const FIntPoint Center(Config.BoardSizeX / 2, Config.BoardSizeY / 2);
	bool bFound = false;
	int32 BestScore = MIN_int32;
	FIntPoint BestCell(-1, -1);
	for (int32 Y = 0; Y < Config.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Config.BoardSizeX; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (!Engine->IsValidEmpty(Cell))
			{
				continue;
			}

			int32 Score = 0;
			if (HasAdjacentStone(Engine, Cell, PlayerId, true))
			{
				Score += 1000;
			}
			if (HasAdjacentStone(Engine, Cell, PlayerId, false))
			{
				Score += 100;
			}
			Score -= FMath::Abs(Cell.X - Center.X) + FMath::Abs(Cell.Y - Center.Y);

			if (!bFound || Score > BestScore)
			{
				bFound = true;
				BestScore = Score;
				BestCell = Cell;
			}
		}
	}

	if (bFound)
	{
		OutCell = BestCell;
	}
	return bFound;
}

bool UGomokuBotLibrary::TakeTurn(UGomokuRuleEngine* Engine, int32 PlayerId, FIntPoint& OutCell)
{
	if (!ChooseMove(Engine, PlayerId, OutCell))
	{
		return false;
	}
	return Engine->TryPlaceStone(PlayerId, OutCell.X, OutCell.Y);
}
