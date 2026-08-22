// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameState.h"
#include "GomokuMatchEventLog.h"
#include "GomokuItemLibrary.h"
#include "GomokuPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AGomokuGameState::AGomokuGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; // Multiplayer-ready: server-authoritative state replication.
	MaxPersonalTime = 120.0f;
	MaxTurnTime = 25.0f;
	PersonalRecoveryRate = 0.35f;
}

void AGomokuGameState::InitializeForLocalHotseat(int32 MaxPlayers)
{
	if (!HasAuthority()) return;

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
	bMiniGameActive = false;
	MiniGameRemainingTime = 0.0f;
	MiniGameResultRemainingTime = 0.0f;
	MiniGameAnswerCell = FIntPoint(-1, -1);
	MiniGamePuzzleCells.Reset();
	MiniGameSubmittedPlayerIndices.Reset();
	MiniGameCorrectPlayerIndices.Reset();
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

	SyncReplicatedBoard();
	StartNewTurn();
}

void AGomokuGameState::SetRuleEngineRef(UGomokuRuleEngine* InRuleEngine)
{
	if (!HasAuthority()) return;
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
			const int32 FirstCandidate = FMath::RandRange(1, 5);
			for (int32 Offset = 0; Offset < 5; ++Offset)
			{
				const int32 NewItemId = ((FirstCandidate - 1 + Offset) % 5) + 1;
				if (RuleEngine->AddItemToInventory(NewPlayerId, NewItemId))
				{
					RuleEngine->MarkItemGainedThisTurn(NewPlayerId, NewItemId);
					break;
				}
			}
		}

		RuleEngine->AddPlayerEnergy(NewPlayerId, 1, UGomokuItemLibrary::MaxEnergy);
	}
	SyncPlayerStates();

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

	if (bHasTimeSystem && CurrentPlayerIndex >= 0 && CurrentPlayerIndex < PlayerTimes.Num())
	{
		FGomokuPlayerTimeState& TS = PlayerTimes[CurrentPlayerIndex];
		TS.TurnElapsedThisTurn = 0.0f;
	}

	if (!bSkipCompletionTracking)
	{
		PlayersCompletedThisRound.Add(CurrentPlayerIndex);
	}
	RoundTurnCount++;

	const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
	int32 SkippedPlayerForNextRound = INDEX_NONE;
	if (!bSkipCompletionTracking && ActiveIndices.Num() > 1)
	{
		const int32 CurrentActivePosition = ActiveIndices.IndexOfByKey(CurrentPlayerIndex);
		if (CurrentActivePosition != INDEX_NONE)
		{
			const int32 Direction = RuleEngine->TurnDirection >= 0 ? 1 : -1;
			const int32 NextActivePosition = (CurrentActivePosition + Direction + ActiveIndices.Num()) % ActiveIndices.Num();
			const int32 NextPlayerIndex = ActiveIndices[NextActivePosition];
			if (RuleEngine->GetPlayerStateData(NextPlayerIndex + 1).bSkipNextTurn)
			{
				if (PlayersCompletedThisRound.Contains(NextPlayerIndex))
				{
					SkippedPlayerForNextRound = NextPlayerIndex;
				}
				else
				{
					PlayersCompletedThisRound.Add(NextPlayerIndex);
				}
			}
		}
	}
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
		RuleEngine->ExpireRoundEffects(CurrentRoundIndex);
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
		if (SkippedPlayerForNextRound != INDEX_NONE)
		{
			PlayersCompletedThisRound.Add(SkippedPlayerForNextRound);
		}
		RoundTurnCount = 0;
		CurrentRoundIndex++;

		if (CurrentRoundIndex > 1 && ((CurrentRoundIndex - 1) % 5) == 0 && !RuleEngine->IsGameOver())
		{
			StartMiniGame();
			return;
		}
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
	SyncPlayerStates();
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

