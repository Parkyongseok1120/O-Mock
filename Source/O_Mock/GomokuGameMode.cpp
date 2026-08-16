// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameMode.h"
#include "GomokuRuleEngine.h"
#include "GomokuGameState.h"
#include "GomokuBoardActor.h"
#include "GomokuPlayerController.h"
#include "GomokuHUD.h"
#include "GomokuPlayerState.h"
#include "GameFramework/DefaultPawn.h"

AGomokuGameMode::AGomokuGameMode()
{
	RuleEngine = CreateDefaultSubobject<UGomokuRuleEngine>(TEXT("RuleEngine"));

	GameStateClass = AGomokuGameState::StaticClass();
	PlayerControllerClass = AGomokuPlayerController::StaticClass();
	PlayerStateClass = AGomokuPlayerState::StaticClass();
	HUDClass = AGomokuHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
	bUseSeamlessTravel = false;
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
		GS->PlayerTimes.Reset();
		bMatchStarted = false;
		return;
	}

	if (!BoardActor.IsValid())
	{
		FTransform BoardTransform(FVector::ZeroVector);
		BoardActor = World->SpawnActor<AGomokuBoardActor>(
			AGomokuBoardActor::StaticClass(),
			BoardTransform
		);
	}

	FGomokuMatchConfig Config;
	if (BuildMatchConfig(Config) && BoardActor.IsValid())
	{
		BoardActor->ApplyBoardSize(Config.BoardSizeX, Config.BoardSizeY);
		BoardActor->FitCameraToBoard();
	}
}

AGomokuGameState* AGomokuGameMode::GetGomokuGameState() const
{
	return Cast<AGomokuGameState>(GameState);
}

void AGomokuGameMode::RestartGame()
{
	if (AGomokuGameState* GS = GetGomokuGameState())
		GS->RestartMatch();

	FGomokuMatchConfig Config;

	if (BoardTemplate)
	{
		int32 MaxPlayers = FMath::Clamp(DefaultHotseatPlayers, 2, 4);
		Config.BoardSizeX = BoardTemplate->Width;
		Config.BoardSizeY = BoardTemplate->Height;
		Config.WinLength = 5;
		Config.MaxPlayers = MaxPlayers;
		Config.BlockedCells = BoardTemplate->BlockedCells;
	}
	else
	{
		Config = RuleEngine->GetMatchConfig();
	}

	RuleEngine->InitializeMatch(Config);

	if (AGomokuBoardActor* Board = GetBoardActor())
		Board->ClearStones();
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

	OutConfig.BoardSizeX = 15;
	OutConfig.BoardSizeY = 15;
	OutConfig.WinLength = 5;
	OutConfig.MaxPlayers = MaxPlayers;
	OutConfig.BlockedCells.Reset();

	if (BoardTemplate)
	{
		if (!BoardTemplate->IsValid())
		{
			return false;
		}

		OutConfig.BoardSizeX = BoardTemplate->Width;
		OutConfig.BoardSizeY = BoardTemplate->Height;
		OutConfig.BlockedCells = BoardTemplate->BlockedCells;
	}

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
}

void AGomokuGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

	if (AGomokuGameState* GS = GetGomokuGameState())
	{
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
	if (PlayerCount < 2 || PlayerCount > FMath::Clamp(MaxLobbyPlayers, 2, 4))
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

	DefaultHotseatPlayers = FMath::Clamp(GameState->PlayerArray.Num(), 2, 4);
	InitializeMatchFromSettings();
	bMatchStarted = true;

	if (UWorld* World = GetWorld())
	{
		if (!BoardActor.IsValid())
		{
			BoardActor = World->SpawnActor<AGomokuBoardActor>(
				AGomokuBoardActor::StaticClass(), FTransform(FVector::ZeroVector));
		}

		FGomokuMatchConfig Config;
		if (BuildMatchConfig(Config) && BoardActor.IsValid())
		{
			BoardActor->ApplyBoardSize(Config.BoardSizeX, Config.BoardSizeY);
			BoardActor->FitCameraToBoard();
		}
	}

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
