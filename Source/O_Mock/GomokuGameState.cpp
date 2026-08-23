// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameState.h"
#include "GomokuMatchEventLog.h"
#include "GomokuItemLibrary.h"
#include "GomokuBotLibrary.h"
#include "GomokuPlayerState.h"
#include "GomokuBalanceStatistics.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuMatchFlow, Log, All);

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
	MiniGameInputPlayerIndex = INDEX_NONE;
	CurrentPlayerIndex = -1;
	ScheduledBotPlayerIndex = INDEX_NONE;
	BotTurnReadyWorldSeconds = 0.0;
	BotRandomStream.Initialize(FMath::Rand());

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
	BeginBalanceStatistics();
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
	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::TurnStarted, NewPlayerId,
			CurrentRoundIndex, FIntPoint(-1, -1), NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}

	// For previous player: clear turn item locks (promote gained->usable).
	if (PreviousPlayerIndex >= 0)
	{
		const int32 PrevId = PreviousPlayerIndex + 1;
		RuleEngine->ClearTurnItemLocksForPlayer(PrevId);
	}

	// For new player: reset used-item flag and ensure inventory.
	RuleEngine->ResetUsedItemThisTurn(NewPlayerId);

	if (RuleEngine.IsValid() && bItemsEnabled)
	{
		FGomokuPlayerStateData PState = RuleEngine->GetPlayerStateData(NewPlayerId);
		int32 GrantedItemId = 0;
		if (PState.PendingInventoryItemId > 0 && PState.ItemIds.Num() < 2)
		{
			GrantedItemId = PState.PendingInventoryItemId;
			RuleEngine->ClaimPendingInventoryItemIntoFreeSlot(NewPlayerId);
		}
		else if (PState.PendingInventoryItemId <= 0)
		{
			const int32 FirstCandidate = FMath::RandRange(1, 5);
			for (int32 Offset = 0; Offset < 5; ++Offset)
			{
				const int32 NewItemId = ((FirstCandidate - 1 + Offset) % 5) + 1;
				if (PState.ItemIds.Num() < 2 && RuleEngine->AddItemToInventory(NewPlayerId, NewItemId))
				{
					RuleEngine->MarkItemGainedThisTurn(NewPlayerId, NewItemId);
					GrantedItemId = NewItemId;
					break;
				}
				if (PState.ItemIds.Num() >= 2 && RuleEngine->SetPendingInventoryItem(NewPlayerId, NewItemId))
				{
					break;
				}
			}
		}
		if (GrantedItemId > 0 && EventLog)
		{
			EventLog->AppendItemEvent(EMatchEventType::ItemGranted, NewPlayerId, INDEX_NONE,
				FIntPoint(-1, -1), GrantedItemId, CurrentRoundIndex);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
		if (GrantedItemId > 0)
		{
			if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
			{
				Balance->RecordItemAcquired(GrantedItemId);
			}
		}

		RuleEngine->AddPlayerEnergy(NewPlayerId, 1, UGomokuItemLibrary::MaxEnergy);
	}
	SyncPlayerStates();

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
		// Temporary blocks and guardian barriers can expire without another move.
		// Refresh their replicated presentation immediately at the round boundary.
		SyncReplicatedBoard();
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

		if (bMiniGameEnabled && CurrentRoundIndex > 1 && ((CurrentRoundIndex - 1) % 5) == 0 && !RuleEngine->IsGameOver())
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
	// A full-inventory offer is a server-authoritative modal decision.  Do not let
	// board input (including a forged placement RPC) consume the turn before the
	// player has selected which existing item to discard.
	if (RuleEngine->GetPlayerStateData(PlayerId).PendingInventoryItemId > 0)
	{
		return;
	}
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
		FinalizeBalanceStatistics(PlayerIndex);
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
		FinalizeBalanceStatistics(INDEX_NONE);
		return;
	}

	EndCurrentTurn(false);
}

