// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameMode.h"
#include "GomokuRuleEngine.h"
#include "GomokuGameState.h"
#include "GomokuBoardActor.h"
#include "GomokuPlayerController.h"
#include "GomokuHUD.h"
#include "GomokuPlayerState.h"
#include "GomokuGameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuGameMode, Log, All);

static void ApplyPlayerCountBoardPolicy(FGomokuMatchConfig& Config)
{
	// The first playable-build contract uses a 21x21 board for four players.
	// Larger explicit templates remain intact; smaller templates are expanded.
	if (Config.MaxPlayers == 4)
	{
		Config.BoardSizeX = FMath::Max(Config.BoardSizeX, 21);
		Config.BoardSizeY = FMath::Max(Config.BoardSizeY, 21);
	}
}

AGomokuGameMode::AGomokuGameMode()
{
	RuleEngine = CreateDefaultSubobject<UGomokuRuleEngine>(TEXT("RuleEngine"));

	GameStateClass = AGomokuGameState::StaticClass();
	PlayerControllerClass = AGomokuPlayerController::StaticClass();
	PlayerStateClass = AGomokuPlayerState::StaticClass();
	HUDClass = AGomokuHUD::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = false;
}

void AGomokuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString MaxPlayersOption = UGameplayStatics::ParseOption(Options, TEXT("MaxPlayers"));
	if (!MaxPlayersOption.IsEmpty())
	{
		MaxLobbyPlayers = FMath::Clamp(FCString::Atoi(*MaxPlayersOption), 2, 4);
	}
	const FString BoardSizeOption = UGameplayStatics::ParseOption(Options, TEXT("BoardSize"));
	const FString BotCountOption = UGameplayStatics::ParseOption(Options, TEXT("BotCount"));
	const FString PersonalTimeOption = UGameplayStatics::ParseOption(Options, TEXT("PersonalTime"));
	const FString TurnTimeOption = UGameplayStatics::ParseOption(Options, TEXT("TurnTime"));
	const FString ItemsOption = UGameplayStatics::ParseOption(Options, TEXT("Items"));
	const FString MiniGameOption = UGameplayStatics::ParseOption(Options, TEXT("MiniGame"));
	const FString ProtectedOption = UGameplayStatics::ParseOption(Options, TEXT("PasswordProtected"));
	ExpectedRoomPasswordHash = UGameplayStatics::ParseOption(Options, TEXT("RoomPasswordHash"));
	RequestedBoardSize = FMath::Clamp(BoardSizeOption.IsEmpty() ? 15 : FCString::Atoi(*BoardSizeOption), 15, 23);
	RequestedBotCount = FMath::Clamp(BotCountOption.IsEmpty() ? 0 : FCString::Atoi(*BotCountOption), 0, MaxLobbyPlayers - 1);
	RequestedPersonalTimeSeconds = FMath::Clamp(PersonalTimeOption.IsEmpty() ? 120 : FCString::Atoi(*PersonalTimeOption), 30, 600);
	RequestedTurnTimeSeconds = FMath::Clamp(TurnTimeOption.IsEmpty() ? 25 : FCString::Atoi(*TurnTimeOption), 5, 120);
	bRequestedItemsEnabled = ItemsOption.IsEmpty() || FCString::Atoi(*ItemsOption) != 0;
	bRequestedMiniGameEnabled = MiniGameOption.IsEmpty() || FCString::Atoi(*MiniGameOption) != 0;
	bPasswordProtected = (!ProtectedOption.IsEmpty() && FCString::Atoi(*ProtectedOption) != 0)
		|| !ExpectedRoomPasswordHash.IsEmpty();
	UE_LOG(LogGomokuGameMode, Display,
		TEXT("Lobby configured: max=%d bots=%d board=%d personal=%d turn=%d items=%d mini=%d locked=%d"),
		MaxLobbyPlayers, RequestedBotCount, RequestedBoardSize, RequestedPersonalTimeSeconds, RequestedTurnTimeSeconds,
		bRequestedItemsEnabled ? 1 : 0, bRequestedMiniGameEnabled ? 1 : 0, bPasswordProtected ? 1 : 0);
}

void AGomokuGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty() || !bPasswordProtected)
	{
		return;
	}
	const FString SuppliedHash = UGameplayStatics::ParseOption(Options, TEXT("RoomPasswordHash"));
	if (ExpectedRoomPasswordHash.IsEmpty() || !SuppliedHash.Equals(ExpectedRoomPasswordHash, ESearchCase::CaseSensitive))
	{
		ErrorMessage = TEXT("ROOM_PASSWORD_INVALID");
		UE_LOG(LogGomokuGameMode, Warning, TEXT("Rejected LAN login from %s: invalid room password"), *Address);
	}
}

void AGomokuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!RuleEngine)
	{
		return;
	}

	// Standalone runs directly into the local hotseat prototype. A listen server
	// starts in the waiting room and is initialized only after all players ready up.
	if (World->GetNetMode() == NM_Standalone)
	{
		InitializeMatchFromSettings();
	}
	else if (AGomokuGameState* GS = GetGomokuGameState())
	{
		GS->SetRuleEngineRef(RuleEngine);
		GS->IsGameActive = false;
		GS->CurrentPlayerIndex = -1;
		GS->MatchPhase = EMatchPhase::Waiting;
		GS->LocalPlayerCount = 0;
		GS->LobbyMaxPlayers = FMath::Clamp(MaxLobbyPlayers, 2, 4);
		GS->LobbyBotCount = RequestedBotCount;
		GS->LobbyBoardSize = RequestedBoardSize;
		GS->MaxPersonalTime = static_cast<float>(RequestedPersonalTimeSeconds);
		GS->MaxTurnTime = static_cast<float>(RequestedTurnTimeSeconds);
		GS->bItemsEnabled = bRequestedItemsEnabled;
		GS->bMiniGameEnabled = bRequestedMiniGameEnabled;
		GS->bLobbyPasswordProtected = bPasswordProtected;
		GS->PlayerTimes.Reset();
		bMatchStarted = false;
	}

	EnsureBoardActor();
	ConfigureBoardActor();
	UE_LOG(LogGomokuGameMode, Display, TEXT("Gomoku runtime ready: map=%s mode=%s board=%s active=%s"),
		*World->GetMapName(), World->GetNetMode() == NM_Standalone ? TEXT("Standalone") : TEXT("Network"),
		*GetNameSafe(BoardActor.Get()), GetGomokuGameState() && GetGomokuGameState()->IsGameActive ? TEXT("true") : TEXT("false"));
}

void AGomokuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// This board game is driven directly by PlayerController input and a board camera.
	// Deliberately skip pawn spawning so an empty editor map needs no PlayerStart actor.
}

AActor* AGomokuGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// No pawn is spawned, but returning a stable start spot keeps the engine's
	// generic login path valid on intentionally empty prototype maps.
	return this;
}

AGomokuGameState* AGomokuGameMode::GetGomokuGameState() const
{
	return Cast<AGomokuGameState>(GameState);
}

void AGomokuGameMode::RestartGame()
{
	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		GS->RestartMatch();
	}
}

void AGomokuGameMode::TravelToMatch(const FString& MapName)
{
	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(MapName);
	}
}

void AGomokuGameMode::ApplyBoardTemplate()
{
	if (!BoardTemplate)
	{
		return;
	}

	int32 MaxPlayers = FMath::Clamp(DefaultHotseatPlayers, 2, 4);

	FGomokuMatchConfig Config;
	Config.BoardSizeX = BoardTemplate->Width;
	Config.BoardSizeY = BoardTemplate->Height;
	Config.WinLength = 5;
	Config.MaxPlayers = MaxPlayers;
	Config.BlockedCells = BoardTemplate->BlockedCells;
	ApplyPlayerCountBoardPolicy(Config);

	RuleEngine->InitializeMatch(Config);

	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		GS->SetRuleEngineRef(RuleEngine);
		GS->InitializeForLocalHotseat(MaxPlayers);
	}

	if (AGomokuBoardActor* Board = GetBoardActor())
	{
		Board->ApplyBoardSize(Config.BoardSizeX, Config.BoardSizeY);
		Board->FitCameraToBoard();
	}
}

