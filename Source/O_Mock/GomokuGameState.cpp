// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameState.h"
#include "GomokuMatchEventLog.h"
#include "GomokuItemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AGomokuGameState::AGomokuGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false; // Stage 4: local hotseat only.
	MaxPersonalTime = 120.0f;
	MaxTurnTime = 25.0f;
	PersonalRecoveryRate = 0.35f;
}

void AGomokuGameState::InitializeForLocalHotseat_Implementation(int32 MaxPlayers)
{
	if (!EventLog)
	{
		EventLog = NewObject<UGomokuMatchEventLog>(this, TEXT("MatchEventLog"));
	}
	LocalPlayerCount = FMath::Clamp(MaxPlayers, 2, 4);
	CurrentRoundIndex = 1;
	RoundTurnCount = 0;
	PlayersCompletedThisRound.Reset();
	IsGameActive = true;
	bTimePaused = false;
	MatchPhase = EMatchPhase::Playing;
	HoveredCell = FIntPoint(-1, -1);
	WinnerPlayerIndex = INDEX_NONE;
	CurrentPlayerIndex = -1;

	PlayerTimes.Reset();
	PlayerColors.Reset();
	static const FLinearColor Colors[] = {
		FLinearColor::Black,
		FLinearColor::White,
		FLinearColor(0.2f, 0.6f, 1.f),
		FLinearColor(0.9f, 0.2f, 0.2f)
	};
	for (int32 i = 0; i < LocalPlayerCount; ++i)
	{
		FGomokuPlayerTimeState TS;
		TS.PersonalRemaining = MaxPersonalTime;
		TS.TurnElapsedThisTurn = 0.0f;
		PlayerTimes.Add(TS);
		PlayerColors.Add(Colors[i % 4]);
	}

	if (EventLog)
	{
		EventLog->Clear();
	}

	if (RuleEngine.IsValid())
	{
		FGomokuMatchConfig Cfg = RuleEngine->GetMatchConfig();
		Cfg.MaxPlayers = LocalPlayerCount;
		RuleEngine->InitializeMatch(Cfg);
	}

	StartNewTurn();
}

void AGomokuGameState::SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine)
{
	RuleEngine = InRuleEngine;
}

void AGomokuGameState::StartNewTurn()
{
	if (!IsGameActive || !RuleEngine.IsValid())
		return;

	const int32 PreviousPlayerIndex = (CurrentPlayerIndex >= 0) ? CurrentPlayerIndex : INDEX_NONE;

	if (CurrentPlayerIndex < 0)
	{
		CurrentPlayerIndex = 0;
		RuleEngine->SetCurrentPlayerIndex(CurrentPlayerIndex);
	}
	else
	{
		RuleEngine->AdvanceTurn();
		CurrentPlayerIndex = RuleEngine->GetCurrentPlayerIndex();
	}

	TurnStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (CurrentPlayerIndex >= 0 && CurrentPlayerIndex < PlayerTimes.Num())
	{
		PlayerTimes[CurrentPlayerIndex].TurnElapsedThisTurn = 0.f;
	}

	const int32 NewPlayerId = CurrentPlayerIndex + 1;

	// For previous player: clear turn item locks (promote gained->usable).
	if (PreviousPlayerIndex >= 0)
	{
		const int32 PrevId = PreviousPlayerIndex + 1;
		RuleEngine->ClearTurnItemLocksForPlayer(PrevId);
	}

	// For new player: reset used-item flag and ensure inventory.
	RuleEngine->ResetUsedItemThisTurn(NewPlayerId);

	if (RuleEngine.IsValid())
	{
		const auto& PState = RuleEngine->GetPlayerStateData(NewPlayerId);
		if (PState.ItemIds.Num() < 2)
		{
			const int32 NewItemId = FMath::RandRange(1, 5);
			RuleEngine->AddItemToInventory(NewPlayerId, NewItemId);
			RuleEngine->MarkItemGainedThisTurn(NewPlayerId, NewItemId);
		}

		RuleEngine->AddPlayerEnergy(NewPlayerId, 1, UGomokuItemLibrary::MaxEnergy);
	}

	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::TurnStarted, NewPlayerId, INDEX_NONE, FIntPoint(-1, -1), NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}

	OnTurnChanged.Broadcast(CurrentPlayerIndex, CurrentRoundIndex);
}