bool AGomokuGameState::HandleUseItem(int32 PlayerIndex, int32 ItemId, const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!HasAuthority() || !bItemsEnabled || !IsGameActive || !RuleEngine.IsValid() || MatchPhase != EMatchPhase::Playing)
	{
		return false;
	}
	if (PlayerIndex != CurrentPlayerIndex || ItemId <= 0)
	{
		return false;
	}

	const int32 PlayerId = PlayerIndex + 1;
	if (RuleEngine->GetPlayerStateData(PlayerId).PendingInventoryItemId > 0)
	{
		return false;
	}
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
		return false;
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
		return false;
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
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->RecordItemUsed(ItemId);
		if (ItemId == 4)
		{
			Balance->RecordSkipUse();
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
		FinalizeBalanceStatistics(ItemWin.WinnerPlayerIndex);
	}

	UE_LOG(LogGomokuMatchFlow, Display,
		TEXT("Item used: player=%d item=%d target=(%d,%d) targetPlayer=%d energy=%d inventory=%d"),
		PlayerId, ItemId, TargetCell.X, TargetCell.Y, TargetPlayerId,
		RuleEngine->GetPlayerStateData(PlayerId).Energy,
		RuleEngine->GetPlayerStateData(PlayerId).ItemIds.Num());
	return true;
}