bool AGomokuGameMode::BuildMatchConfig(FGomokuMatchConfig& OutConfig) const
{
	int32 MaxPlayers = FMath::Clamp(DefaultHotseatPlayers, 2, 4);

	OutConfig.BoardSizeX = FMath::Clamp(RequestedBoardSize, 15, 23);
	OutConfig.BoardSizeY = FMath::Clamp(RequestedBoardSize, 15, 23);
	OutConfig.WinLength = 5;
	OutConfig.MaxPlayers = MaxPlayers;
	OutConfig.TurnTimeLimit = static_cast<float>(RequestedTurnTimeSeconds);
	OutConfig.BlockedCells.Reset();

	if (BoardTemplate)
	{
		if (!BoardTemplate->IsValid())
		{
			return false;
		}

		// A selected room template owns dimensions. Reuse blocked cells only when the editor asset matches it.
		if (BoardTemplate->Width == OutConfig.BoardSizeX && BoardTemplate->Height == OutConfig.BoardSizeY)
		{
			OutConfig.BlockedCells = BoardTemplate->BlockedCells;
		}
	}

	ApplyPlayerCountBoardPolicy(OutConfig);

	return true;
}

void AGomokuGameMode::InitializeMatchFromSettings()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!RuleEngine)
	{
		return;
	}

	FGomokuMatchConfig Config;
	if (!BuildMatchConfig(Config))
	{
		return;
	}

	RuleEngine->InitializeMatch(Config);

	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		GS->LobbyMaxPlayers = FMath::Clamp(MaxLobbyPlayers, 2, 4);
		GS->LobbyBotCount = RequestedBotCount;
		GS->LobbyBoardSize = Config.BoardSizeX;
		GS->MaxPersonalTime = static_cast<float>(RequestedPersonalTimeSeconds);
		GS->MaxTurnTime = static_cast<float>(RequestedTurnTimeSeconds);
		GS->bItemsEnabled = bRequestedItemsEnabled;
		GS->bMiniGameEnabled = bRequestedMiniGameEnabled;
		GS->bLobbyPasswordProtected = bPasswordProtected;
		GS->SetRuleEngineRef(RuleEngine);
		GS->InitializeForLocalHotseat(Config.MaxPlayers);
	}
}

void AGomokuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}
	if (bMatchStarted)
	{
		UE_LOG(LogGomokuGameMode, Warning, TEXT("Network player rejected: match already started"));
		NewPlayer->Destroy();
		return;
	}

	AGomokuPlayerState* NewPlayerState = NewPlayer->GetPlayerState<AGomokuPlayerState>();
	if (!NewPlayerState)
	{
		return;
	}

	// Assign the first free 1-based Gomoku ID. PlayerArray is authoritative on the server.
	TSet<int32> UsedIds;
	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		for (APlayerState* ExistingState : GS->PlayerArray)
		{
			if (const AGomokuPlayerState* GomokuState = Cast<AGomokuPlayerState>(ExistingState))
			{
				UsedIds.Add(GomokuState->GomokuPlayerId);
			}
		}
	}

	int32 AssignedId = 0;
	for (int32 CandidateId = 1; CandidateId <= FMath::Clamp(MaxLobbyPlayers, 2, 4); ++CandidateId)
	{
		if (!UsedIds.Contains(CandidateId))
		{
			AssignedId = CandidateId;
			break;
		}
	}

	if (AssignedId == 0)
	{
		UE_LOG(LogGomokuGameMode, Warning, TEXT("Network player rejected: lobby is full (%d players max)"),
			FMath::Clamp(MaxLobbyPlayers, 2, 4));
		NewPlayer->Destroy();
		return;
	}

	static const FLinearColor Colors[] = {
		FLinearColor::Black,
		FLinearColor::White,
		FLinearColor(0.2f, 0.6f, 1.f),
		FLinearColor(0.9f, 0.2f, 0.2f)
	};
	NewPlayerState->SetIdentity(AssignedId, Colors[(AssignedId - 1) % UE_ARRAY_COUNT(Colors)]);
	if (GetNetMode() != NM_Standalone)
	{
		UE_LOG(LogGomokuGameMode, Display, TEXT("Network player joined: id=%d controller=%s players=%d"),
			AssignedId, *GetNameSafe(NewPlayer), GameState ? GameState->PlayerArray.Num() : 0);
	}
}

