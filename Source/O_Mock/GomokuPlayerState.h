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

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku")
	int32 GomokuPlayerId = 0;
};
