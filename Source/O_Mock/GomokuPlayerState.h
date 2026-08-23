// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GomokuPlayerState.generated.h"

UCLASS()
class O_MOCK_API AGomokuPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGomokuPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	int32 GomokuPlayerId = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	FLinearColor StoneColor = FLinearColor::White;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	float RemainingTime = 120.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	int32 Energy = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	TArray<FName> PublicStatusEffects;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	bool bAbandoned = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku")
	bool bReady = false;

	/** Server-owned participant driven by the authoritative bot turn processor. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Bot")
	bool bGomokuBot = false;

	/** Owner-only inventory replication; other clients should not see hidden item contents. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Items")
	TArray<int32> InventoryItemIds;

	/** Owner-only list of items gained this turn and therefore not usable yet. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Items")
	TArray<int32> LockedInventoryItemIds;

	/** Owner-only one-item-per-turn state used by the interactive inventory. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Items")
	bool bUsedItemThisTurn = false;

	/** Owner-only new-item offer shown when the inventory was full at turn start. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Items")
	int32 PendingInventoryItemId = 0;

public:
	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetIdentity(int32 InId, const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetPublicMatchState(float InRemainingTime, int32 InEnergy,
		const TArray<FName>& InEffects, bool bInAbandoned, const TArray<int32>& InInventoryItemIds,
		const TArray<int32>& InLockedInventoryItemIds, bool bInUsedItemThisTurn, int32 InPendingInventoryItemId);

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetReady(bool bInReady);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Bot")
	void SetGomokuBot(bool bInBot);
};