bool AGomokuGameState::HandleReplacePendingInventoryItem(int32 PlayerIndex, int32 DiscardItemId)
{
	if (!HasAuthority() || !bItemsEnabled || !IsGameActive || !RuleEngine.IsValid()
		|| MatchPhase != EMatchPhase::Playing || PlayerIndex != CurrentPlayerIndex || DiscardItemId <= 0)
	{
		return false;
	}
	const int32 PlayerId = PlayerIndex + 1;
	const int32 PendingItemId = RuleEngine->GetPlayerStateData(PlayerId).PendingInventoryItemId;
	if (PendingItemId <= 0 || !RuleEngine->ReplaceInventoryItemWithPending(PlayerId, DiscardItemId))
	{
		return false;
	}
	SyncPlayerStates();
	if (EventLog)
	{
		EventLog->AppendItemEvent(EMatchEventType::ItemDiscarded, PlayerId, INDEX_NONE,
			FIntPoint(-1, -1), DiscardItemId, CurrentRoundIndex);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		EventLog->AppendItemEvent(EMatchEventType::ItemGranted, PlayerId, INDEX_NONE,
			FIntPoint(-1, -1), PendingItemId, CurrentRoundIndex);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->RecordItemAcquired(PendingItemId);
	}
	return true;
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
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->RecordTimeout();
	}
	if (EventLog)
	{
		EventLog->AppendEvent(EMatchEventType::TurnTimedOut, CurrentPlayerIndex + 1,
			CurrentRoundIndex, FIntPoint(-1, -1), NAME_None);
		OnMatchEvent.Broadcast(EventLog->GetLastEvent());
	}

	// A player normally chooses the discarded slot.  On timeout, resolve the
	// modal offer deterministically so an unattended client cannot stall the
	// match forever, then continue with the normal server auto-move.
	const int32 TimedOutPlayerId = CurrentPlayerIndex + 1;
	const FGomokuPlayerStateData TimedOutPlayer = RuleEngine->GetPlayerStateData(TimedOutPlayerId);
	if (TimedOutPlayer.PendingInventoryItemId > 0)
	{
		if (TimedOutPlayer.ItemIds.IsEmpty()
			|| !HandleReplacePendingInventoryItem(CurrentPlayerIndex, TimedOutPlayer.ItemIds[0]))
		{
			EndCurrentTurn(true);
			return;
		}
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
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->CancelActiveMatch();
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
	MiniGameSubmittedPlayerIndices.Reset();
	MiniGameCorrectPlayerIndices.Reset();
	MiniGameInputPlayerIndex = INDEX_NONE;

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
	BeginBalanceStatistics();
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
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->RecordAbandon();
	}

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
			FinalizeBalanceStatistics(SoleIndex);
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
	if (!HasAuthority() || !bMiniGameEnabled || !IsGameActive || !RuleEngine.IsValid() || MatchPhase != EMatchPhase::Playing)
	{
		return;
	}

	bMiniGameActive = true;
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->RecordMiniGameStarted();
	}
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
	MiniGameInputPlayerIndex = FindNextMiniGameInputPlayer(INDEX_NONE);
	MatchPhase = EMatchPhase::MiniGamePlaying;
	bTimePaused = true;
	HoveredCell = FIntPoint(-1, -1);

	UE_LOG(LogGomokuMatchFlow, Display,
		TEXT("Mini-game started after round %d; first hotseat input player=%d"),
		CurrentRoundIndex - 1, MiniGameInputPlayerIndex + 1);

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
	if (GetNetMode() == NM_Standalone && PlayerIndex != MiniGameInputPlayerIndex)
	{
		return false;
	}

	MiniGameSubmittedPlayerIndices.Add(PlayerIndex);
	const bool bCorrect = AnswerCell == MiniGameAnswerCell;
	if (bCorrect)
	{
		const int32 CorrectRank = MiniGameCorrectPlayerIndices.Num();
		MiniGameCorrectPlayerIndices.Add(PlayerIndex);
		const int32 Reward = FMath::Max(1, 3 - CorrectRank);
		RuleEngine->AddPlayerEnergy(PlayerIndex + 1, Reward, UGomokuItemLibrary::MaxEnergy);
		if (CorrectRank == 0)
		{
			if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
			{
				Balance->RecordMiniGameWinner(PlayerIndex + 1);
			}
		}
	}
	SyncPlayerStates();

	if (MiniGameSubmittedPlayerIndices.Num() >= RuleEngine->GetActivePlayerIndices().Num())
	{
		bMiniGameActive = false;
		MiniGameInputPlayerIndex = INDEX_NONE;
		MatchPhase = EMatchPhase::MiniGameResult;
		MiniGameRemainingTime = 0.0f;
		MiniGameResultRemainingTime = 3.0f;
		if (EventLog)
		{
			EventLog->AppendEvent(EMatchEventType::MiniGameResult, PlayerIndex + 1, INDEX_NONE, AnswerCell, NAME_None);
			OnMatchEvent.Broadcast(EventLog->GetLastEvent());
		}
	}
	else
	{
		MiniGameInputPlayerIndex = FindNextMiniGameInputPlayer(PlayerIndex);
	}

	UE_LOG(LogGomokuMatchFlow, Display,
		TEXT("Mini-game answer: player=%d cell=(%d,%d) correct=%s submitted=%d/%d next=%d"),
		PlayerIndex + 1, AnswerCell.X, AnswerCell.Y, bCorrect ? TEXT("true") : TEXT("false"),
		MiniGameSubmittedPlayerIndices.Num(), RuleEngine->GetActivePlayerIndices().Num(),
		MiniGameInputPlayerIndex == INDEX_NONE ? 0 : MiniGameInputPlayerIndex + 1);

	return true;
}

