// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AGomokuGameState::AGomokuGameState()
{
	MaxPersonalTime = 120.0f;
	MaxTurnTime = 25.0f;
	PersonalRecoveryRate = 0.35f;
}

void AGomokuGameState::InitializeForLocalHotseat_Implementation(int32 MaxPlayers)
{
	LocalPlayerCount = MaxPlayers;
	CurrentRoundIndex = 0;
	IsGameActive = true;
	bTimePaused = false;
	HoveredCell = FIntPoint(-1, -1);

	// Initialize per-player time state
	PlayerTimes.Reset();
	for (int32 i = 0; i < LocalPlayerCount; ++i)
	{
		FGomokuPlayerTimeState TS;
		TS.PersonalRemaining = MaxPersonalTime;
		TS.TurnElapsedThisTurn = 0.0f;
		PlayerTimes.Add(TS);
	}

	StartNewTurn();
}

void AGomokuGameState::SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine)
{
	RuleEngine = InRuleEngine;
}

void AGomokuGameState::StartNewTurn()
{
	if (!IsGameActive || !RuleEngine.IsValid()) return;

	CurrentRoundIndex++;
	CurrentPlayerIndex = (CurrentRoundIndex - 1) % LocalPlayerCount;
	TurnStartTime = GetWorld()->GetTimeSeconds();
	RuleEngine->SetCurrentPlayerIndex(CurrentPlayerIndex);

	OnTurnChanged.Broadcast(CurrentPlayerIndex, CurrentRoundIndex);
}

void AGomokuGameState::EndCurrentTurn(bool bForceEnd)
{
	if (!IsGameActive || !RuleEngine.IsValid()) return;

	// Apply time accounting for current player
	float TurnDuration = GetWorld()->GetTimeSeconds() - TurnStartTime;
	if (bHasTimeSystem && CurrentPlayerIndex >= 0 && CurrentPlayerIndex < PlayerTimes.Num())
	{
		FGomokuPlayerTimeState& TS = PlayerTimes[CurrentPlayerIndex];

		// Clamp to max turn time if exceeded
		float EffectiveDuration = FMath::Min(TurnDuration, MaxTurnTime);
		TS.TurnElapsedThisTurn = 0.0f;
		TS.PersonalRemaining = FMath::Max(0.0f, TS.PersonalRemaining - EffectiveDuration);
	}

	// Round completion: if we cycled through all players once this round
	bool bRoundCompleted = (CurrentPlayerIndex == LocalPlayerCount - 1);

	if (bHasTimeSystem && bRoundCompleted)
	{
		ApplyEndOfRoundRecovery();
	}

	StartNewTurn();
}

void AGomokuGameState::HandlePlaceStone(int32 PlayerIndex, const FIntPoint& Cell)
{
	if (!IsGameActive || !RuleEngine.IsValid()) return;

	int32 PlayerId = 1 + PlayerIndex; // RuleEngine uses 1-based IDs.
	const bool bValid = RuleEngine->TryPlaceStone(PlayerId, Cell.X, Cell.Y);
	if (!bValid) return;

	// Broadcast placement for HUD / BoardActor to react
	OnStonePlaced.Broadcast(Cell);

	const FGomokuWinResult Win = RuleEngine->CheckWinAt(Cell);
	if (Win.IsWin)
	{
		IsGameActive = false;
		WinnerPlayerIndex = PlayerIndex;
		OnMatchEnded.Broadcast(Win);
		return;
	}

	if (RuleEngine->IsBoardFull())
	{
		IsGameActive = false;
		WinnerPlayerIndex = INDEX_NONE; // Draw
		FGomokuWinResult DrawResult;
		DrawResult.IsWin = false;
		OnMatchEnded.Broadcast(DrawResult);
		return;
	}

	EndCurrentTurn(false);
}

void AGomokuGameState::TickTimeSystem(float DeltaSeconds)
{
	if (!bHasTimeSystem || !IsGameActive || bTimePaused) return;
	if (CurrentPlayerIndex < 0 || CurrentPlayerIndex >= PlayerTimes.Num()) return;

	FGomokuPlayerTimeState& TS = PlayerTimes[CurrentPlayerIndex];
	float ElapsedSinceTurnStart = GetWorld()->GetTimeSeconds() - TurnStartTime;

	TS.TurnElapsedThisTurn = ElapsedSinceTurnStart;

	if (TS.PersonalRemaining <= 0.0f || TS.TurnElapsedThisTurn >= MaxTurnTime)
	{
		AutoMoveOnTimeout();
	}
	else
	{
		OnTickPlayerTime.Broadcast(CurrentPlayerIndex, TS);
	}
}

