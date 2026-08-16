// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GomokuMinigameTypes.generated.h"

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Waiting UMETA(DisplayName = "Waiting"),
	Playing UMETA(DisplayName = "Playing"),
	GameOver UMETA(DisplayName = "Game Over"),
	MiniGameIntro UMETA(DisplayName = "Mini Game Intro"),
	MiniGamePlaying UMETA(DisplayName = "Mini Game Playing"),
	MiniGameResult UMETA(DisplayName = "Mini Game Result")
};

UENUM(BlueprintType)
enum class EMatchEventType : uint8
{
	None UMETA(DisplayName = "None"),
	TurnStarted UMETA(DisplayName = "Turn Started"),
	StonePlaced UMETA(DisplayName = "Stone Placed"),
	ItemUsed UMETA(DisplayName = "Item Used"),
	StoneMoved UMETA(DisplayName = "Stone Moved"),
	StoneConverted UMETA(DisplayName = "Stone Converted"),
	TurnSkipped UMETA(DisplayName = "Turn Skipped"),
	RoundEnded UMETA(DisplayName = "Round Ended"),
	MiniGameStarted UMETA(DisplayName = "Mini Game Started"),
	MiniGameResult UMETA(DisplayName = "Mini Game Result"),
	PlayerWon UMETA(DisplayName = "Player Won"),
	PlayerLeft UMETA(DisplayName = "Player Left")
};

USTRUCT(BlueprintType)
struct FMatchEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	int32 SequenceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	EMatchEventType Type = EMatchEventType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	int32 PlayerId = 0;

	/** Explicit instigator field for replay/network consumers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	int32 InstigatorPlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	int32 TargetPlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	int32 RoundIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	FIntPoint Cell = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	FIntPoint TargetCell = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Event")
	FName Tag;
};
