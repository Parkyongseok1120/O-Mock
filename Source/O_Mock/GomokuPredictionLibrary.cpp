// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuPredictionLibrary.h"
#include "GomokuRuleEngine.h"

bool UGomokuPredictionLibrary::IsInside(int32 X, int32 Y, int32 SizeX, int32 SizeY)
{
	return X >= 0 && Y >= 0 && X < SizeX && Y < SizeY;
}

ECellState UGomokuPredictionLibrary::GetCell(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 X, int32 Y)
{
	if (!IsInside(X, Y, SizeX, SizeY))
	{
		return ECellState::Empty;
	}
	const int32 Index = Y * SizeX + X;
	return Cells.IsValidIndex(Index) ? Cells[Index] : ECellState::Empty;
}

TArray<FIntPoint> UGomokuPredictionLibrary::FindImmediateWinCells(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 PlayerId, int32 WinLength)
{
	TArray<FIntPoint> Result;
	const ECellState PlayerState = UGomokuRuleEngine::PlayerIdToCellState(PlayerId);
	if (PlayerState == ECellState::Empty || Cells.Num() != SizeX * SizeY || WinLength <= 0)
	{
		return Result;
	}
	static const FIntPoint Directions[] = {
		FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1), FIntPoint(1, -1)
	};
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			if (GetCell(Cells, SizeX, SizeY, X, Y) != ECellState::Empty)
			{
				continue;
			}
			for (const FIntPoint& Direction : Directions)
			{
				int32 Count = 1;
				for (int32 Sign : { -1, 1 })
				{
					int32 Step = 1;
					while (GetCell(Cells, SizeX, SizeY,
						X + Direction.X * Step * Sign, Y + Direction.Y * Step * Sign) == PlayerState)
					{
						++Count;
						++Step;
					}
				}
				if (Count >= WinLength)
				{
					Result.Add(FIntPoint(X, Y));
					break;
				}
			}
		}
	}
	return Result;
}

bool UGomokuPredictionLibrary::IsIsolated(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, const FIntPoint& Cell)
{
	const ECellState Owner = GetCell(Cells, SizeX, SizeY, Cell.X, Cell.Y);
	if (Owner < ECellState::Player1 || Owner > ECellState::Player4)
	{
		return false;
	}
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			if ((DX != 0 || DY != 0) && GetCell(Cells, SizeX, SizeY, Cell.X + DX, Cell.Y + DY) == Owner)
			{
				return false;
			}
		}
	}
	return true;
}

TArray<FIntPoint> UGomokuPredictionLibrary::FindStealTargets(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells)
{
	TArray<FIntPoint> Result;
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const FIntPoint Cell(X, Y);
			const int32 OwnerId = UGomokuRuleEngine::CellStateToPlayerId(GetCell(Cells, SizeX, SizeY, X, Y));
			if (OwnerId > 0 && OwnerId != PlayerId && !GuardianCells.Contains(Cell)
				&& IsIsolated(Cells, SizeX, SizeY, Cell))
			{
				Result.Add(Cell);
			}
		}
	}
	return Result;
}

bool UGomokuPredictionLibrary::FindPullSource(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 PlayerId, const FIntPoint& Destination,
	const TArray<FIntPoint>& GuardianCells, FIntPoint& OutSource)
{
	if (GetCell(Cells, SizeX, SizeY, Destination.X, Destination.Y) != ECellState::Empty
		|| !IsInside(Destination.X, Destination.Y, SizeX, SizeY))
	{
		return false;
	}
	static const FIntPoint Directions[] = {
		FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, 0)
	};
	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint Candidate = Destination + Direction;
		if (IsInside(Candidate.X, Candidate.Y, SizeX, SizeY) && !GuardianCells.Contains(Candidate)
			&& UGomokuRuleEngine::CellStateToPlayerId(
				GetCell(Cells, SizeX, SizeY, Candidate.X, Candidate.Y)) == PlayerId)
		{
			OutSource = Candidate;
			return true;
		}
	}
	return false;
}

TArray<FIntPoint> UGomokuPredictionLibrary::FindPullDestinations(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells)
{
	TArray<FIntPoint> Result;
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			FIntPoint Source;
			if (FindPullSource(Cells, SizeX, SizeY, PlayerId, FIntPoint(X, Y), GuardianCells, Source))
			{
				Result.Add(FIntPoint(X, Y));
			}
		}
	}
	return Result;
}

TArray<FIntPoint> UGomokuPredictionLibrary::FindGuardianTargets(const TArray<ECellState>& Cells,
	int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells)
{
	TArray<FIntPoint> Result;
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (!GuardianCells.Contains(Cell) && UGomokuRuleEngine::CellStateToPlayerId(
				GetCell(Cells, SizeX, SizeY, X, Y)) == PlayerId)
			{
				Result.Add(Cell);
			}
		}
	}
	return Result;
}