int32 AGomokuGameState::FindNextMiniGameInputPlayer(int32 AfterPlayerIndex) const
{
	if (!RuleEngine.IsValid())
	{
		return INDEX_NONE;
	}

	const TArray<int32>& ActiveIndices = RuleEngine->GetActivePlayerIndices();
	if (ActiveIndices.IsEmpty())
	{
		return INDEX_NONE;
	}

	const int32 StartPosition = FMath::Max(0, ActiveIndices.IndexOfByKey(AfterPlayerIndex) + 1);
	for (int32 Offset = 0; Offset < ActiveIndices.Num(); ++Offset)
	{
		const int32 Candidate = ActiveIndices[(StartPosition + Offset) % ActiveIndices.Num()];
		if (!MiniGameSubmittedPlayerIndices.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return INDEX_NONE;
}

void AGomokuGameState::ResumeFromMiniGame()
{
	if (!HasAuthority() || MatchPhase != EMatchPhase::MiniGameResult || !IsGameActive)
	{
		return;
	}

	bMiniGameActive = false;
	MiniGameInputPlayerIndex = INDEX_NONE;
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
	ReplicatedGuardianCells = RuleEngine->GetGuardianProtectedCells();
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
		TArray<int32> LockedItemIds = Data.ItemIdsGainedThisTurn.Array();
		LockedItemIds.Sort();
		PlayerState->SetPublicMatchState(RemainingTime, Data.Energy, PlayerState->PublicStatusEffects,
			Data.bHasAbandoned, Data.ItemIds, LockedItemIds, Data.bUsedItemThisTurn, Data.PendingInventoryItemId);
	}
}

const AGomokuPlayerState* AGomokuGameState::FindGomokuPlayerState(int32 PlayerId) const
{
	for (APlayerState* ExistingState : PlayerArray)
	{
		const AGomokuPlayerState* PlayerState = Cast<AGomokuPlayerState>(ExistingState);
		if (PlayerState && PlayerState->GomokuPlayerId == PlayerId)
		{
			return PlayerState;
		}
	}
	return nullptr;
}

bool AGomokuGameState::TickBotTurn(bool bIgnoreThinkDelay)
{
	if (!HasAuthority() || !IsGameActive || MatchPhase != EMatchPhase::Playing
		|| !RuleEngine.IsValid() || CurrentPlayerIndex < 0)
	{
		ScheduledBotPlayerIndex = INDEX_NONE;
		return false;
	}

	const int32 PlayerId = CurrentPlayerIndex + 1;
	const AGomokuPlayerState* PlayerState = FindGomokuPlayerState(PlayerId);
	if (!PlayerState || !PlayerState->bGomokuBot)
	{
		ScheduledBotPlayerIndex = INDEX_NONE;
		return false;
	}

	const double WorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (ScheduledBotPlayerIndex != CurrentPlayerIndex)
	{
		ScheduledBotPlayerIndex = CurrentPlayerIndex;
		BotTurnReadyWorldSeconds = WorldSeconds + FMath::Clamp(BotThinkDelaySeconds, 0.1f, 5.0f);
		if (!bIgnoreThinkDelay)
		{
			return true;
		}
	}
	if (!bIgnoreThinkDelay && WorldSeconds < BotTurnReadyWorldSeconds)
	{
		return true;
	}

	// Clear before acting. A successful placement advances to another seat; a failed
	// choice is scheduled again instead of executing every frame.
	ScheduledBotPlayerIndex = INDEX_NONE;
	FGomokuPlayerStateData BotData = RuleEngine->GetPlayerStateData(PlayerId);
	if (BotData.PendingInventoryItemId > 0)
	{
		// Preserve the tactically stronger IDs when possible. This is deterministic so
		// a full inventory can never leave a server-controlled seat modal-blocked.
		static const int32 DiscardPriority[] = { 1, 5, 4, 2, 3 };
		int32 DiscardItemId = 0;
		for (const int32 Candidate : DiscardPriority)
		{
			if (BotData.ItemIds.Contains(Candidate))
			{
				DiscardItemId = Candidate;
				break;
			}
		}
		if (DiscardItemId <= 0 && !BotData.ItemIds.IsEmpty())
		{
			DiscardItemId = BotData.ItemIds[0];
		}
		if (DiscardItemId > 0)
		{
			HandleReplacePendingInventoryItem(CurrentPlayerIndex, DiscardItemId);
		}
	}

	if (bItemsEnabled && IsGameActive)
	{
		FGomokuBotItemAction ItemAction;
		if (UGomokuBotLibrary::ChooseItemAction(RuleEngine.Get(), PlayerId,
			BotItemUseProbability, BotRandomStream, ItemAction))
		{
			HandleUseItem(CurrentPlayerIndex, ItemAction.ItemId,
				ItemAction.TargetCell, ItemAction.TargetPlayerIndex);
		}
	}
	if (!IsGameActive || MatchPhase != EMatchPhase::Playing)
	{
		return true;
	}

	FIntPoint Move(-1, -1);
	if (UGomokuBotLibrary::ChooseMove(RuleEngine.Get(), PlayerId, Move))
	{
		HandlePlaceStone(CurrentPlayerIndex, Move);
	}
	return true;
}

int32 AGomokuGameState::GetNextMinigameRound() const
{
	constexpr int32 Interval = 5;
	const int32 CompletedRounds = FMath::Max(0, CurrentRoundIndex - 1);
	const int32 Next = ((CompletedRounds / Interval) + 1) * Interval;
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

UGomokuBalanceStatisticsSubsystem* AGomokuGameState::GetBalanceStatistics() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGomokuBalanceStatisticsSubsystem>() : nullptr;
}

void AGomokuGameState::BeginBalanceStatistics()
{
	if (!HasAuthority() || !RuleEngine.IsValid())
	{
		return;
	}
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->BeginMatch(RuleEngine->GetMatchConfig(), bItemsEnabled, bMiniGameEnabled);
	}
}