void AGomokuGameState::HandleUseItem(int32 PlayerIndex, int32 ItemId, const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!HasAuthority() || !IsGameActive || !RuleEngine.IsValid() || MatchPhase != EMatchPhase::Playing)
	{
		return;
	}
	if (PlayerIndex != CurrentPlayerIndex || ItemId <= 0)
	{
		return;
	}

	const int32 PlayerId = PlayerIndex + 1;
	if (ItemId == 4 && TargetPlayerIndex < 0)
	{
		const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
		const int32 CurrentActivePosition = ActiveIndices.IndexOfByKey(PlayerIndex);
		if (CurrentActivePosition != INDEX_NONE && ActiveIndices.Num() > 1)
		{
			const int32 Direction = RuleEngine->TurnDirection >= 0 ? 1 : -1;
			TargetPlayerIndex = (CurrentActivePosition + Direction + ActiveIndices.Num()) % ActiveIndices.Num();
		}
	}
	if (!UGomokuItemLibrary::CanUseItem(RuleEngine.Get(), PlayerId, ItemId) ||
		!UGomokuItemLibrary::ValidateTargetForPlayer(RuleEngine.Get(), PlayerId, ItemId, TargetCell, TargetPlayerIndex))
	{
		return;
	}

	int32 TargetPlayerId = INDEX_NONE;
	const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
	if (ActiveIndices.IsValidIndex(TargetPlayerIndex))
	{
		TargetPlayerId = ActiveIndices[TargetPlayerIndex] + 1;
	}

	// Reset the cached result so an item without win recheck cannot observe stale data.
	RuleEngine->SetLastItemWinResult(FGomokuWinResult());
	if (!UGomokuItemLibrary::ExecuteItem(RuleEngine.Get(), ItemId, PlayerId, TargetCell, TargetPlayerIndex, CurrentRoundIndex))
	{
		return;
	}

	SyncReplicatedBoard();
	SyncPlayerStates();
	if (EventLog)
	{
		EventLog->AppendItemEvent(EMatchEventType::ItemUsed, PlayerId, TargetPlayerId, TargetCell, ItemId, CurrentRoundIndex);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());

		if (ItemId == 4)
		{
			EventLog->AppendItemEvent(EMatchEventType::TurnSkipped, PlayerId, TargetPlayerId, TargetCell, ItemId, CurrentRoundIndex);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
	}

	const FGomokuWinResult ItemWin = RuleEngine->GetLastItemWinResult();
	if (ItemWin.IsWin)
	{
		IsGameActive = false;
		WinnerPlayerIndex = ItemWin.WinnerPlayerIndex;
		MatchPhase = EMatchPhase::GameOver;
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::PlayerWon, ItemWin.WinnerPlayerIndex + 1, INDEX_NONE,
				ItemWin.WinCell, NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
		OnMatchEnded.Broadcast(ItemWin);
	}
}

