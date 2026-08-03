// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuItemLibrary.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** Minimal item data table: maps ItemId -> (Type, Cost, TargetType). */
	struct FStaticItemDef
	{
		int32 Id;
		EGomokuItemType Type;
		int32 EnergyCost;
		EItemTargetType TargetType;
	};

	const TArray<FStaticItemDef>& GetStaticItemDefs()
	{
		static const auto Init = []() {
			TArray<FStaticItemDef> Defs;
			// 1=SealStone, cost1, Cell
			Defs.Add(FStaticItemDef{1, EGomokuItemType::SealStone, 1, EItemTargetType::Cell});
			// 2=Pull (stubbed in ExecuteItem)
			Defs.Add(FStaticItemDef{2, EGomokuItemType::Pull, 1, EItemTargetType::Cell});
			// 3=Steal
			Defs.Add(FStaticItemDef{3, EGomokuItemType::Steal, 1, EItemTargetType::Cell});
			// 4=SkipTurn (target is player)
			Defs.Add(FStaticItemDef{4, EGomokuItemType::SkipTurn, 1, EItemTargetType::Player});
			// 5=GuardianBarrier
			Defs.Add(FStaticItemDef{5, EGomokuItemType::GuardianBarrier, 1, EItemTargetType::Cell});
			return Defs;
		}();
		return Init;
	}

	const FStaticItemDef* GetItemData(int32 ItemId)
	{
		const auto& Defs = GetStaticItemDefs();
		for (const auto& Def : Defs)
		{
			if (Def.Id == ItemId)
				return &Def;
		}
		return nullptr;
	}
}

bool UGomokuItemLibrary::IsRegisteredItemId(int32 ItemId)
{
	return GetItemData(ItemId) != nullptr;
}

bool UGomokuItemLibrary::CanUseItem(UGomokuRuleEngine* Engine, int32 PlayerId, int32 ItemId)
{
	if (!Engine || !Engine->IsMatchInitialized())
		return false;

	if (ItemId <= 0 || PlayerId <= 0)
		return false;

	// Contract: only registered items may be used.
	if (!IsRegisteredItemId(ItemId))
		return false;

	// Stage 7: use IsItemUsableNow instead of raw PlayerHasItem.
	return Engine->IsItemUsableNow(PlayerId, ItemId);
}

bool UGomokuItemLibrary::ValidateTarget(UGomokuRuleEngine* Engine, int32 ItemId, const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!Engine || !Engine->IsMatchInitialized())
		return false;
	if (ItemId <= 0)
		return false;

	const auto* Def = GetItemData(ItemId);
	if (!Def)
		return false;

	switch (Def->TargetType)
	{
	case EItemTargetType::Cell:
	{
		if (TargetCell.X < 0 || TargetCell.Y < 0)
			return false;
		if (!Engine->IsValidCoordinate(TargetCell.X, TargetCell.Y))
			return false;

		switch (Def->Type)
		{
		case EGomokuItemType::SealStone:
		{
			const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
			if (C.State != ECellState::Empty) return false;
			break;
		}
		case EGomokuItemType::Steal:
		{
			if (Engine->IsCellGuardianProtected(TargetCell.X, TargetCell.Y)) return false;
			const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
			if (C.State == ECellState::Empty || C.State == ECellState::Blocked) return false;
			const int32 OwnerId = UGomokuRuleEngine::CellStateToPlayerId(C.State);
			if (OwnerId <= 0) return false;
			break;
		}
		case EGomokuItemType::Pull:
		{
			const auto Dest = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
			if (Dest.State != ECellState::Empty) return false;
			static const int32 DXs[4] = {0,1,0,-1};
			static const int32 DYs[4] = {1,0,-1,0};
			bool bFound = false;
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 SX = TargetCell.X + DXs[i];
				const int32 SY = TargetCell.Y + DYs[i];
				if (!Engine->IsValidCoordinate(SX, SY)) continue;
				if (Engine->IsCellGuardianProtected(SX, SY)) continue;
				const auto S = Engine->GetBoardCell(SX, SY);
				if (S.State == ECellState::Empty || S.State == ECellState::Blocked) continue;
				const int32 OwnerId = UGomokuRuleEngine::CellStateToPlayerId(S.State);
				if (OwnerId > 0) { bFound = true; break; }
			}
			if (!bFound) return false;
			break;
		}
		case EGomokuItemType::GuardianBarrier:
		{
			const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
			if (C.State == ECellState::Empty) return false;
			break;
		}
		}
		return true;
	}
	case EItemTargetType::Player:
	{
		if (TargetPlayerIndex < 0) return false;
		const auto& A = Engine->GetActivePlayerIndices();
		if (TargetPlayerIndex >= A.Num()) return false;
		return true;
	}
	case EItemTargetType::Self:
		return true;
	default:
		return false;
	}
}