void AGomokuGameMode::Logout(AController* Exiting)
{
	int32 DepartingPlayerId = 0;
	if (Exiting)
	{
		if (const AGomokuPlayerState* DepartingState = Exiting->GetPlayerState<AGomokuPlayerState>())
		{
			DepartingPlayerId = DepartingState->GomokuPlayerId;
		}
	}

	if (HasAuthority() && bMatchStarted && DepartingPlayerId > 0)
	{
		if (AGomokuGameState* GS = GetGomokuGameState())
		{
			GS->RequestAbandonPlayer(DepartingPlayerId);
		}
	}

	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		if (GetNetMode() != NM_Standalone)
		{
			UE_LOG(LogGomokuGameMode, Display, TEXT("Network player left: id=%d remaining=%d"),
				DepartingPlayerId, GS->PlayerArray.Num());
		}
		if (GS->PlayerArray.Num() == 0)
		{
			bMatchStarted = false;
		}
	}
}

bool AGomokuGameMode::SetPlayerReady(APlayerController* Player, bool bReady)
{
	if (!HasAuthority() || bMatchStarted || !Player)
	{
		return false;
	}

	AGomokuPlayerState* PlayerState = Player->GetPlayerState<AGomokuPlayerState>();
	if (!PlayerState)
	{
		return false;
	}

	PlayerState->SetReady(bReady);
	return true;
}

bool AGomokuGameMode::AreAllPlayersReady() const
{
	if (!HasAuthority() || !GameState)
	{
		return false;
	}

	const int32 PlayerCount = GameState->PlayerArray.Num();
	const int32 AvailableBotSeats = FMath::Min(RequestedBotCount,
		FMath::Max(0, FMath::Clamp(MaxLobbyPlayers, 2, 4) - PlayerCount));
	if (PlayerCount < 1 || PlayerCount + AvailableBotSeats < 2
		|| PlayerCount > FMath::Clamp(MaxLobbyPlayers, 2, 4))
	{
		return false;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AGomokuPlayerState* GomokuState = Cast<AGomokuPlayerState>(PlayerState);
		if (!GomokuState || !GomokuState->bReady)
		{
			return false;
		}
	}

	return true;
}

