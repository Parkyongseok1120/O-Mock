// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameMode.h"
#include "GomokuRuleEngine.h"
#include "GomokuGameState.h"
#include "GomokuBoardActor.h"

AGomokuGameMode::AGomokuGameMode()
{
	RuleEngine = NewObject<UGomokuRuleEngine>(this);
}

void AGomokuGameMode::BeginPlay()
{
	Super::BeginPlay();

	FGomokuMatchConfig Config;
	Config.BoardSizeX = 15;
	Config.BoardSizeY = 15;
	Config.WinLength = 5;
	Config.MaxPlayers = 2;
	RuleEngine->InitializeMatch(Config);

	if (AGomokuGameState* GS = GetGomokuGameState())
	{
		GS->SetRuleEngineRef(RuleEngine);
		GS->InitializeForLocalHotseat(2);
	}

	FTransform BoardTransform(FVector::ZeroVector);
	BoardActor = GetWorld()->SpawnActor<AGomokuBoardActor>(
		AGomokuBoardActor::StaticClass(),
		BoardTransform
	);
}

AGomokuGameState* AGomokuGameMode::GetGomokuGameState() const
{
	return Cast<AGomokuGameState>(GameState);
}

void AGomokuGameMode::RestartGame()
{
	if (AGomokuGameState* GS = GetGomokuGameState())
		GS->RestartMatch();

	RuleEngine->InitializeMatch(RuleEngine->GetMatchConfig());

	if (AGomokuBoardActor* Board = GetBoardActor())
		Board->ClearStones();
}