void AGomokuGameState::EndCurrentTurn(bool bForceEnd, bool bSkipCompletionTracking)
{
	if (!IsGameActive || !RuleEngine.IsValid())
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : TurnStartTime;
	const float TurnDuration = Now - TurnStartTime;
	if (bHasTimeSystem && CurrentPlayerIndex >= 0 && CurrentPlayerIndex < PlayerTimes.Num())
	{
		FGomokuPlayerTimeState& TS = PlayerTimes[CurrentPlayerIndex];
		const float EffectiveDuration = FMath::Min(TurnDuration, MaxTurnTime);
		TS.TurnElapsedThisTurn = 0.0f;
		TS.PersonalRemaining = FMath::Max(0.0f, TS.PersonalRemaining - EffectiveDuration);
	}

	if (!bSkipCompletionTracking)
	{
		PlayersCompletedThisRound.Add(CurrentPlayerIndex);
	}
	RoundTurnCount++;

	const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
	bool bRoundCompleted = !bSkipCompletionTracking && !ActiveIndices.IsEmpty() &&
		[&]()
		{
			for (int32 Idx : ActiveIndices)
			{
				if (!PlayersCompletedThisRound.Contains(Idx))
					return false;
			}
			return true;
		}();

	if (bRoundCompleted)
	{
		if (bHasTimeSystem)
		{
			ApplyEndOfRoundRecovery();
		}
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::RoundEnded, CurrentPlayerIndex + 1, INDEX_NONE, FIntPoint(-1, -1), NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
		PlayersCompletedThisRound.Reset();
		RoundTurnCount = 0;
		CurrentRoundIndex++;
	}

	StartNewTurn();
}

void AGomokuGameState::HandlePlaceStone(int32 PlayerIndex, const FIntPoint& Cell)
{
	if (!HasAuthority())
	{
		return; // only authority handles stone placement (local hotseat or server).
	}
	if (!IsGameActive || !RuleEngine.IsValid() || MatchPhase != EMatchPhase::Playing)
	{
		return;
	}
	if (PlayerIndex != CurrentPlayerIndex)
	{
		return;
	}

	const int32 PlayerId = 1 + PlayerIndex;
	const bool bValid = RuleEngine->TryPlaceStone(PlayerId, Cell.X, Cell.Y);
	if (!bValid)
	{
		return;
	}

	SyncReplicatedBoard();
	OnStonePlaced.Broadcast(Cell);

	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::StonePlaced, PlayerId, INDEX_NONE, Cell, NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}

	const FGomokuWinResult Win = RuleEngine->CheckWinAt(Cell);
	if (Win.IsWin)
	{
		IsGameActive = false;
		WinnerPlayerIndex = PlayerIndex;
		MatchPhase = EMatchPhase::GameOver;
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::PlayerWon, PlayerId, INDEX_NONE, Cell, NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
		OnMatchEnded.Broadcast(Win);
		return;
	}

	if (RuleEngine->IsBoardFull())
	{
		IsGameActive = false;
		WinnerPlayerIndex = INDEX_NONE;
		MatchPhase = EMatchPhase::GameOver;
		FGomokuWinResult DrawResult;
		DrawResult.IsWin = false;
		OnMatchEnded.Broadcast(DrawResult);
		return;
	}

	EndCurrentTurn(false);
}

void AGomokuGameState::TickTimeSystem(float DeltaSeconds)
{
	if (!bHasTimeSystem || !IsGameActive || bTimePaused)
	{
		return;
	}
	if (MatchPhase != EMatchPhase::Playing)
	{
		return;
	}
	if (CurrentPlayerIndex < 0 || CurrentPlayerIndex >= PlayerTimes.Num())
	{
		return;
	}

	FGomokuPlayerTimeState& TS = PlayerTimes[CurrentPlayerIndex];
	const float ElapsedSinceTurnStart = (GetWorld() ? GetWorld()->GetTimeSeconds() : TurnStartTime) - TurnStartTime;
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
		const float Deficit = FMath::Max(0.0f, MaxPersonalTime - TS.PersonalRemaining);
		const float RecoveredTime = Deficit * PersonalRecoveryRate;
		TS.PersonalRemaining = FMath::Min(MaxPersonalTime, TS.PersonalRemaining + RecoveredTime);
	}
}

void AGomokuGameState::AutoMoveOnTimeout()
{
	if (!IsGameActive || !RuleEngine.IsValid())
	{
		return;
	}

	TArray<FIntPoint> Candidates;
	if (HoveredCell.X >= 0 && HoveredCell.Y >= 0 && RuleEngine->IsValidEmpty(HoveredCell))
	{
		Candidates.Add(HoveredCell);
	}

	if (Candidates.Num() == 0)
	{
		const FGomokuMatchConfig& Config = RuleEngine->GetMatchConfig();
		const FIntPoint Center((Config.BoardSizeX - 1) / 2, (Config.BoardSizeY - 1) / 2);
		for (int32 r = 0; r < FMath::Max(Config.BoardSizeX, Config.BoardSizeY); ++r)
		{
			for (int32 x = FMath::Max(0, Center.X - r); x <= FMath::Min(Config.BoardSizeX - 1, Center.X + r); ++x)
			{
				for (int32 y = FMath::Max(0, Center.Y - r); y <= FMath::Min(Config.BoardSizeY - 1, Center.Y + r); ++y)
				{
					const FIntPoint P(x, y);
					if (RuleEngine->IsValidEmpty(P))
					{
						Candidates.Add(P);
						break;
					}
				}
				if (Candidates.Num() > 0)
				{
					break;
				}
			}
			if (Candidates.Num() > 0)
			{
				break;
			}
		}
	}

	if (Candidates.Num() > 0)
	{
		HandlePlaceStone(CurrentPlayerIndex, Candidates[0]);
	}
	else
	{
		EndCurrentTurn(true);
	}
}