bool UGomokuItemLibrary::ExecuteItem(UGomokuRuleEngine* Engine, int32 ItemId, int32 PlayerId, const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!Engine || !Engine->IsMatchInitialized()) return false;
	if (ItemId <= 0 || PlayerId <= 0) return false;

	const auto* Def = GetItemData(ItemId);
	if (!Def) return false;
	if (!Engine->IsItemUsableNow(PlayerId, ItemId)) return false;
	if (!ValidateTarget(Engine, ItemId, TargetCell, TargetPlayerIndex)) return false;

	// Pre-check energy without consuming yet.
	if (Engine->GetPlayerStateData(PlayerId).Energy < Def->EnergyCost) return false;

	switch (Def->Type)
	{
	case EGomokuItemType::SealStone:
	{
		const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
		if (C.State != ECellState::Empty) return false;
		if (!Engine->SetCellBlocked(TargetCell.X, TargetCell.Y, true)) return false;
		break;
	}
	case EGomokuItemType::SkipTurn:
	{
		const int32 TgtId = (TargetPlayerIndex + 1);
		if (!Engine->SetPlayerSkipNextTurn(TgtId, true)) return false;
		break;
	}
	case EGomokuItemType::Steal:
	{
		if (Engine->IsCellGuardianProtected(TargetCell.X, TargetCell.Y)) return false;
		const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
		if (C.State == ECellState::Empty || C.State == ECellState::Blocked) return false;
		if (!Engine->ChangeCellOwnership(TargetCell.X, TargetCell.Y, PlayerId)) return false;
		Engine->SetLastItemWinResult(Engine->CheckWinAt(TargetCell));
		break;
	}
	case EGomokuItemType::GuardianBarrier:
	{
		const auto C = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
		if (C.State == ECellState::Empty) return false;
		if (!Engine->SetCellGuardianProtected(TargetCell.X, TargetCell.Y, true)) return false;
		break;
	}
	case EGomokuItemType::Pull:
	{
		int32 SrcX = -1, SrcY = -1, OwnerId = 0;
		static const int32 DXs[4] = {0,1,0,-1};
		static const int32 DYs[4] = {1,0,-1,0};
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 SX = TargetCell.X + DXs[i];
			const int32 SY = TargetCell.Y + DYs[i];
			if (!Engine->IsValidCoordinate(SX, SY)) continue;
			if (Engine->IsCellGuardianProtected(SX, SY)) continue;
			const auto S = Engine->GetBoardCell(SX, SY);
			if (S.State == ECellState::Empty || S.State == ECellState::Blocked) continue;
			OwnerId = UGomokuRuleEngine::CellStateToPlayerId(S.State);
			if (OwnerId <= 0) continue;
			SrcX = SX; SrcY = SY; break;
		}
		if (SrcX < 0) return false;

		const auto Dest = Engine->GetBoardCell(TargetCell.X, TargetCell.Y);
		if (Dest.State != ECellState::Empty) return false;

		if (!Engine->RemoveStoneAt(SrcX, SrcY)) return false;
		if (!Engine->ForcePlaceStone(OwnerId, TargetCell.X, TargetCell.Y)) return false;
		Engine->SetLastItemWinResult(Engine->CheckWinAt(TargetCell));
		break;
	}
	default:
		return false;
	}

	// Consume energy after successful effect.
	if (!Engine->ConsumePlayerEnergy(PlayerId, Def->EnergyCost)) return false;
	if (!Engine->RemoveItemFromInventory(PlayerId, ItemId)) return false;
	Engine->SetUsedItemThisTurn(PlayerId);
	return true;
}