void AGomokuGameState::FinalizeBalanceStatistics(int32 WinnerIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	if (UGomokuBalanceStatisticsSubsystem* Balance = GetBalanceStatistics())
	{
		Balance->CompleteMatch(WinnerIndex, FMath::Max(0, CurrentRoundIndex - 1));
	}
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
	// Editor focus changes and shader work can produce multi-second frame hitches.
	// Advancing a turn or an eight-second mini-game by that entire hitch makes the
	// UI appear to skip states, so live simulation consumes a bounded frame step.
	constexpr float MaxLiveSimulationStep = 0.25f;
	const float SimulationStep = FMath::Clamp(DeltaTime, 0.0f, MaxLiveSimulationStep);
	if (MatchPhase == EMatchPhase::MiniGamePlaying)
	{
		TickMiniGame(SimulationStep);
		return;
	}
	if (MatchPhase == EMatchPhase::MiniGameResult)
	{
		MiniGameResultRemainingTime = FMath::Max(0.0f, MiniGameResultRemainingTime - SimulationStep);
		if (MiniGameResultRemainingTime <= 0.0f)
		{
			ResumeFromMiniGame();
		}
		return;
	}
	if (TickBotTurn())
	{
		return;
	}
	TickTimeSystem(SimulationStep);
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
		MiniGameInputPlayerIndex = INDEX_NONE;
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
	DOREPLIFETIME(AGomokuGameState, LobbyMaxPlayers);
	DOREPLIFETIME(AGomokuGameState, LobbyBotCount);
	DOREPLIFETIME(AGomokuGameState, LobbyBoardSize);
	DOREPLIFETIME(AGomokuGameState, bItemsEnabled);
	DOREPLIFETIME(AGomokuGameState, bMiniGameEnabled);
	DOREPLIFETIME(AGomokuGameState, bLobbyPasswordProtected);
	DOREPLIFETIME(AGomokuGameState, MaxPersonalTime);
	DOREPLIFETIME(AGomokuGameState, MaxTurnTime);
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
	DOREPLIFETIME(AGomokuGameState, MiniGameInputPlayerIndex);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardSizeX);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardSizeY);
	DOREPLIFETIME(AGomokuGameState, ReplicatedBoardCells);
	DOREPLIFETIME(AGomokuGameState, ReplicatedGuardianCells);
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

void AGomokuGameState::OnRep_GuardianCells()
{
	OnReplicatedBoardChanged.Broadcast();
}

void AGomokuGameState::OnRep_MatchPresentation()
{
	OnReplicatedBoardChanged.Broadcast();
}