void AGomokuGameState::RestartMatch_Implementation()
{
	if (!RuleEngine.IsValid())
	{
		return;
	}

	RuleEngine->InitializeMatch(RuleEngine->GetMatchConfig());
	CurrentRoundIndex = 1;
	RoundTurnCount = 0;
	PlayersCompletedThisRound.Reset();
	IsGameActive = true;
	WinnerPlayerIndex = INDEX_NONE;
	bTimePaused = false;
	MatchPhase = EMatchPhase::Playing;
	HoveredCell = FIntPoint(-1, -1);
	CurrentPlayerIndex = -1;

	PlayerTimes.Reset();
	for (int32 i = 0; i < LocalPlayerCount; ++i)
	{
		FGomokuPlayerTimeState TS;
		TS.PersonalRemaining = MaxPersonalTime;
		TS.TurnElapsedThisTurn = 0.0f;
		PlayerTimes.Add(TS);
	}

	if (EventLog)
	{
		EventLog->Clear();
	}

	SyncReplicatedBoard();
	StartNewTurn();
	OnMatchRestarted.Broadcast();
}

void AGomokuGameState::RequestAbandonCurrentPlayer_Implementation()
{
	if (!IsGameActive || !RuleEngine.IsValid())
		return;

	const int32 PlayerId = CurrentPlayerIndex + 1; // RuleEngine uses 1-based IDs.
	const int32 Remaining = RuleEngine->AbandonPlayer(PlayerId);

	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::PlayerLeft, PlayerId, INDEX_NONE, FIntPoint(-1, -1), NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}

	// If only one active player remains after abandon, they win.
	if (Remaining <= 1)
	{
		const int32 SoleIndex = RuleEngine->GetSoleActivePlayerIndex();
		if (SoleIndex != INDEX_NONE)
		{
			WinnerPlayerIndex = SoleIndex;
			IsGameActive = false;
			MatchPhase = EMatchPhase::GameOver;

			FGomokuWinResult WinResult;
			WinResult.IsWin = true;
			WinResult.WinnerPlayerIndex = SoleIndex;
			OnMatchEnded.Broadcast(WinResult);
			return;
		}
	}

	// Purge any abandoned/non-active indices from round-completion tracking so they cannot falsely complete the round.
	if (RuleEngine.IsValid())
	{
		const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
		TArray<int32> ToRemove;
		for (int32 Idx : PlayersCompletedThisRound)
		{
			if (!ActiveIndices.Contains(Idx))
				ToRemove.Add(Idx);
		}
		for (int32 Idx : ToRemove)
			PlayersCompletedThisRound.Remove(Idx);
	}

	// Advance turn to next active player without treating abandon as a completed action.
	EndCurrentTurn(true, true); // bSkipCompletionTracking = true: do not add abandoned player to round completion set.
}

void AGomokuGameState::SetHoveredCell(const FIntPoint& InCell)
{
	HoveredCell = InCell;
}

void AGomokuGameState::ClearHoveredCell()
{
	HoveredCell = FIntPoint(-1, -1);
}

void AGomokuGameState::SetTimePaused(bool bPaused)
{
	bTimePaused = bPaused;
}

void AGomokuGameState::SetMatchPhase(EMatchPhase NewPhase)
{
	MatchPhase = NewPhase;
	if (NewPhase == EMatchPhase::MiniGameIntro
		|| NewPhase == EMatchPhase::MiniGamePlaying
		|| NewPhase == EMatchPhase::MiniGameResult)
	{
		bTimePaused = true;
	}
	else if (NewPhase == EMatchPhase::Playing)
	{
		bTimePaused = false;
	}
}

void AGomokuGameState::SyncReplicatedBoard()
{
	if (!RuleEngine.IsValid())
	{
		return;
	}
	const FGomokuMatchConfig& Cfg = RuleEngine->GetMatchConfig();
	ReplicatedBoardCells.Reset();
	ReplicatedBoardCells.Reserve(Cfg.BoardSizeX * Cfg.BoardSizeY);
	for (int32 Y = 0; Y < Cfg.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Cfg.BoardSizeX; ++X)
		{
			ReplicatedBoardCells.Add(RuleEngine->GetCellState(X, Y));
		}
	}
}

int32 AGomokuGameState::GetNextMinigameRound() const
{
	constexpr int32 Interval = 5;
	const int32 Next = ((CurrentRoundIndex / Interval) + 1) * Interval;
	return Next;
}

int32 AGomokuGameState::GetNumActivePlayers() const
{
	if (RuleEngine.IsValid())
	{
		return RuleEngine->GetActivePlayerIndices().Num();
	}
	return LocalPlayerCount;
}

void AGomokuGameState::BeginPlay()
{
	Super::BeginPlay();
	if (!EventLog)
	{
		EventLog = NewObject<UGomokuMatchEventLog>(this, TEXT("MatchEventLog"));
	}
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
	if (!IsGameActive)
	{
		return;
	}
	TickTimeSystem(DeltaTime);
}
