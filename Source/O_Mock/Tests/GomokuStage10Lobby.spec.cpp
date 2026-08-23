// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GomokuGameMode.h"
#include "GomokuGameState.h"
#include "GomokuPlayerController.h"
#include "GomokuPlayerState.h"
#include "GomokuGameInstance.h"
#include "GomokuItemLibrary.h"
#include "GomokuRoomSettings.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage10_CustomRoomSettingsAndPasswordDigest,
	TEXT("Gomoku.Stage10.CustomRoomSettingsAndPasswordDigest"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage10_CustomRoomSettingsAndPasswordDigest::RunTest(const FString& Parameters)
{
	FGomokuRoomSettings Settings;
	Settings.MaxPlayers = 4;
	Settings.BotCount = 99;
	Settings.BoardSize = 15;
	Settings.PersonalTimeSeconds = 999;
	Settings.TurnTimeSeconds = 1;
	Settings.Password = TEXT("  sample-room-password  ");
	Settings.Sanitize();
	const FString Digest = UGomokuGameInstance::BuildPasswordDigest(Settings.Password, TEXT("room-a"));
	return TestEqual(TEXT("Four-player room enforces the 21 board template"), Settings.BoardSize, 21)
		&& TestEqual(TEXT("Bot count leaves at least one human seat"), Settings.BotCount, 3)
		&& TestEqual(TEXT("Personal time clamps"), Settings.PersonalTimeSeconds, 600)
		&& TestEqual(TEXT("Turn time clamps"), Settings.TurnTimeSeconds, 5)
		&& TestEqual(TEXT("Password whitespace is normalized"), Settings.Password, FString(TEXT("sample-room-password")))
		&& TestTrue(TEXT("Password produces a 64-character salted BLAKE3 proof"), Digest.Len() == 64)
		&& TestEqual(TEXT("Digest is deterministic"), Digest,
			UGomokuGameInstance::BuildPasswordDigest(TEXT("sample-room-password"), TEXT("room-a")))
		&& TestNotEqual(TEXT("Different rooms produce different proofs"), Digest,
			UGomokuGameInstance::BuildPasswordDigest(TEXT("sample-room-password"), TEXT("room-b")))
		&& TestTrue(TEXT("Empty password has no digest"), UGomokuGameInstance::BuildPasswordDigest(TEXT("  ")).IsEmpty());
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGomokuStage10_OneHumanCanStartAndPlayAgainstItemUsingBot,
	TEXT("Gomoku.Stage10.OneHumanCanStartAndPlayAgainstItemUsingBot"),
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGomokuStage10_OneHumanCanStartAndPlayAgainstItemUsingBot::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Bot integration world created"), World))
	{
		return false;
	}

	AGomokuGameMode* GameMode = World->SpawnActor<AGomokuGameMode>();
	AGomokuGameState* GameState = World->SpawnActor<AGomokuGameState>();
	AGomokuPlayerController* Host = World->SpawnActor<AGomokuPlayerController>();
	AGomokuPlayerState* HostState = World->SpawnActor<AGomokuPlayerState>();
	if (!GameMode || !GameState || !Host || !HostState)
	{
		AddError(TEXT("Bot integration actors could not be created"));
		World->DestroyWorld(false);
		return false;
	}

	GameMode->GameState = GameState;
	GameMode->MaxLobbyPlayers = 2;
	GameMode->RequestedBotCount = 1;
	Host->PlayerState = HostState;
	GameState->PlayerArray.Add(HostState);
	HostState->SetIdentity(1, FLinearColor::Black);
	HostState->SetReady(true);

	if (!TestTrue(TEXT("One ready human plus one planned bot can start"), GameMode->AreAllPlayersReady())
		|| !TestTrue(TEXT("Host starts bot match"), GameMode->TryStartMatch(Host, TEXT(""))))
	{
		World->DestroyWorld(false);
		return false;
	}

	AGomokuPlayerState* BotState = nullptr;
	for (APlayerState* ExistingState : GameState->PlayerArray)
	{
		AGomokuPlayerState* GomokuState = Cast<AGomokuPlayerState>(ExistingState);
		if (GomokuState && GomokuState->bGomokuBot)
		{
			BotState = GomokuState;
			break;
		}
	}
	UGomokuRuleEngine* Engine = GameMode->GetRuleEngine();
	if (!TestNotNull(TEXT("Server bot PlayerState exists"), BotState)
		|| !TestNotNull(TEXT("Bot match rule engine exists"), Engine)
		|| !TestEqual(TEXT("Human and bot occupy two seats"), GameState->LocalPlayerCount, 2))
	{
		World->DestroyWorld(false);
		return false;
	}

	GameState->HandlePlaceStone(0, FIntPoint(0, 0));
	if (!TestEqual(TEXT("Bot becomes current after human move"), GameState->CurrentPlayerIndex, 1))
	{
		World->DestroyWorld(false);
		return false;
	}

	const int32 BotPlayerId = 2;
	Engine->AddPlayerEnergy(BotPlayerId, 5, UGomokuItemLibrary::MaxEnergy);
	const FGomokuPlayerStateData BeforeData = Engine->GetPlayerStateData(BotPlayerId);
	int32 AddedItemId = 1;
	while (AddedItemId <= 5 && BeforeData.ItemIds.Contains(AddedItemId))
	{
		++AddedItemId;
	}
	if (AddedItemId <= 5)
	{
		Engine->AddItemToInventory(BotPlayerId, AddedItemId);
	}
	Engine->ClearTurnItemLocksForPlayer(BotPlayerId);
	GameState->BotItemUseProbability = 1.0f;
	const bool bProcessedBot = GameState->TickBotTurn(true);
	const FGomokuPlayerStateData AfterData = Engine->GetPlayerStateData(BotPlayerId);

	int32 StoneCount = 0;
	const FGomokuMatchConfig Config = Engine->GetMatchConfig();
	for (int32 Y = 0; Y < Config.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Config.BoardSizeX; ++X)
		{
			const ECellState Cell = Engine->GetCellState(X, Y);
			StoneCount += Cell >= ECellState::Player1 && Cell <= ECellState::Player4 ? 1 : 0;
		}
	}

	const bool bValid = TestTrue(TEXT("Authoritative bot turn is processed"), bProcessedBot)
		&& TestTrue(TEXT("Bot probabilistically uses an item through GameState"), AfterData.bUsedItemThisTurn)
		&& TestTrue(TEXT("Bot also places a stone"), StoneCount >= 2)
		&& TestEqual(TEXT("Turn returns to the human"), GameState->CurrentPlayerIndex, 0);
	World->DestroyWorld(false);
	return bValid;
}

#endif // WITH_DEV_AUTOMATION_TESTS
