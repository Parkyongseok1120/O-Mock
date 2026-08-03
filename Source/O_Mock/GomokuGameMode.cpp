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

	int32 MaxPlayers = FMath::Clamp(DefaultHotseatPlayers, 2, 4);

	if (BoardTemplate)
	{
		// Use template-based configuration.
		ApplyBoardTemplate();
	}
	else
	{
		// Default 15x15 path when no template is set.
		FGomokuMatchConfig Config;
		Config.BoardSizeX = 15;
		Config.BoardSizeY = 15;
		Config.WinLength = 5;
		Config.MaxPlayers = MaxPlayers;

		RuleEngine->InitializeMatch(Config);

		if (AGomokuGameState* GS = GetGomokuGameState())
		{
			GS->SetRuleEngineRef(RuleEngine);
			GS->InitializeForLocalHotseat(MaxPlayers);
		}
	}

	FTransform BoardTransform(FVector::ZeroVector);
	BoardActor = GetWorld()->SpawnActor<AGomokuBoardActor>(
		AGomokuBoardActor::StaticClass(),
		BoardTransform
	);

	if (AGomokuBoardActor* Board = BoardActor.Get())
	{
		const FGomokuMatchConfig& Cfg = RuleEngine->GetMatchConfig();
		Board->ApplyBoardSize(Cfg.BoardSizeX, Cfg.BoardSizeY);
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