int32 AGomokuGameMode::SpawnRequestedBots()
{
	if (!HasAuthority() || !GameState || !GetWorld() || RequestedBotCount <= 0)
	{
		return 0;
	}

	TSet<int32> UsedIds;
	for (APlayerState* ExistingState : GameState->PlayerArray)
	{
		if (const AGomokuPlayerState* GomokuState = Cast<AGomokuPlayerState>(ExistingState))
		{
			UsedIds.Add(GomokuState->GomokuPlayerId);
		}
	}

	static const FLinearColor Colors[] = {
		FLinearColor::Black,
		FLinearColor::White,
		FLinearColor(0.2f, 0.6f, 1.f),
		FLinearColor(0.9f, 0.2f, 0.2f)
	};
	const int32 Capacity = FMath::Clamp(MaxLobbyPlayers, 2, 4);
	const int32 BotsToSpawn = FMath::Min(RequestedBotCount, FMath::Max(0, Capacity - GameState->PlayerArray.Num()));
	int32 SpawnedBots = 0;
	for (int32 BotIndex = 0; BotIndex < BotsToSpawn; ++BotIndex)
	{
		int32 AssignedId = 0;
		for (int32 CandidateId = 1; CandidateId <= Capacity; ++CandidateId)
		{
			if (!UsedIds.Contains(CandidateId))
			{
				AssignedId = CandidateId;
				break;
			}
		}
		if (AssignedId <= 0)
		{
			break;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* EffectivePlayerStateClass = PlayerStateClass
			? PlayerStateClass.Get()
			: AGomokuPlayerState::StaticClass();
		AGomokuPlayerState* BotState = GetWorld()->SpawnActor<AGomokuPlayerState>(
			EffectivePlayerStateClass, FTransform::Identity, SpawnParameters);
		if (!BotState)
		{
			continue;
		}

		BotState->SetIdentity(AssignedId, Colors[(AssignedId - 1) % UE_ARRAY_COUNT(Colors)]);
		BotState->SetGomokuBot(true);
		BotState->SetReady(true);
		BotState->SetPlayerName(FString::Printf(TEXT("BOT %d"), AssignedId));
		if (!GameState->PlayerArray.Contains(BotState))
		{
			GameState->AddPlayerState(BotState);
		}
		BotState->ForceNetUpdate();
		UsedIds.Add(AssignedId);
		++SpawnedBots;
	}

	UE_LOG(LogGomokuGameMode, Display, TEXT("Spawned %d/%d planned bots for %d total participants"),
		SpawnedBots, RequestedBotCount, GameState->PlayerArray.Num());
	return SpawnedBots;
}

bool AGomokuGameMode::TryStartMatch(APlayerController* RequestingPlayer, const FString& MapName)
{
	if (!HasAuthority() || bMatchStarted || !RequestingPlayer || !AreAllPlayersReady())
	{
		return false;
	}

	if (GameState && GameState->PlayerArray.Num() > 0 &&
		RequestingPlayer->PlayerState != GameState->PlayerArray[0])
	{
		return false;
	}

	SpawnRequestedBots();
	DefaultHotseatPlayers = FMath::Clamp(GameState->PlayerArray.Num(), 2, 4);
	InitializeMatchFromSettings();
	bMatchStarted = true;
	if (UGomokuGameInstance* GameInstance = GetGameInstance<UGomokuGameInstance>())
	{
		GameInstance->SetLanRoomJoinable(false);
	}
	UE_LOG(LogGomokuGameMode, Display, TEXT("Network lobby match started: host=%s players=%d map=%s"),
		*GetNameSafe(RequestingPlayer), DefaultHotseatPlayers, *MapName);

	EnsureBoardActor();
	ConfigureBoardActor();

	if (!MapName.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			// The prototype uses one map for lobby and match. Avoid a self-travel
			// that would reset the ready state before a persistent lobby exists.
			const FString CurrentMapName = World->GetMapName();
			if (!MapName.Contains(CurrentMapName))
			{
				TravelToMatch(MapName);
			}
		}
	}

	return true;
}

AGomokuBoardActor* AGomokuGameMode::EnsureBoardActor()
{
	if (BoardActor.IsValid())
	{
		return BoardActor.Get();
	}

	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return nullptr;
	}

	for (TActorIterator<AGomokuBoardActor> It(World); It; ++It)
	{
		BoardActor = *It;
		return BoardActor.Get();
	}

	BoardActor = World->SpawnActor<AGomokuBoardActor>(AGomokuBoardActor::StaticClass(), FTransform::Identity);
	return BoardActor.Get();
}

void AGomokuGameMode::ConfigureBoardActor()
{
	AGomokuBoardActor* Board = EnsureBoardActor();
	if (!Board)
	{
		return;
	}

	FGomokuMatchConfig Config;
	if (BuildMatchConfig(Config))
	{
		Board->ApplyBoardSize(Config.BoardSizeX, Config.BoardSizeY);
		Board->FitCameraToBoard();
	}
}