void AGomokuGameState::TickTimeSystem(float DeltaSeconds)
{
	if (!HasAuthority()) return;
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
	const float RequestedStep = FMath::Max(0.0f, DeltaSeconds);
	const float TurnCapacity = FMath::Max(0.0f, MaxTurnTime - TS.TurnElapsedThisTurn);
	const float AppliedStep = FMath::Min3(RequestedStep, TurnCapacity, TS.PersonalRemaining);
	TS.TurnElapsedThisTurn += AppliedStep;
	TS.PersonalRemaining = FMath::Max(0.0f, TS.PersonalRemaining - AppliedStep);

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
	if (!HasAuthority()) return;
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

void AGomokuGameState::RestartMatch()
{
	if (!HasAuthority()) return;
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
	bMiniGameActive = false;
	MiniGameRemainingTime = 0.0f;
	MiniGameResultRemainingTime = 0.0f;
	MiniGameAnswerCell = FIntPoint(-1, -1);
	MiniGamePuzzleCells.Reset();

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

void AGomokuGameState::RequestAbandonCurrentPlayer()
{
	RequestAbandonPlayer(CurrentPlayerIndex + 1);
}

void AGomokuGameState::RequestAbandonPlayer(int32 PlayerId)
{
	if (!HasAuthority() || !IsGameActive || !RuleEngine.IsValid() || !RuleEngine->IsPlayerActive(PlayerId))
	{
		return;
	}

	const int32 AbandoningPlayerIndex = PlayerId - 1;
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

	SyncPlayerStates();
	if (AbandoningPlayerIndex == CurrentPlayerIndex)
	{
		// Advance turn without treating abandon as a completed action.
		EndCurrentTurn(true, true);
	}
}

void AGomokuGameState::StartMiniGame()
{
	if (!HasAuthority() || !IsGameActive || !RuleEngine.IsValid() || MatchPhase != EMatchPhase::Playing)
	{
		return;
	}

	bMiniGameActive = true;
	MiniGameRemainingTime = 8.0f;
	MiniGameResultRemainingTime = 0.0f;
	MiniGameAnswerCell = FIntPoint(5, 3);
	MiniGamePuzzleCells.Init(ECellState::Empty, 49);
	MiniGamePuzzleCells[3 * 7] = ECellState::Blocked;
	for (int32 X = 1; X <= 4; ++X)
	{
		MiniGamePuzzleCells[3 * 7 + X] = ECellState::Player1;
	}
	MiniGamePuzzleCells[1 * 7 + 2] = ECellState::Player2;
	MiniGamePuzzleCells[2 * 7 + 2] = ECellState::Player2;
	MiniGamePuzzleCells[4 * 7 + 4] = ECellState::Player3;
	MiniGameSubmittedPlayerIndices.Reset();
	MiniGameCorrectPlayerIndices.Reset();
	MatchPhase = EMatchPhase::MiniGamePlaying;
	bTimePaused = true;

	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::MiniGameStarted, 0, CurrentRoundIndex, FIntPoint(-1, -1), NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}
	OnReplicatedBoardChanged.Broadcast();
}

bool AGomokuGameState::SubmitMiniGameAnswer(int32 PlayerIndex, const FIntPoint& AnswerCell)
{
	if (!HasAuthority() || !bMiniGameActive || MatchPhase != EMatchPhase::MiniGamePlaying ||
		!RuleEngine.IsValid() || !RuleEngine->GetActivePlayerIndices().Contains(PlayerIndex) ||
		MiniGameSubmittedPlayerIndices.Contains(PlayerIndex))
	{
		return false;
	}

	MiniGameSubmittedPlayerIndices.Add(PlayerIndex);
	if (AnswerCell == MiniGameAnswerCell)
	{
		const int32 CorrectRank = MiniGameCorrectPlayerIndices.Num();
		MiniGameCorrectPlayerIndices.Add(PlayerIndex);
		const int32 Reward = FMath::Max(1, 3 - CorrectRank);
		RuleEngine->AddPlayerEnergy(PlayerIndex + 1, Reward, UGomokuItemLibrary::MaxEnergy);
	}
	SyncPlayerStates();

	if (MiniGameSubmittedPlayerIndices.Num() >= RuleEngine->GetActivePlayerIndices().Num())
	{
		bMiniGameActive = false;
		MatchPhase = EMatchPhase::MiniGameResult;
		MiniGameRemainingTime = 0.0f;
		MiniGameResultRemainingTime = 3.0f;
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::MiniGameResult, PlayerIndex + 1, INDEX_NONE, AnswerCell, NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
	}

	return true;
}

void AGomokuGameState::ResumeFromMiniGame()
{
	if (!HasAuthority() || MatchPhase != EMatchPhase::MiniGameResult || !IsGameActive)
	{
		return;
	}

	bMiniGameActive = false;
	MiniGameRemainingTime = 0.0f;
	MiniGameAnswerCell = FIntPoint(-1, -1);
	MiniGamePuzzleCells.Reset();
	MiniGameResultRemainingTime = 0.0f;
	MatchPhase = EMatchPhase::Playing;
	bTimePaused = false;
	SyncReplicatedBoard();
	StartNewTurn();
}

void AGomokuGameState::SetHoveredCell(const FIntPoint& InCell)
{
	if (!HasAuthority()) return;
	HoveredCell = InCell;
}

void AGomokuGameState::ClearHoveredCell()
{
	if (!HasAuthority()) return;
	HoveredCell = FIntPoint(-1, -1);
}

void AGomokuGameState::SetTimePaused(bool bPaused)
{
	if (!HasAuthority()) return;
	bTimePaused = bPaused;
}

void AGomokuGameState::SetMatchPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority()) return;
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
	if (!HasAuthority()) return;
	if (!RuleEngine.IsValid())
	{
		return;
	}
	const FGomokuMatchConfig& Cfg = RuleEngine->GetMatchConfig();
	ReplicatedBoardSizeX = Cfg.BoardSizeX;
	ReplicatedBoardSizeY = Cfg.BoardSizeY;
	ReplicatedBoardCells.Reset();
	ReplicatedBoardCells.Reserve(Cfg.BoardSizeX * Cfg.BoardSizeY);
	for (int32 Y = 0; Y < Cfg.BoardSizeY; ++Y)
	{
		for (int32 X = 0; X < Cfg.BoardSizeX; ++X)
		{
			ReplicatedBoardCells.Add(RuleEngine->GetCellState(X, Y));
		}
	}
	OnReplicatedBoardChanged.Broadcast();
}

