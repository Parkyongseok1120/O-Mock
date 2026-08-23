// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GomokuTypes.h"
#include "GomokuPredictionLibrary.generated.h"

/** Pure client-safe predictions derived only from the replicated public board snapshot. */
UCLASS()
class O_MOCK_API UGomokuPredictionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static TArray<FIntPoint> FindImmediateWinCells(const TArray<ECellState>& Cells,
		int32 SizeX, int32 SizeY, int32 PlayerId, int32 WinLength = 5);

	static TArray<FIntPoint> FindStealTargets(const TArray<ECellState>& Cells,
		int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells);

	static TArray<FIntPoint> FindPullDestinations(const TArray<ECellState>& Cells,
		int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells);

	/** Uses the same north/east/south/west source priority as authoritative Pull validation. */
	static bool FindPullSource(const TArray<ECellState>& Cells, int32 SizeX, int32 SizeY,
		int32 PlayerId, const FIntPoint& Destination, const TArray<FIntPoint>& GuardianCells,
		FIntPoint& OutSource);

	static TArray<FIntPoint> FindGuardianTargets(const TArray<ECellState>& Cells,
		int32 SizeX, int32 SizeY, int32 PlayerId, const TArray<FIntPoint>& GuardianCells);

private:
	static bool IsInside(int32 X, int32 Y, int32 SizeX, int32 SizeY);
	static ECellState GetCell(const TArray<ECellState>& Cells, int32 SizeX, int32 SizeY, int32 X, int32 Y);
	static bool IsIsolated(const TArray<ECellState>& Cells, int32 SizeX, int32 SizeY, const FIntPoint& Cell);
};