void AGomokuGameState::ApplyEndOfRoundRecovery()
{
	for (FGomokuPlayerTimeState& TS : PlayerTimes)
	{
		float Deficit = FMath::Max(0.0f, MaxPersonalTime - TS.PersonalRemaining);
		float Recovered = Deficit * PersonalRecoveryRate;
		TS.PersonalRemaining = FMath::Min(MaxPersonalTime, TS.PersonalRemaining + Recovered);
	}
}

void AGomokuGameState::AutoMoveOnTimeout()
{
	if (!IsGameActive || !RuleEngine.IsValid()) return;

	TArray<FIntPoint> Candidates;

	// Prefer last hovered cell if valid and empty
	if (HoveredCell.X >= 0 && HoveredCell.Y >= 0)
	{
		if (RuleEngine->IsValidEmpty(HoveredCell))
		{
			Candidates.Add(HoveredCell);
		}
	}

	if (Candidates.Num() == 0)
	{
		// Fallback: nearest empty cell to center
		const FGomokuMatchConfig& Config = RuleEngine->GetMatchConfig();
		FIntPoint Center((Config.BoardSizeX - 1) / 2, (Config.BoardSizeY - 1) / 2);

		TArray<FIntPoint> Ring;
		Ring.Add(Center);
		for (int32 r = 1; r < Config.BoardSizeX; ++r)
		{
			for (int32 x = FMath::Max(0, Center.X - r); x <= FMath::Min(Config.BoardSizeX - 1, Center.X + r); ++x)
			{
				for (int32 y = FMath::Max(0, Center.Y - r); y <= FMath::Min(Config.BoardSizeY - 1, Center.Y + r); ++y)
				{
					Ring.Add(FIntPoint(x, y));
				}
			}

			for (const FIntPoint& P : Ring)
			{
				if (RuleEngine->IsValidEmpty(P))
				{
					Candidates.Add(P);
					break;
				}
			}
			if (!Candidates.IsEmpty()) break;
		}
	}

	if (Candidates.Num() > 0)
	{
		const FIntPoint& Chosen = Candidates[0];
		HandlePlaceStone(CurrentPlayerIndex, Chosen);
	}
	else
	{
		EndCurrentTurn(true);
	}
}

void AGomokuGameState::RestartMatch_Implementation()
{
	if (!RuleEngine.IsValid()) return;

	RuleEngine->InitializeMatch(RuleEngine->GetMatchConfig());

	LocalPlayerCount = 2;
	CurrentRoundIndex = 0;
	IsGameActive = true;
	WinnerPlayerIndex = INDEX_NONE;
	bTimePaused = false;
	HoveredCell = FIntPoint(-1, -1);

	PlayerTimes.Reset();
	for (int32 i = 0; i < LocalPlayerCount; ++i)
	{
		FGomokuPlayerTimeState TS;
		TS.PersonalRemaining = MaxPersonalTime;
		TS.TurnElapsedThisTurn = 0.0f;
		PlayerTimes.Add(TS);
	}

	StartNewTurn();
	OnMatchRestarted.Broadcast();
}

void AGomokuGameState::SetHoveredCell(const FIntPoint& InCell)
{
	HoveredCell = InCell;
}

void AGomokuGameState::BeginPlay()
{
	Super::BeginPlay();
	// Ensure time system initialized if enabled
	if (bHasTimeSystem && PlayerTimes.Num() != LocalPlayerCount)
	{
		PlayerTimes.Reset();
		for (int32 i = 0; i < LocalPlayerCount; ++i)
		{
			FGomokuPlayerTimeState TS;
			TS.PersonalRemaining = MaxPersonalTime;
			TS.TurnElapsedThisTurn = 0.0f;
			PlayerTimes.Add(TS);
		}
	}
}

void AGomokuGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsGameActive) return;
	TickTimeSystem(DeltaTime);
}
