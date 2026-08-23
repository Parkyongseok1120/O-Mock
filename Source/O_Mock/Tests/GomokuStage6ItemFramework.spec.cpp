// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "GomokuItemTypes.h"
#include "GomokuItemLibrary.h"
#include "GomokuPlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"

static UGomokuRuleEngine* MakeStage6Engine(int32 MaxPlayers, int32 BoardSize = 15)
{
	UObject* Outer = GetTransientPackage();
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(Outer);

	FGomokuMatchConfig Config;
	Config.BoardSizeX = BoardSize;
	Config.BoardSizeY = BoardSize;
	Config.WinLength = 5;
	Config.MaxPlayers = MaxPlayers;
	Config.TurnTimeLimit = 30.f;
	Config.InitialEnergyPerPlayer = 10;

	Engine->InitializeMatch(Config);
	return Engine;
}

// Test: Gomoku.Stage6.ItemTypesExist
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage6_ItemTypesExist,
	TEXT("Gomoku.Stage6.ItemTypesExist"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage6_ItemTypesExist::RunTest(const FString& Parameters)
{
	// Contract: only registered item IDs (1..5) are valid.
	// Verify SealStone (id=1) is recognized as a registered item.
	const int32 SealStoneId = 1;

	if (!UGomokuItemLibrary::IsRegisteredItemId(SealStoneId))
	{
		AddError(TEXT("SealStone (id=1) must be a registered item"));
		return false;
	}

	// Unregistered id (e.g. 101) must not be considered valid.
	if (UGomokuItemLibrary::IsRegisteredItemId(101))
	{
		AddError(TEXT("Unregistered ItemId=101 must not be treated as registered"));
		return false;
	}

	FItemData SealData;
	if (!TestTrue(TEXT("Shared item metadata resolves Seal"), UGomokuItemLibrary::GetItemData(SealStoneId, SealData))
		|| !TestEqual(TEXT("Seal metadata name"), SealData.DisplayName.ToString(), FString(TEXT("Seal")))
		|| !TestEqual(TEXT("Seal metadata energy cost"), SealData.EnergyCost, 1)
		|| !TestEqual(TEXT("Seal metadata target"), SealData.TargetType, EItemTargetType::Cell))
	{
		return false;
	}
	FItemData UnknownData;
	if (!TestFalse(TEXT("Unknown item metadata is rejected"), UGomokuItemLibrary::GetItemData(101, UnknownData)))
	{
		return false;
	}

	return true;
}

// Test: Gomoku.Stage6.CanUseValidateExecuteSeparated
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage6_CanUseValidateExecuteSeparated,
	TEXT("Gomoku.Stage6.CanUseValidateExecuteSeparated"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage6_CanUseValidateExecuteSeparated::RunTest(const FString& Parameters)
{
	auto* Engine = MakeStage6Engine(2, 15);
	const int32 PlayerId = 1;

	// A) Registered item: SealStone (id=1), target empty cell.
	const int32 SealStoneId = 1;
	FIntPoint TargetCell(0, 0);
	int32 TargetPlayerIndex = -1;

	if (!TestFalse(TEXT("A-CanUseItem before add"), UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
		return false;

	bool AddedSeal = Engine->AddItemToInventory(PlayerId, SealStoneId);
	if (!TestTrue(TEXT("A-AddItemToInventory succeeds for 1"), AddedSeal))
		return false;

	const int32 EnergyBefore = Engine->GetPlayerStateData(PlayerId).Energy;

	if (!TestTrue(TEXT("A-CanUseItem after add"), UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
		return false;

	bool ValidTargetSeal = UGomokuItemLibrary::ValidateTarget(Engine, SealStoneId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("A-ValidateTarget on empty cell"), ValidTargetSeal))
		return false;

	bool ExecutedSeal = UGomokuItemLibrary::ExecuteItem(Engine, SealStoneId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestTrue(TEXT("A-ExecuteItem succeeds for 1"), ExecutedSeal))
		return false;

	const int32 EnergyAfter = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("A-Energy decreased by cost(1)"), EnergyBefore - EnergyAfter, 1))
		return false;

	if (Engine->PlayerHasItem(PlayerId, SealStoneId))
	{
		AddError(TEXT("SealStone should be removed from inventory after execute"));
		return false;
	}

	if (!TestFalse(TEXT("A-CanUseItem after execute is false"), UGomokuItemLibrary::CanUseItem(Engine, PlayerId, SealStoneId)))
		return false;

	// B) Unregistered item: id=101 must be rejected.
	const int32 BadItemId = 101;

	bool AddedBad = Engine->AddItemToInventory(PlayerId, BadItemId);
	if (!TestFalse(TEXT("B-AddItemToInventory fails for unregistered 101"), AddedBad))
		return false;

	if (!TestFalse(TEXT("B-CanUseItem false for 101"), UGomokuItemLibrary::CanUseItem(Engine, PlayerId, BadItemId)))
		return false;

	bool ValidTargetBad = UGomokuItemLibrary::ValidateTarget(Engine, BadItemId, TargetCell, TargetPlayerIndex);
	if (!TestFalse(TEXT("B-ValidateTarget false for 101"), ValidTargetBad))
		return false;

	const int32 EnergyBeforeBad = Engine->GetPlayerStateData(PlayerId).Energy;
	bool ExecutedBad = UGomokuItemLibrary::ExecuteItem(Engine, BadItemId, PlayerId, TargetCell, TargetPlayerIndex);
	if (!TestFalse(TEXT("B-ExecuteItem false for 101"), ExecutedBad))
		return false;

	const int32 EnergyAfterBad = Engine->GetPlayerStateData(PlayerId).Energy;
	if (!TestEqual<int32>(TEXT("B-Energy unchanged after failed execute"), EnergyBeforeBad, EnergyAfterBad))
		return false;

	return true;
}

// Test: Gomoku.Stage6.SelectCancelTargeting
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage6_SelectCancelTargeting,
	TEXT("Gomoku.Stage6.SelectCancelTargeting"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage6_SelectCancelTargeting::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!IsValid(World))
	{
		AddError(TEXT("Failed to create world for SelectCancelTargeting test"));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGomokuPlayerController* PC = World->SpawnActor<AGomokuPlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!PC)
	{
		AddError(TEXT("Failed to spawn AGomokuPlayerController"));
		return false;
	}

	const int32 TestItemId = 5;

	if (PC->bItemTargetingActive || PC->SelectedItemId != 0)
	{
		AddError(TEXT("Initial item targeting state is incorrect"));
		return false;
	}

	PC->SelectItem(TestItemId);

	if (!TestTrue(TEXT("bItemTargetingActive after SelectItem"), PC->bItemTargetingActive))
		return false;

	if (!TestEqual<int32>(TEXT("SelectedItemId after SelectItem"), PC->SelectedItemId, TestItemId))
		return false;

	PC->CancelItemTargeting();

	if (PC->bItemTargetingActive)
	{
		AddError(TEXT("bItemTargetingActive not cleared after CancelItemTargeting"));
		return false;
	}

	if (!TestEqual<int32>(TEXT("SelectedItemId after cancel"), PC->SelectedItemId, 0))
		return false;

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
