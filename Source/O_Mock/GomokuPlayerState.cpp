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
	InventoryItemIds.Empty();
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
	DOREPLIFETIME_CONDITION(AGomokuPlayerState, InventoryItemIds, COND_OwnerOnly);
}

void AGomokuPlayerState::SetIdentity(int32 InId, const FLinearColor& InColor)
{
	if (!HasAuthority()) return;

	GomokuPlayerId = FMath::Clamp(InId, 0, 4);
	StoneColor = InColor;
}

void AGomokuPlayerState::SetPublicMatchState(float InRemainingTime, int32 InEnergy,
	const TArray<FName>& InEffects, bool bInAbandoned, const TArray<int32>& InInventoryItemIds)
{
	if (!HasAuthority()) return;

	RemainingTime = FMath::Max(InRemainingTime, 0.0f);
	Energy = FMath::Max(InEnergy, 0);
	PublicStatusEffects = InEffects;
	bAbandoned = bInAbandoned;
	InventoryItemIds = InInventoryItemIds;
}

void AGomokuPlayerState::SetReady(bool bInReady)
{
	if (!HasAuthority()) return;

	bReady = bInReady;
}
