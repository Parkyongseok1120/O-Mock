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

	/** Owner-only inventory replication; other clients should not see hidden item contents. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gomoku|Items")
	TArray<int32> InventoryItemIds;

public:
	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetIdentity(int32 InId, const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetPublicMatchState(float InRemainingTime, int32 InEnergy,
		const TArray<FName>& InEffects, bool bInAbandoned, const TArray<int32>& InInventoryItemIds);

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void SetReady(bool bInReady);
};
