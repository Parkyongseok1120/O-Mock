// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GomokuTypes.generated.h"

UENUM(BlueprintType)
enum class ECellState : uint8
{
	Empty UMETA(DisplayName = "Empty"),
	Player1 UMETA(DisplayName = "Player 1"),
	Player2 UMETA(DisplayName = "Player 2"),
	Player3 UMETA(DisplayName = "Player 3"),
	Player4 UMETA(DisplayName = "Player 4"),
	Blocked UMETA(DisplayName = "Blocked")
};

USTRUCT(BlueprintType)
struct FBoardCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	ECellState State = ECellState::Empty;

	FBoardCell() = default;

	FBoardCell(int32 InX, int32 InY, ECellState InState)
		: X(InX)
		, Y(InY)
		, State(InState)
	{
	}
};

USTRUCT(BlueprintType)
struct FGomokuPlayerStateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 PlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	float RemainingTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 Energy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	bool bSkipNextTurn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	bool bHasAbandoned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Items")
	TArray<int32> ItemIds;

	static FGomokuPlayerStateData CreateDefault(int32 InPlayerId, float InTimeLimit, int32 InInitialEnergy)
	{
		FGomokuPlayerStateData Data;
		Data.PlayerId = InPlayerId;
		Data.RemainingTime = InTimeLimit;
		Data.Energy = InInitialEnergy;
		Data.bSkipNextTurn = false;
		Data.bHasAbandoned = false;
		return Data;
	}
};

USTRUCT(BlueprintType)
struct FGomokuMatchConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 BoardSizeX = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 BoardSizeY = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 WinLength = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 MaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	float TurnTimeLimit = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 InitialEnergyPerPlayer = 10;

	static FGomokuMatchConfig CreateDefault()
	{
		return FGomokuMatchConfig();
	}
};

USTRUCT(BlueprintType)
struct FGomokuBoardState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 SizeX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	int32 SizeY = 0;

	/** Row-major cells: index = Y * SizeX + X */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku")
	TArray<ECellState> Cells;

	void Reset(int32 InSizeX, int32 InSizeY)
	{
		SizeX = FMath::Max(0, InSizeX);
		SizeY = FMath::Max(0, InSizeY);
		Cells.Init(ECellState::Empty, SizeX * SizeY);
	}

	bool IsInside(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < SizeX && Y < SizeY;
	}

	int32 ToIndex(int32 X, int32 Y) const
	{
		return Y * SizeX + X;
	}

	ECellState Get(int32 X, int32 Y) const
	{
		if (!IsInside(X, Y))
		{
			return ECellState::Empty;
		}
		return Cells[ToIndex(X, Y)];
	}

	void Set(int32 X, int32 Y, ECellState State)
	{
		if (IsInside(X, Y))
		{
			Cells[ToIndex(X, Y)] = State;
		}
	}

	bool HasEmptyCell() const
	{
		for (const ECellState State : Cells)
		{
			if (State == ECellState::Empty)
			{
				return true;
			}
		}
		return false;
	}
};

USTRUCT(BlueprintType)
struct FGomokuWinResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Win")
	bool IsWin = false;

	/** 0-based index into player order (GameState's CurrentPlayerIndex style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Win")
	int32 WinnerPlayerIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Win")
	FIntPoint WinCell;
};
