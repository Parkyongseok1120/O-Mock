// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GomokuRoomSettings.generated.h"

/** Host-selected rules advertised in the LAN room browser and applied authoritatively by GameMode. */
USTRUCT(BlueprintType)
struct FGomokuRoomSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room", meta = (ClampMin = 2, ClampMax = 4))
	int32 MaxPlayers = 4;

	/** Planned server-controlled seats. Human players always replace planned bots when the lobby fills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room", meta = (ClampMin = 0, ClampMax = 3))
	int32 BotCount = 0;

	/** Square board template size. Supported UI templates are 15, 17, 19, 21 and 23. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room", meta = (ClampMin = 15, ClampMax = 23))
	int32 BoardSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room", meta = (ClampMin = 30, ClampMax = 600))
	int32 PersonalTimeSeconds = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room", meta = (ClampMin = 5, ClampMax = 120))
	int32 TurnTimeSeconds = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room")
	bool bItemsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room")
	bool bMiniGameEnabled = true;

	/** Host-side plaintext only. It is converted to a one-way LAN challenge digest before session travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Room")
	FString Password;

	void Sanitize()
	{
		MaxPlayers = FMath::Clamp(MaxPlayers, 2, 4);
		BotCount = FMath::Clamp(BotCount, 0, MaxPlayers - 1);
		static const int32 SupportedSizes[] = { 15, 17, 19, 21, 23 };
		int32 ClosestSize = SupportedSizes[0];
		for (const int32 Candidate : SupportedSizes)
		{
			if (FMath::Abs(BoardSize - Candidate) < FMath::Abs(BoardSize - ClosestSize))
			{
				ClosestSize = Candidate;
			}
		}
		BoardSize = ClosestSize;
		if (MaxPlayers == 4)
		{
			BoardSize = FMath::Max(BoardSize, 21);
		}
		PersonalTimeSeconds = FMath::Clamp(PersonalTimeSeconds, 30, 600);
		TurnTimeSeconds = FMath::Clamp(TurnTimeSeconds, 5, 120);
		Password.TrimStartAndEndInline();
		Password = Password.Left(32);
	}
};
