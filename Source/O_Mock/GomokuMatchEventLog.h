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
		E.Type = Type;
		E.PlayerId = PlayerId;
		E.RoundIndex = RoundIndex;
		E.Cell = Cell;
		E.Tag = Tag;
		Events.Add(E);
	}

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Events")
	void Clear() { Events.Reset(); }

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
