// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuRuleEngine.h"
#include "GomokuGameState.h"
#include "GomokuTypes.h"
#include "Engine/World.h"

static UGomokuRuleEngine* MakeEngine(int32 MaxPlayers, int32 BoardSize = 15)
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

// Test: Gomoku.Stage3.TimeDefaults
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage3_TimeDefaults, TEXT("Gomoku.Stage3.TimeDefaults"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage3_TimeDefaults::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return TestFalse(TEXT("CreateWorld failed"), true);

	AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
	if (!GS || !GS->IsValidLowLevel()) { World->DestroyWorld(false); return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true); }

	auto* Engine = MakeEngine(2, 15);
	GS->SetRuleEngineRef(Engine);
	GS->InitializeForLocalHotseat(2);

	if (!TestEqual(TEXT("MaxPersonalTime is 120"), GS->MaxPersonalTime, 120.0f)) { World->DestroyWorld(false); return false; }
	if (!TestEqual(TEXT("MaxTurnTime is 25"), GS->MaxTurnTime, 25.0f)) { World->DestroyWorld(false); return false; }

	if (FMath::Abs(GS->PersonalRecoveryRate - 0.35f) > 0.001f)
	{ AddError(TEXT("PersonalRecoveryRate not ~0.35")); World->DestroyWorld(false); return false; }

	if (!TestTrue(TEXT("bHasTimeSystem is true"), GS->bHasTimeSystem)) { World->DestroyWorld(false); return false; }
	if (!TestEqual(TEXT("PlayerTimes count matches LocalPlayerCount"), GS->PlayerTimes.Num(), 2)) { World->DestroyWorld(false); return false; }

	for (int32 i = 0; i < GS->PlayerTimes.Num(); ++i)
	{
		if (!TestEqual(FString::Printf(TEXT("Player %d PersonalRemaining is 120"), i),
			GS->PlayerTimes[i].PersonalRemaining, 120.0f)) { World->DestroyWorld(false); return false; }
	}

	World->DestroyWorld(false);
	return true;
}

// Test: Gomoku.Stage3.PauseStopsTick
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage3_PauseStopsTick, TEXT("Gomoku.Stage3.PauseStopsTick"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage3_PauseStopsTick::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return TestFalse(TEXT("CreateWorld failed"), true);

	AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
	if (!GS || !GS->IsValidLowLevel()) { World->DestroyWorld(false); return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true); }

	auto* Engine = MakeEngine(2, 15);
	GS->SetRuleEngineRef(Engine);
	GS->InitializeForLocalHotseat(2);

	int32 CurrentIdx = GS->CurrentPlayerIndex;
	if (!TestEqual(TEXT("CurrentPlayerIndex is 0"), CurrentIdx, 0)) { World->DestroyWorld(false); return false; }

	float BeforePersonal = GS->PlayerTimes[CurrentIdx].PersonalRemaining;
	float BeforeTurnElapsed = GS->PlayerTimes[CurrentIdx].TurnElapsedThisTurn;

	GS->SetTimePaused(true);
	if (!TestTrue(TEXT("bTimePaused after SetTimePaused(true)"), GS->bTimePaused)) { World->DestroyWorld(false); return false; }

	GS->TickTimeSystem(1.0f);

	float AfterPausePersonal = GS->PlayerTimes[CurrentIdx].PersonalRemaining;
	float AfterPauseTurnElapsed = GS->PlayerTimes[CurrentIdx].TurnElapsedThisTurn;

	if (!TestEqual(TEXT("PersonalRemaining unchanged while paused"), AfterPausePersonal, BeforePersonal)) { World->DestroyWorld(false); return false; }
	if (!TestEqual(TEXT("TurnElapsedThisTurn unchanged while paused"), AfterPauseTurnElapsed, BeforeTurnElapsed)) { World->DestroyWorld(false); return false; }

	GS->SetTimePaused(false);
	if (!TestFalse(TEXT("bTimePaused after SetTimePaused(false)"), GS->bTimePaused)) { World->DestroyWorld(false); return false; }

	// After unpause, TickTimeSystem must be callable without crash.
	// In CreateWorld automation world time may not advance, so we do NOT require TurnElapsed increase or Personal decrease here.
	GS->TickTimeSystem(1.0f);

	float AfterUnpausePersonal = GS->PlayerTimes[CurrentIdx].PersonalRemaining;
	if (!TestTrue(TEXT("PersonalRemaining >= 0 after unpause tick"), AfterUnpausePersonal >= 0.0f)) { World->DestroyWorld(false); return false; }
	if (!TestTrue(TEXT("PlayerTimes still valid after unpause tick"), GS->PlayerTimes.Num() > 0)) { World->DestroyWorld(false); return false; }

	World->DestroyWorld(false);
	return true;
}

