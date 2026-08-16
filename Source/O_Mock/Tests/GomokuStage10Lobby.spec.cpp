// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuGameMode.h"
#include "GomokuGameState.h"
#include "GomokuPlayerController.h"
#include "GomokuPlayerState.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage10_LobbyReadinessControlsMatchStart,
	TEXT("Gomoku.Stage10.LobbyReadinessControlsMatchStart"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage10_LobbyReadinessControlsMatchStart::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World created"), World))
		return false;

	AGomokuGameMode* GameMode = World->SpawnActor<AGomokuGameMode>();
	AGomokuGameState* GameState = World->SpawnActor<AGomokuGameState>();
	AGomokuPlayerController* Host = World->SpawnActor<AGomokuPlayerController>();
	AGomokuPlayerState* HostState = World->SpawnActor<AGomokuPlayerState>();
	AGomokuPlayerState* GuestState = World->SpawnActor<AGomokuPlayerState>();

	if (!TestNotNull(TEXT("GameMode created"), GameMode) ||
		!TestNotNull(TEXT("GameState created"), GameState) ||
		!TestNotNull(TEXT("Host controller created"), Host) ||
		!TestNotNull(TEXT("Host state created"), HostState) ||
		!TestNotNull(TEXT("Guest state created"), GuestState))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameMode->GameState = GameState;
	Host->PlayerState = HostState;
	GameState->PlayerArray.Add(HostState);
	GameState->PlayerArray.Add(GuestState);
	HostState->SetIdentity(1, FLinearColor::White);
	GuestState->SetIdentity(2, FLinearColor::Blue);

	if (!TestFalse(TEXT("Not all players ready initially"), GameMode->AreAllPlayersReady()))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameMode->SetPlayerReady(Host, true);
	GuestState->SetReady(true);
	if (!TestTrue(TEXT("All players ready after both confirmations"), GameMode->AreAllPlayersReady()))
	{
		World->DestroyWorld(false);
		return false;
	}

	const bool bStarted = GameMode->TryStartMatch(Host, TEXT(""));
	const bool bValid =
		TestTrue(TEXT("Host can start ready lobby"), bStarted) &&
		TestTrue(TEXT("Match is marked started"), GameMode->bMatchStarted) &&
		TestTrue(TEXT("GameState is active after start"), GameState->IsGameActive) &&
		TestEqual(TEXT("Lobby player count becomes match player count"), GameState->LocalPlayerCount, 2) &&
		TestEqual(TEXT("Match starts in playing phase"), GameState->MatchPhase, EMatchPhase::Playing);

	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
