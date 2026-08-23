// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GomokuItemTypes.generated.h"

/** Where/how an item is applied. */
UENUM(BlueprintType)
enum class EItemTargetType : uint8
{
	None UMETA(DisplayName = "None"),
	Cell UMETA(DisplayName = "Cell"),
	Player UMETA(DisplayName = "Player"),
	Self UMETA(DisplayName = "Self")
};

/**
 * High-level item type categories.
 * Concrete effects implemented in Stage 7 via ExecuteItem / RuleEngine helpers.
 */
UENUM(BlueprintType)
enum class EGomokuItemType : uint8
{
	None UMETA(DisplayName = "None"),

	// SealStone: empty cell -> Blocked (SetCellBlocked).
	SealStone UMETA(DisplayName = "Seal Stone"),

	// Pull: move an adjacent allied stone into the selected empty target.
	Pull UMETA(DisplayName = "Pull"),

	// Steal: reassign opponent stone to current player, unless protected by GuardianBarrier.
	Steal UMETA(DisplayName = "Steal"),

	// SkipTurn: set bSkipNextTurn on target player.
	SkipTurn UMETA(DisplayName = "Skip Turn"),

	// GuardianBarrier: mark cell protected so Steal/Pull fail against it.
	GuardianBarrier UMETA(DisplayName = "Guardian Barrier")
};

/** Minimal item metadata used by inventory / UI. */
USTRUCT(BlueprintType)
struct FItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	int32 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	int32 EnergyCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	EItemTargetType TargetType = EItemTargetType::None;

	/** Short effect explanation shown by the inventory card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	FText Description;

	/** Contextual instruction shown while choosing a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Item")
	FText TargetInstruction;

	FItemData() = default;

	FItemData(int32 InId, const FText& InName, int32 InCost, EItemTargetType InTarget,
		const FText& InDescription, const FText& InTargetInstruction)
		: Id(InId)
		, DisplayName(InName)
		, EnergyCost(InCost)
		, TargetType(InTarget)
		, Description(InDescription)
		, TargetInstruction(InTargetInstruction)
	{
	}
};
