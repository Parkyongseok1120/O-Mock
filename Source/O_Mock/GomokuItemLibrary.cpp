// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuItemLibrary.h"

struct FGomokuStaticItemDef
{
	int32 Id;
	EGomokuItemType Type;
	int32 EnergyCost;
	EItemTargetType TargetType;
};

static const FGomokuStaticItemDef GomokuItemDefinitions[] =
{
	{1, EGomokuItemType::SealStone, 1, EItemTargetType::Cell},
	{2, EGomokuItemType::Pull, 1, EItemTargetType::Cell},
	{3, EGomokuItemType::Steal, 3, EItemTargetType::Cell},
	{4, EGomokuItemType::SkipTurn, 3, EItemTargetType::Player},
	{5, EGomokuItemType::GuardianBarrier, 2, EItemTargetType::Cell}
};

static const FIntPoint GomokuCardinalDirections[] =
{
	FIntPoint(0, 1),
	FIntPoint(1, 0),
	FIntPoint(0, -1),
	FIntPoint(-1, 0)
};

static const FGomokuStaticItemDef* FindGomokuItemDefinition(int32 ItemId)
{
	for (const FGomokuStaticItemDef& Definition : GomokuItemDefinitions)
	{
		if (Definition.Id == ItemId)
		{
			return &Definition;
		}
	}
	return nullptr;
}

static bool FindPullSource(UGomokuRuleEngine* Engine, int32 PlayerId, const FIntPoint& Destination,
	FIntPoint& OutSource)
{
	if (!Engine || !Engine->IsValidEmpty(Destination))
	{
		return false;
	}

	for (const FIntPoint& Direction : GomokuCardinalDirections)
	{
		const FIntPoint Candidate = Destination + Direction;
		if (!Engine->IsValidCoordinate(Candidate.X, Candidate.Y)
			|| Engine->IsCellGuardianProtected(Candidate.X, Candidate.Y))
		{
			continue;
		}

		const FBoardCell SourceCell = Engine->GetBoardCell(Candidate.X, Candidate.Y);
		if (UGomokuRuleEngine::CellStateToPlayerId(SourceCell.State) == PlayerId)
		{
			OutSource = Candidate;
			return true;
		}
	}

	return false;
}

bool UGomokuItemLibrary::IsRegisteredItemId(int32 ItemId)
{
	return FindGomokuItemDefinition(ItemId) != nullptr;
}

bool UGomokuItemLibrary::GetItemData(int32 ItemId, FItemData& OutItemData)
{
	const FGomokuStaticItemDef* Definition = FindGomokuItemDefinition(ItemId);
	if (!Definition)
	{
		OutItemData = FItemData();
		return false;
	}

	FText DisplayName;
	FText Description;
	FText TargetInstruction;
	switch (Definition->Type)
	{
	case EGomokuItemType::SealStone:
		DisplayName = FText::FromString(TEXT("Seal Stone"));
		Description = FText::FromString(TEXT("Block an empty cell for one round."));
		TargetInstruction = FText::FromString(TEXT("Choose an empty cell."));
		break;
	case EGomokuItemType::Pull:
		DisplayName = FText::FromString(TEXT("Pull"));
		Description = FText::FromString(TEXT("Move an adjacent allied stone into an empty cell."));
		TargetInstruction = FText::FromString(TEXT("Choose an empty cell beside one of your stones."));
		break;
	case EGomokuItemType::Steal:
		DisplayName = FText::FromString(TEXT("Steal"));
		Description = FText::FromString(TEXT("Convert an isolated enemy stone to your color."));
		TargetInstruction = FText::FromString(TEXT("Choose an isolated, unguarded enemy stone."));
		break;
	case EGomokuItemType::SkipTurn:
		DisplayName = FText::FromString(TEXT("Skip Turn"));
		Description = FText::FromString(TEXT("Skip the next active player's turn."));
		TargetInstruction = FText::FromString(TEXT("Click the card to use it immediately."));
		break;
	case EGomokuItemType::GuardianBarrier:
		DisplayName = FText::FromString(TEXT("Guardian Barrier"));
		Description = FText::FromString(TEXT("Protect one allied stone for two rounds."));
		TargetInstruction = FText::FromString(TEXT("Choose one of your unguarded stones."));
		break;
	default:
		DisplayName = FText::FromString(TEXT("Unknown"));
		Description = FText::GetEmpty();
		TargetInstruction = FText::GetEmpty();
		break;
	}
	OutItemData = FItemData(Definition->Id, DisplayName, Definition->EnergyCost, Definition->TargetType,
		Description, TargetInstruction);
	return true;
}

bool UGomokuItemLibrary::CanUseItem(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId)
{
	return Engine
		&& Engine->IsMatchInitialized()
		&& PlayerId > 0
		&& Engine->IsPlayerActive(PlayerId)
		&& IsRegisteredItemId(ItemId)
		&& Engine->IsItemUsableNow(PlayerId, ItemId);
}

bool UGomokuItemLibrary::ValidateTarget(UGomokuRuleEngine* Engine, int32 ItemId,
	const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	const int32 CurrentPlayerId = Engine ? Engine->GetCurrentPlayerId() : INDEX_NONE;
	return ValidateTargetForPlayer(Engine, CurrentPlayerId, ItemId, TargetCell, TargetPlayerIndex);
}

