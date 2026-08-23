// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBotLibrary.h"
#include "GomokuItemLibrary.h"

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

bool UGomokuBotLibrary::FindItemTarget(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId,
	FIntPoint& OutCell, int32& OutTargetPlayerIndex)
{
	if (!Engine)
	{
		return false;
	}
	OutCell = FIntPoint(-1, -1);
	OutTargetPlayerIndex = INDEX_NONE;
	FItemData ItemData;
	if (!UGomokuItemLibrary::GetItemData(ItemId, ItemData))
	{
		return false;
	}
	if (ItemData.TargetType == EItemTargetType::Player)
	{
		const TArray<int32>& ActiveIndices = Engine->GetActivePlayerIndices();
		const int32 CurrentPosition = ActiveIndices.IndexOfByKey(PlayerId - 1);
		if (CurrentPosition == INDEX_NONE || ActiveIndices.Num() <= 1)
		{
			return false;
		}
		const int32 Direction = Engine->TurnDirection >= 0 ? 1 : -1;
		OutTargetPlayerIndex = (CurrentPosition + Direction + ActiveIndices.Num()) % ActiveIndices.Num();
		return UGomokuItemLibrary::ValidateTargetForPlayer(
			Engine, PlayerId, ItemId, OutCell, OutTargetPlayerIndex);
	}

	const FGomokuMatchConfig Config = Engine->GetMatchConfig();
	const FIntPoint Center(Config.BoardSizeX / 2, Config.BoardSizeY / 2);
	TArray<FIntPoint> Candidates;
	Candidates.Reserve(Config.BoardSizeX * Config.BoardSizeY);
	for (int32 Y = 0; Y < Config.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Config.BoardSizeX; ++X)
		{
			Candidates.Add(FIntPoint(X, Y));
		}
	}
	Candidates.Sort([Center](const FIntPoint& A, const FIntPoint& B)
	{
		const int32 DistanceA = FMath::Abs(A.X - Center.X) + FMath::Abs(A.Y - Center.Y);
		const int32 DistanceB = FMath::Abs(B.X - Center.X) + FMath::Abs(B.Y - Center.Y);
		return DistanceA == DistanceB ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : DistanceA < DistanceB;
	});
	for (const FIntPoint& Candidate : Candidates)
	{
		if (UGomokuItemLibrary::ValidateTargetForPlayer(
			Engine, PlayerId, ItemId, Candidate, OutTargetPlayerIndex))
		{
			OutCell = Candidate;
			return true;
		}
	}
	return false;
}

bool UGomokuBotLibrary::ChooseItemAction(UGomokuRuleEngine* Engine, int32 PlayerId,
	float ItemUseProbability, FRandomStream& RandomStream, FGomokuBotItemAction& OutAction)
{
	OutAction = FGomokuBotItemAction();
	if (!Engine || !Engine->IsMatchInitialized()
		|| Engine->GetCurrentPlayerId() != PlayerId
		|| RandomStream.FRand() >= FMath::Clamp(ItemUseProbability, 0.f, 1.f))
	{
		return false;
	}
	const FGomokuPlayerStateData PlayerData = Engine->GetPlayerStateData(PlayerId);
	if (PlayerData.bHasAbandoned || PlayerData.PendingInventoryItemId > 0)
	{
		return false;
	}
	// Conversion and movement have the highest tactical impact; defensive utility follows.
	static const int32 ItemPriority[] = { 3, 2, 4, 5, 1 };
	for (const int32 ItemId : ItemPriority)
	{
		if (!PlayerData.ItemIds.Contains(ItemId) || !UGomokuItemLibrary::CanUseItem(Engine, PlayerId, ItemId))
		{
			continue;
		}
		FIntPoint TargetCell;
		int32 TargetPlayerIndex = INDEX_NONE;
		if (!FindItemTarget(Engine, PlayerId, ItemId, TargetCell, TargetPlayerIndex))
		{
			continue;
		}
		OutAction.bUsedItem = true;
		OutAction.ItemId = ItemId;
		OutAction.TargetCell = TargetCell;
		OutAction.TargetPlayerIndex = TargetPlayerIndex;
		return true;
	}
	return false;
}

bool UGomokuBotLibrary::TryUseItem(UGomokuRuleEngine* Engine, int32 PlayerId, float ItemUseProbability,
	FRandomStream& RandomStream, int32 CurrentRoundIndex, FGomokuBotItemAction& OutAction)
{
	if (!ChooseItemAction(Engine, PlayerId, ItemUseProbability, RandomStream, OutAction))
	{
		return false;
	}
	if (UGomokuItemLibrary::ExecuteItem(Engine, OutAction.ItemId, PlayerId,
		OutAction.TargetCell, OutAction.TargetPlayerIndex, CurrentRoundIndex))
	{
		return true;
	}
	OutAction = FGomokuBotItemAction();
	return false;
}

bool UGomokuBotLibrary::TakeTurnWithItems(UGomokuRuleEngine* Engine, int32 PlayerId,
	float ItemUseProbability, FRandomStream& RandomStream, int32 CurrentRoundIndex,
	FGomokuBotItemAction& OutItemAction, FIntPoint& OutCell)
{
	OutItemAction = FGomokuBotItemAction();
	if (!Engine || Engine->GetCurrentPlayerId() != PlayerId
		|| Engine->GetPlayerStateData(PlayerId).PendingInventoryItemId > 0)
	{
		OutCell = FIntPoint(-1, -1);
		return false;
	}
	TryUseItem(Engine, PlayerId, ItemUseProbability, RandomStream, CurrentRoundIndex, OutItemAction);
	if (Engine && Engine->IsGameOver())
	{
		OutCell = FIntPoint(-1, -1);
		return true;
	}
	return TakeTurn(Engine, PlayerId, OutCell);
}
