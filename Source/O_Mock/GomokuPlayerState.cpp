// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuPlayerState.h"
#include "Net/UnrealNetwork.h"

AGomokuPlayerState::AGomokuPlayerState()
{
	bReplicates = true;

	GomokuPlayerId = 0;
	StoneColor = FLinearColor::White;
	RemainingTime = 120.0f;
	Energy = 0;
	PublicStatusEffects.Empty();
	bAbandoned = false;
	bReady = false;
	bGomokuBot = false;
	InventoryItemIds.Empty();
	LockedInventoryItemIds.Empty();
	bUsedItemThisTurn = false;
	PendingInventoryItemId = 0;
}

void AGomokuPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGomokuPlayerState, GomokuPlayerId);
	DOREPLIFETIME(AGomokuPlayerState, StoneColor);
	DOREPLIFETIME(AGomokuPlayerState, RemainingTime);
	DOREPLIFETIME(AGomokuPlayerState, Energy);
	DOREPLIFETIME(AGomokuPlayerState, PublicStatusEffects);
	DOREPLIFETIME(AGomokuPlayerState, bAbandoned);
	DOREPLIFETIME(AGomokuPlayerState, bReady);
	DOREPLIFETIME(AGomokuPlayerState, bGomokuBot);
	DOREPLIFETIME_CONDITION(AGomokuPlayerState, InventoryItemIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGomokuPlayerState, LockedInventoryItemIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGomokuPlayerState, bUsedItemThisTurn, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGomokuPlayerState, PendingInventoryItemId, COND_OwnerOnly);
}

void AGomokuPlayerState::SetIdentity(int32 InId, const FLinearColor& InColor)
{
	if (!HasAuthority()) return;

	GomokuPlayerId = FMath::Clamp(InId, 0, 4);
	StoneColor = InColor;
}

void AGomokuPlayerState::SetPublicMatchState(float InRemainingTime, int32 InEnergy,
	const TArray<FName>& InEffects, bool bInAbandoned, const TArray<int32>& InInventoryItemIds,
	const TArray<int32>& InLockedInventoryItemIds, bool bInUsedItemThisTurn, int32 InPendingInventoryItemId)
{
	if (!HasAuthority()) return;

	RemainingTime = FMath::Max(InRemainingTime, 0.0f);
	Energy = FMath::Max(InEnergy, 0);
	PublicStatusEffects = InEffects;
	bAbandoned = bInAbandoned;
	InventoryItemIds = InInventoryItemIds;
	LockedInventoryItemIds = InLockedInventoryItemIds;
	bUsedItemThisTurn = bInUsedItemThisTurn;
	PendingInventoryItemId = InPendingInventoryItemId;
}

void AGomokuPlayerState::SetReady(bool bInReady)
{
	if (!HasAuthority()) return;

	bReady = bInReady;
}

void AGomokuPlayerState::SetGomokuBot(bool bInBot)
{
	if (!HasAuthority()) return;

	bGomokuBot = bInBot;
	SetIsABot(bInBot);
}
