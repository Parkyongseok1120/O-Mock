// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GomokuBoardTemplateTypes.h"
#include "GomokuBoardTemplateDataAsset.generated.h"

/**
 * Data asset that defines a Gomoku board template: size, blocked cells, presets.
 */
UCLASS(BlueprintType)
class O_MOCK_API UGomokuBoardTemplateDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGomokuBoardTemplateDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	FString TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	int32 Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	int32 Height = 15;

	/** Cells that are permanently blocked (no stone placement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	TArray<FIntPoint> BlockedCells;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Template")
	FBoardTemplateData ToBoardTemplateData() const;

	/** Create a preset template for common sizes (15/17/19/21/23). */
	static UGomokuBoardTemplateDataAsset* CreatePresetForSize(int32 Size);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Template")
	bool IsValid() const;
};
