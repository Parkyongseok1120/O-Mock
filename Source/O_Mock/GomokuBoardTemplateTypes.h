// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GomokuBoardTemplateTypes.generated.h"

/**
 * Board template definition used by UGomokuBoardTemplateDataAsset.
 */
USTRUCT(BlueprintType)
struct FBoardTemplateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	FString TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	int32 Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	int32 Height = 15;

	/** Cells that are permanently blocked (no stone placement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	TArray<FIntPoint> BlockedCells;

	FBoardTemplateData() = default;

	FBoardTemplateData(const FString& InId, int32 InW, int32 InH)
		: TemplateId(InId), Width(InW), Height(InH)
	{
	}

	static FBoardTemplateData CreatePreset(int32 Size)
	{
		return FBoardTemplateData(
			FString::Printf(TEXT("Preset_%d"), Size),
			Size,
			Size);
	}
};