void AGomokuGameState::SyncPlayerStates()
{
	if (!HasAuthority() || !RuleEngine.IsValid())
	{
		return;
	}

	for (APlayerState* ExistingState : PlayerArray)
	{
		AGomokuPlayerState* PlayerState = Cast<AGomokuPlayerState>(ExistingState);
		if (!PlayerState || PlayerState->GomokuPlayerId <= 0)
		{
			continue;
		}

		const FGomokuPlayerStateData Data = RuleEngine->GetPlayerStateData(PlayerState->GomokuPlayerId);
		float RemainingTime = Data.RemainingTime;
		const int32 PlayerIndex = PlayerState->GomokuPlayerId - 1;
		if (PlayerTimes.IsValidIndex(PlayerIndex))
		{
			RemainingTime = PlayerTimes[PlayerIndex].PersonalRemaining;
		}
		PlayerState->SetPublicMatchState(RemainingTime, Data.Energy, PlayerState->PublicStatusEffects,
			Data.bHasAbandoned, Data.ItemIds);
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
	if (HasAuthority())
	{
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
}

void AGomokuGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) return;
	if (!IsGameActive)
	{
		return;
	}
	if (MatchPhase == EMatchPhase::MiniGamePlaying)
	{
		TickMiniGame(DeltaTime);
		return;
	}
	if (MatchPhase == EMatchPhase::MiniGameResult)
	{
		MiniGameResultRemainingTime = FMath::Max(0.0f, MiniGameResultRemainingTime - FMath::Max(0.0f, DeltaTime));
		if (MiniGameResultRemainingTime <= 0.0f)
		{
			ResumeFromMiniGame();
		}
		return;
	}
	TickTimeSystem(DeltaTime);
}

void AGomokuGameState::TickMiniGame(float DeltaSeconds)
{
	if (!HasAuthority() || !bMiniGameActive || MatchPhase != EMatchPhase::MiniGamePlaying)
	{
		return;
	}

	MiniGameRemainingTime = FMath::Max(0.0f, MiniGameRemainingTime - FMath::Max(0.0f, DeltaSeconds));
	if (MiniGameRemainingTime <= 0.0f)
	{
		bMiniGameActive = false;
		MatchPhase = EMatchPhase::MiniGameResult;
		MiniGameResultRemainingTime = 3.0f;
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::MiniGameResult, 0, INDEX_NONE, FIntPoint(-1, -1), NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
	}
}

void AGomokuGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGomokuGameState, LocalPlayerCount);
	DOREPLIFETIME(AGomokuGameState, CurrentRoundIndex);
	DOREPLIFETIME(AGomokuGameState, RoundTurnCount);
	DOREPLIFETIME(AGomokuGameState, CurrentPlayerIndex);
	DOREPLIFETIME(AGomokuGameState, IsGameActive);
	DOREPLIFETIME(AGomokuGameState, WinnerPlayerIndex);
	DOREPLIFETIME(AGomokuGameState, bHasTimeSystem);
	DOREPLIFETIME(AGomokuGameState, bTimePaused);
	DOREPLIFETIME(AGomokuGameState, PlayerTimes);
	DOREPLIFETIME(AGomokuGameState, PlayerColors);
	DOREPLIFETIME(AGomokuGameState, HoveredCell);
	DOREPLIFETIME(AGomokuGameState, MatchPhase);
	DOREPLIFETIME(AGomokuGameState, bMiniGameActive);
	DOREPLIFETIME(AGomokuGameState, MiniGameRemainingTime);
	DOREPLIFETIME(AGomokuGameState, MiniGamePuzzleCells);
	DOREPLIFETIME(AGomokuGameState, MiniGameResultRemainingTime);
	DOREPLIFETIME(AGomokuGameState, MiniGameSubmittedPlayerIndices);
	DOREPLIFETIME(AGomokuGameState, MiniGameCorrectPlayerIndices);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardSizeX);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardSizeY);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardCells);
}

void AGomokuGameState::OnRep_TurnState()
{
	OnTurnChanged.Broadcast(CurrentPlayerIndex, CurrentRoundIndex);
}

void AGomokuGameState::OnRep_PlayerTimes()
{
	if (PlayerTimes.IsValidIndex(CurrentPlayerIndex))
	{
		OnTickPlayerTime.Broadcast(CurrentPlayerIndex, PlayerTimes[CurrentPlayerIndex]);
	}
}

void AGomokuGameState::OnRep_IsGameActive(bool bPreviousValue)
{
	if (bPreviousValue == IsGameActive) { return; }

	if (IsGameActive)
	{
		OnMatchRestarted.Broadcast();
		return;
	}

	FGomokuWinResult Result;
	Result.IsWin = WinnerPlayerIndex != INDEX_NONE;
	Result.WinnerPlayerIndex = WinnerPlayerIndex;
	Result.WinCell = FIntPoint(-1, -1);
	OnMatchEnded.Broadcast(Result);
}

void AGomokuGameState::OnRep_ReplicatedBoardCells(const TArray<ECellState>& PreviousCells)
{
	auto IsStone = [](ECellState State) { return State >= ECellState::Player1 && State <= ECellState::Player4; };

	bool bHadPreviousStones = false;
	for (const auto& S : PreviousCells)
	{
		if (IsStone(S))
		{
			bHadPreviousStones = true;
			break;
		}
	}

	bool bHasCurrentStones = false;
	for (const auto& S : ReplicatedBoardCells)
	{
		if (IsStone(S))
		{
			bHasCurrentStones = true;
			break;
		}
	}

	if (ReplicatedBoardSizeX > 0)
	{
		for (int32 Index = 0; Index < ReplicatedBoardCells.Num(); ++Index)
		{
			const ECellState CurrentState = ReplicatedBoardCells[Index];
			if (!IsStone(CurrentState)) continue;

			bool bWasPreviouslyStone = PreviousCells.IsValidIndex(Index) && IsStone(PreviousCells[Index]);
			if (!bWasPreviouslyStone)
			{
				const FIntPoint Cell(Index % ReplicatedBoardSizeX, Index / ReplicatedBoardSizeX);
				OnStonePlaced.Broadcast(Cell);
			}
		}
	}

	if (bHadPreviousStones && !bHasCurrentStones)
	{
		OnMatchRestarted.Broadcast();
	}

	OnReplicatedBoardChanged.Broadcast();
}

void AGomokuGameState::OnRep_MatchPresentation()
{
	OnReplicatedBoardChanged.Broadcast();
}