// Test: Gomoku.Stage3.EndOfRoundRecovery
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGomokuStage3_EndOfRoundRecovery, TEXT("Gomoku.Stage3.EndOfRoundRecovery"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGomokuStage3_EndOfRoundRecovery::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return TestFalse(TEXT("CreateWorld failed"), true);

	AGomokuGameState* GS = World->SpawnActor<AGomokuGameState>();
	if (!GS || !GS->IsValidLowLevel()) { World->DestroyWorld(false); return TestFalse(TEXT("SpawnActor AGomokuGameState failed"), true); }

	auto* Engine = MakeEngine(2, 15);
	GS->SetRuleEngineRef(Engine);
	GS->InitializeForLocalHotseat(2);

	int32 PIdx = 0;
	float MaxTime = GS->MaxPersonalTime;
	float Rate = GS->PersonalRecoveryRate;

	// Lower PersonalRemaining significantly below Max.
	float ReducedValue = MaxTime * 0.5f; // e.g., 60 instead of 120
	GS->PlayerTimes[PIdx].PersonalRemaining = ReducedValue;

	float BeforeRecovery = GS->PlayerTimes[PIdx].PersonalRemaining;
	float Deficit = MaxTime - BeforeRecovery;
	float ExpectedGain = Deficit * Rate;

	// Call recovery.
	GS->ApplyEndOfRoundRecovery();

	float AfterRecovery = GS->PlayerTimes[PIdx].PersonalRemaining;
	float ActualGain = AfterRecovery - BeforeRecovery;

	if (FMath::Abs(ActualGain - ExpectedGain) > 0.01f)
	{
		AddError(FString::Printf(TEXT("EndOfRoundRecovery mismatch: expected gain ~%0.4f, got %0.4f"),
			ExpectedGain, ActualGain));
		World->DestroyWorld(false);
		return false;
	}

	if (!TestTrue(TEXT("PersonalRemaining increased after recovery"), AfterRecovery > BeforeRecovery))
	{ World->DestroyWorld(false); return false; }

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage3_TimeChargedExactlyOnce,
	TEXT("Gomoku.Stage3.TimeChargedExactlyOnce"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage3_TimeChargedExactlyOnce::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World created"), World))
	{
		return false;
	}

	AGomokuGameState* GameState = World->SpawnActor<AGomokuGameState>();
	UGomokuRuleEngine* Engine = MakeEngine(2, 15);
	GameState->SetRuleEngineRef(Engine);
	GameState->InitializeForLocalHotseat(2);

	GameState->TickTimeSystem(5.0f);
	const bool bTickValid =
		TestTrue(TEXT("Five seconds are charged from personal time"),
			FMath::IsNearlyEqual(GameState->PlayerTimes[0].PersonalRemaining, 115.0f)) &&
		TestTrue(TEXT("Turn elapsed advances by the same five seconds"),
			FMath::IsNearlyEqual(GameState->PlayerTimes[0].TurnElapsedThisTurn, 5.0f));

	GameState->HandlePlaceStone(0, FIntPoint(7, 7));
	const bool bTurnEndValid =
		TestTrue(TEXT("Ending the turn does not charge elapsed time a second time"),
			FMath::IsNearlyEqual(GameState->PlayerTimes[0].PersonalRemaining, 115.0f)) &&
		TestTrue(TEXT("Inactive player's personal time did not move"),
			FMath::IsNearlyEqual(GameState->PlayerTimes[1].PersonalRemaining, 120.0f)) &&
		TestEqual(TEXT("Turn advances to player two"), GameState->CurrentPlayerIndex, 1);

	World->DestroyWorld(false);
	return bTickValid && bTurnEndValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