bool UGomokuItemLibrary::ValidateTargetForPlayer(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId,
	const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!Engine || !Engine->IsMatchInitialized() || !Engine->IsPlayerActive(PlayerId))
	{
		return false;
	}

	const FGomokuStaticItemDef* Definition = FindGomokuItemDefinition(ItemId);
	if (!Definition)
	{
		return false;
	}

	if (Definition->TargetType == EItemTargetType::Player)
	{
		const int32 TargetPlayerId = Engine->ResolveActivePlayerId(TargetPlayerIndex);
		const TArray<int32>& ActiveIndices = Engine->GetActivePlayerIndices();
		const int32 CurrentPlayerIndex = PlayerId - 1;
		const int32 CurrentActivePosition = ActiveIndices.IndexOfByKey(CurrentPlayerIndex);
		if (TargetPlayerId <= 0 || CurrentActivePosition == INDEX_NONE || ActiveIndices.Num() <= 1)
		{
			return false;
		}
		const int32 Direction = Engine->TurnDirection >= 0 ? 1 : -1;
		const int32 ExpectedTargetPosition = (CurrentActivePosition + Direction + ActiveIndices.Num()) % ActiveIndices.Num();
		return TargetPlayerIndex == ExpectedTargetPosition && TargetPlayerId != PlayerId;
	}

	if (Definition->TargetType == EItemTargetType::Self)
	{
		return true;
	}

	if (Definition->TargetType != EItemTargetType::Cell
		|| !Engine->IsValidCoordinate(TargetCell.X, TargetCell.Y))
	{
		return false;
	}

	const FBoardCell Target = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
	switch (Definition->Type)
	{
	case EGomokuItemType::SealStone:
		return Target.State == ECellState::Empty;

	case EGomokuItemType::Pull:
	{
		FIntPoint Source;
		return FindPullSource(Engine, PlayerId, TargetCell, Source);
	}

	case EGomokuItemType::Steal:
	{
		const int32 OwnerId = UGomokuRuleEngine::CellStateToPlayerId(Target.State);
		return OwnerId > 0
			&& OwnerId != PlayerId
			&& !Engine->IsCellGuardianProtected(TargetCell.X, TargetCell.Y)
			&& Engine->IsStoneIsolated(TargetCell);
	}

	case EGomokuItemType::GuardianBarrier:
		return UGomokuRuleEngine::CellStateToPlayerId(Target.State) == PlayerId
			&& !Engine->IsCellGuardianProtected(TargetCell.X, TargetCell.Y);

	default:
		return false;
	}
}

bool UGomokuItemLibrary::ExecuteItem(UGomokuRuleEngine* Engine, int32 ItemId, int32 PlayerId,
	const FIntPoint& TargetCell, int32 TargetPlayerIndex, int32 CurrentRoundIndex)
{
	const FGomokuStaticItemDef* Definition = FindGomokuItemDefinition(ItemId);
	if (!Definition
		|| !CanUseItem(Engine, PlayerId, ItemId)
		|| !ValidateTargetForPlayer(Engine, PlayerId, ItemId, TargetCell, TargetPlayerIndex)
		|| Engine->GetPlayerStateData(PlayerId).Energy < Definition->EnergyCost)
	{
		return false;
	}
	if (!Engine->ConsumePlayerEnergy(PlayerId, Definition->EnergyCost))
	{
		return false;
	}
	if (!Engine->RemoveItemFromInventory(PlayerId, ItemId))
	{
		Engine->AddPlayerEnergy(PlayerId, Definition->EnergyCost, MaxEnergy);
		return false;
	}
	Engine->SetUsedItemThisTurn(PlayerId);

	bool bEffectApplied = false;
	switch (Definition->Type)
	{
	case EGomokuItemType::SealStone:
		bEffectApplied = Engine->ApplyTemporaryBlock(TargetCell, CurrentRoundIndex + 1);
		break;

	case EGomokuItemType::Pull:
	{
		FIntPoint Source;
		bEffectApplied = FindPullSource(Engine, PlayerId, TargetCell, Source)
			&& Engine->MoveStone(PlayerId, Source, TargetCell);
		if (bEffectApplied)
		{
			Engine->SetLastItemWinResult(Engine->CheckWinAt(TargetCell));
		}
		break;
	}

	case EGomokuItemType::Steal:
		bEffectApplied = Engine->ChangeCellOwnership(TargetCell.X, TargetCell.Y, PlayerId);
		if (bEffectApplied)
		{
			Engine->SetLastItemWinResult(Engine->CheckWinAt(TargetCell));
		}
		break;

	case EGomokuItemType::SkipTurn:
	{
		const int32 TargetPlayerId = Engine->ResolveActivePlayerId(TargetPlayerIndex);
		bEffectApplied = Engine->SetPlayerSkipNextTurn(TargetPlayerId, true);
		break;
	}

	case EGomokuItemType::GuardianBarrier:
		bEffectApplied = Engine->ApplyGuardianProtection(TargetCell, CurrentRoundIndex + 2);
		break;

	default:
		break;
	}

	if (!bEffectApplied)
	{
		const bool bEnergyRestored = Engine->AddPlayerEnergy(PlayerId, Definition->EnergyCost, MaxEnergy);
		const bool bItemRestored = Engine->AddItemToInventory(PlayerId, ItemId);
		Engine->ResetUsedItemThisTurn(PlayerId);
		ensureAlwaysMsgf(bEnergyRestored && bItemRestored,
			TEXT("Failed to roll back Gomoku item %d for player %d."), ItemId, PlayerId);
		return false;
	}
	return true;
}
