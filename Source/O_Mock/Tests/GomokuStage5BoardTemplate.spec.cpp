// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "GomokuBoardTemplateDataAsset.h"
#include "GomokuBoardTemplateTypes.h"
#include "GomokuGameMode.h"
#include "Engine/World.h"

// Helper: create a rule engine with given config.
static UGomokuRuleEngine* MakeEngine(const FGomokuMatchConfig& Config)
{
	UObject* Outer = GetTransientPackage();
	UGomokuRuleEngine* Engine = NewObject<UGomokuRuleEngine>(Outer);
	Engine->InitializeMatch(Config);
	return Engine;
}

// Helper: create a default config with square board and 2 players.
static FGomokuMatchConfig MakeDefaultConfig(int32 Size)
{
	FGomokuMatchConfig C;
	C.BoardSizeX = Size;
	C.BoardSizeY = Size;
	C.WinLength = 5;
	C.MaxPlayers = 2;
	C.TurnTimeLimit = 30.f;
	C.InitialEnergyPerPlayer = 10;
	return C;
}

// Test: Gomoku.Stage5.PresetSizes1517192123
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage5_PresetSizes1517192123,
	TEXT("Gomoku.Stage5.PresetSizes1517192123"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage5_PresetSizes1517192123::RunTest(const FString& Parameters)
{
	TArray<int32> Sizes;
	Sizes.Add(15);
	Sizes.Add(17);
	Sizes.Add(19);
	Sizes.Add(21);
	Sizes.Add(23);

	for (int32 Size : Sizes)
	{
		UGomokuBoardTemplateDataAsset* Template = UGomokuBoardTemplateDataAsset::CreatePresetForSize(Size);
		if (!TestNotNull(FString::Printf(TEXT("Template for size %d"), Size), Template))
			return false;

		if (!TestEqual(FString::Printf(TEXT("Width == %d"), Size), Template->Width, Size))
			return false;
		if (!TestEqual(FString::Printf(TEXT("Height == %d"), Size), Template->Height, Size))
			return false;

		FBoardTemplateData Data = Template->ToBoardTemplateData();
		if (!TestEqual(FString::Printf(TEXT("ToBoardTemplateData Width == %d"), Size), Data.Width, Size))
			return false;
		if (!TestEqual(FString::Printf(TEXT("ToBoardTemplateData Height == %d"), Size), Data.Height, Size))
			return false;

		FGomokuMatchConfig Config = MakeDefaultConfig(Size);
		Config.BlockedCells = Template->BlockedCells;

		auto* Engine = MakeEngine(Config);
		if (!TestNotNull(FString::Printf(TEXT("RuleEngine for size %d"), Size), Engine))
			return false;

		const FGomokuMatchConfig& MC = Engine->GetMatchConfig();
		if (!TestEqual(FString::Printf(TEXT("BoardSizeX == %d"), Size), MC.BoardSizeX, Size))
			return false;
		if (!TestEqual(FString::Printf(TEXT("BoardSizeY == %d"), Size), MC.BoardSizeY, Size))
			return false;

		bool CornerA = Engine->IsValidEmpty(FIntPoint(0, 0));
		if (!TestTrue(FString::Printf(TEXT("IsValidEmpty(0,0) for size %d"), Size), CornerA))
			return false;

		bool CornerB = Engine->IsValidEmpty(FIntPoint(Size - 1, Size - 1));
		if (!TestTrue(FString::Printf(TEXT("IsValidEmpty(%d,%d) for size %d"), Size - 1, Size - 1, Size), CornerB))
			return false;

		bool OutOfBounds = Engine->IsValidEmpty(FIntPoint(Size, 0));
		if (!TestFalse(FString::Printf(TEXT("IsValidEmpty(%d,0) out-of-bounds for size %d"), Size, Size), OutOfBounds))
			return false;
	}

	return true;
}

// Test: Gomoku.Stage5.BlockedCellsRejectPlacement
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage5_BlockedCellsRejectPlacement,
	TEXT("Gomoku.Stage5.BlockedCellsRejectPlacement"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage5_BlockedCellsRejectPlacement::RunTest(const FString& Parameters)
{
	FGomokuMatchConfig Config = MakeDefaultConfig(15);
	Config.MaxPlayers = 2;
	Config.BlockedCells.Add(FIntPoint(7, 7));

	auto* Engine = MakeEngine(Config);
	if (!TestNotNull(TEXT("RuleEngine with blocked cell"), Engine))
		return false;

	bool CenterEmpty = Engine->IsValidEmpty(FIntPoint(7, 7));
	if (!TestFalse(TEXT("Blocked center (7,7) not IsValidEmpty"), CenterEmpty))
		return false;

	bool PlacedOnBlocked = Engine->TryPlaceStone(1, 7, 7);
	if (!TestFalse(TEXT("TryPlaceStone on blocked (7,7) fails"), PlacedOnBlocked))
		return false;

	bool CornerEmpty = Engine->IsValidEmpty(FIntPoint(0, 0));
	if (!TestTrue(TEXT("Corner (0,0) IsValidEmpty"), CornerEmpty))
		return false;

	bool PlacedOnCorner = Engine->TryPlaceStone(1, 0, 0);
	if (!TestTrue(TEXT("TryPlaceStone on (0,0) succeeds"), PlacedOnCorner))
		return false;

	return true;
}

// Test: Gomoku.Stage5.ApplyBoardTemplateUpdatesConfig
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage5_ApplyBoardTemplateUpdatesConfig,
	TEXT("Gomoku.Stage5.ApplyBoardTemplateUpdatesConfig"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage5_ApplyBoardTemplateUpdatesConfig::RunTest(const FString& Parameters)
{
	UObject* Outer = GetTransientPackage();
	UGomokuBoardTemplateDataAsset* Template = NewObject<UGomokuBoardTemplateDataAsset>(Outer);
	Template->Width = 21;
	Template->Height = 21;
	Template->BlockedCells.Add(FIntPoint(1, 1));

	if (!TestTrue(TEXT("Template IsValid"), Template->IsValid()))
		return false;

	FGomokuMatchConfig Config = MakeDefaultConfig(Template->Width);
	Config.BoardSizeY = Template->Height;
	Config.BlockedCells = Template->BlockedCells;

	auto* Engine = MakeEngine(Config);
	if (!TestNotNull(TEXT("RuleEngine from template config"), Engine))
		return false;

	const FGomokuMatchConfig& MC = Engine->GetMatchConfig();
	if (!TestEqual(TEXT("BoardSizeX == 21"), MC.BoardSizeX, 21))
		return false;
	if (!TestEqual(TEXT("BoardSizeY == 21"), MC.BoardSizeY, 21))
		return false;

	bool BlockedValid = Engine->IsValidEmpty(FIntPoint(1, 1));
	if (!TestFalse(TEXT("Blocked (1,1) not IsValidEmpty"), BlockedValid))
		return false;

	bool CornerValid = Engine->IsValidEmpty(FIntPoint(0, 0));
	if (!TestTrue(TEXT("Corner (0,0) IsValidEmpty on 21x21"), CornerValid))
		return false;

	return true;
}

// Test: Gomoku.Stage5.WinCheckWorksOnLargerBoard
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage5_WinCheckWorksOnLargerBoard,
	TEXT("Gomoku.Stage5.WinCheckWorksOnLargerBoard"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage5_WinCheckWorksOnLargerBoard::RunTest(const FString& Parameters)
{
	FGomokuMatchConfig Config = MakeDefaultConfig(21);
	Config.WinLength = 5;
	Config.MaxPlayers = 2;

	auto* Engine = MakeEngine(Config);
	if (!TestNotNull(TEXT("RuleEngine 21x21"), Engine))
		return false;

	const FGomokuMatchConfig& MC = Engine->GetMatchConfig();
	if (!TestEqual(TEXT("BoardSizeX == 21"), MC.BoardSizeX, 21))
		return false;
	if (!TestEqual(TEXT("WinLength == 5"), MC.WinLength, 5))
		return false;

	TArray<FIntPoint> LineCells;
	for (int32 x = 0; x < 5; ++x)
	{
		FIntPoint P(x, 10);
		bool EmptyBefore = Engine->IsValidEmpty(P);
		if (!TestTrue(FString::Printf(TEXT("Cell (%d,10) empty before placement"), x), EmptyBefore))
			return false;

		bool Placed = Engine->TryPlaceStone(1, P.X, P.Y);
		if (!TestTrue(FString::Printf(TEXT("TryPlaceStone(%d,10) for player 1"), x), Placed))
			return false;

		LineCells.Add(P);
	}

	FGomokuWinResult Result = Engine->CheckWinAt(LineCells.Last());
	if (!TestTrue(TEXT("IsWin after 5 in a row"), Result.IsWin))
		return false;

	if (!TestEqual(TEXT("WinnerPlayerIndex == 0"), Result.WinnerPlayerIndex, 0))
		return false;

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage5_FourPlayersUseAtLeastTwentyOneByTwentyOne,
	TEXT("Gomoku.Stage5.FourPlayersUseAtLeastTwentyOneByTwentyOne"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage5_FourPlayersUseAtLeastTwentyOneByTwentyOne::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World created"), World))
	{
		return false;
	}

	AGomokuGameMode* GameMode = World->SpawnActor<AGomokuGameMode>();
	UGomokuBoardTemplateDataAsset* Template = NewObject<UGomokuBoardTemplateDataAsset>(GameMode);
	Template->Width = 15;
	Template->Height = 15;
	GameMode->DefaultHotseatPlayers = 4;
	GameMode->BoardTemplate = Template;
	GameMode->ApplyBoardTemplate();

	const FGomokuMatchConfig& Config = GameMode->GetRuleEngine()->GetMatchConfig();
	const bool bValid =
		TestEqual(TEXT("Four-player width is 21"), Config.BoardSizeX, 21) &&
		TestEqual(TEXT("Four-player height is 21"), Config.BoardSizeY, 21) &&
		TestEqual(TEXT("Four players remain configured"), Config.MaxPlayers, 4);

	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
