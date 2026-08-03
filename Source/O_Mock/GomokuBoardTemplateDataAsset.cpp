// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBoardTemplateDataAsset.h"

UGomokuBoardTemplateDataAsset::UGomokuBoardTemplateDataAsset()
{
	Width = 15;
	Height = 15;
}

FBoardTemplateData UGomokuBoardTemplateDataAsset::ToBoardTemplateData() const
{
	FBoardTemplateData Data;
	Data.TemplateId = TemplateId.IsEmpty() ? FString::Printf(TEXT("Custom_%dx%d"), Width, Height) : TemplateId;
	Data.Width = FMath::Max(3, Width);
	Data.Height = FMath::Max(3, Height);
	Data.BlockedCells = BlockedCells;
	return Data;
}

UGomokuBoardTemplateDataAsset* UGomokuBoardTemplateDataAsset::CreatePresetForSize(int32 Size)
{
	// Allowed sizes: 15, 17, 19, 21, 23 (odd only). Clamp to nearest allowed.
	TArray<int32> Allowed = { 15, 17, 19, 21, 23 };

	if (!Allowed.Contains(Size))
	{
		int32 Best = Allowed[0];
		float BestDist = FMath::Abs(Size - Best);
		for (int32 V : Allowed)
		{
			float D = FMath::Abs(Size - V);
			if (D < BestDist)
			{
				BestDist = D;
				Best = V;
			}
		}
		Size = Best;
	}

	UGomokuBoardTemplateDataAsset* Template = NewObject<UGomokuBoardTemplateDataAsset>();
	check(Template);

	Template->TemplateId = FString::Printf(TEXT("Preset_%d"), Size);
	Template->Width = Size;
	Template->Height = Size;

	return Template;
}

bool UGomokuBoardTemplateDataAsset::IsValid() const
{
	if (Width < 3 || Height < 3)
	{
		return false;
	}
	return true;
}
