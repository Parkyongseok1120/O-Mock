// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GomokuMinigameTypes.h"
#include "GomokuMatchEventLog.generated.h"

UCLASS()
class UGomokuMatchEventLog : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Events")
	void AppendEvent(EMatchEventType Type, int32 PlayerId, int32 RoundIndex, const FIntPoint& Cell, FName Tag)
	{
		FMatchEvent E;
		E.SequenceId = Events.Num() + 1;
		E.Type = Type;
		E.PlayerId = PlayerId;
		E.InstigatorPlayerId = PlayerId;
		E.RoundIndex = RoundIndex;
		E.Cell = Cell;
		E.TargetCell = Cell;
		E.Tag = Tag;
		Events.Add(E);
	}

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Events")
	void AppendItemEvent(EMatchEventType Type, int32 InstigatorPlayerId, int32 TargetPlayerId,
		const FIntPoint& TargetCell, int32 ItemNumericId, int32 RoundIndex)
	{
		FMatchEvent E;
		E.SequenceId = Events.Num() + 1;
		E.Type = Type;
		E.PlayerId = InstigatorPlayerId;
		E.InstigatorPlayerId = InstigatorPlayerId;
		E.TargetPlayerId = TargetPlayerId;
		E.RoundIndex = RoundIndex;
		E.Cell = TargetCell;
		E.TargetCell = TargetCell;
		E.ItemId = FName(*FString::FromInt(ItemNumericId));
		Events.Add(E);
	}

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Events")
	void Clear() { Events.Reset(); }

	UFUNCTION(BlueprintPure, Category = "Gomoku|Events")
	const TArray<FMatchEvent>& GetEvents() const { return Events; }

	UFUNCTION(BlueprintPure, Category = "Gomoku|Events")
	const FMatchEvent& GetLastEvent() const
	{
		static FMatchEvent Empty;
		if (Events.Num() == 0) { return Empty; }
		return Events.Last();
	}

private:
	UPROPERTY()
	TArray<FMatchEvent> Events;
};
