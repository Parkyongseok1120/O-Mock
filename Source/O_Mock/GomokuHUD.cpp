// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "GomokuGameMode.h"
#include "GomokuHUDWidget.h"
#include "GomokuPlayerController.h"
#include "GomokuPlayerState.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuHUD, Log, All);

static FString GetGomokuPhaseLabel(EMatchPhase Phase)
{
	switch (Phase)
	{
	case EMatchPhase::Waiting: return TEXT("Waiting");
	case EMatchPhase::Playing: return TEXT("Playing");
	case EMatchPhase::MiniGameIntro: return TEXT("Mini-game intro");
	case EMatchPhase::MiniGamePlaying: return TEXT("Mini-game");
	case EMatchPhase::MiniGameResult: return TEXT("Mini-game result");
	case EMatchPhase::GameOver: return TEXT("Game over");
	default: return TEXT("Unknown");
	}
}

static FString GetGomokuItemLabel(int32 ItemId)
{
	switch (ItemId)
	{
	case 1: return TEXT("1:Seal(1)");
	case 2: return TEXT("2:Pull(1)");
	case 3: return TEXT("3:Steal(3)");
	case 4: return TEXT("4:Skip(3)");
	case 5: return TEXT("5:Guard(2)");
	default: return FString::Printf(TEXT("%d:?"), ItemId);
	}
}

AGomokuHUD::AGomokuHUD()
{
	HUDWidgetClass = UGomokuHUDWidget::StaticClass();
}

void AGomokuHUD::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		GomokuGameState = Cast<AGomokuGameState>(World->GetGameState());
		if (GomokuGameState)
		{
			GomokuGameState->OnTurnChanged.AddDynamic(this, &AGomokuHUD::HandleTurnChanged);
			GomokuGameState->OnMatchEnded.AddDynamic(this, &AGomokuHUD::HandleMatchEnded);
			GomokuGameState->OnMatchRestarted.AddDynamic(this, &AGomokuHUD::HandleMatchRestarted);
			GomokuGameState->OnTickPlayerTime.AddDynamic(this, &AGomokuHUD::OnTickPlayerTime);
		}
	}
	bShowGameOver = false;
	GameOverText = TEXT("");
	CurrentTurnText = TEXT("Player 1's Turn");
	PlayerTimeStrings.Reset();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && PlayerController->IsLocalController())
	{
		TSubclassOf<UGomokuHUDWidget> EffectiveWidgetClass = HUDWidgetClass;
		if (!EffectiveWidgetClass)
		{
			EffectiveWidgetClass = UGomokuHUDWidget::StaticClass();
		}
		HUDWidget = CreateWidget<UGomokuHUDWidget>(PlayerController, EffectiveWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(10);
			UE_LOG(LogGomokuHUD, Display, TEXT("Gomoku HUD widget ready: class=%s"), *HUDWidget->GetClass()->GetName());
		}
		else
		{
			UE_LOG(LogGomokuHUD, Warning, TEXT("Gomoku HUD widget creation failed; Canvas fallback remains active."));
		}
	}
}

void AGomokuHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GomokuGameState)
	{
		GomokuGameState->OnTurnChanged.RemoveDynamic(this, &AGomokuHUD::HandleTurnChanged);
		GomokuGameState->OnMatchEnded.RemoveDynamic(this, &AGomokuHUD::HandleMatchEnded);
		GomokuGameState->OnMatchRestarted.RemoveDynamic(this, &AGomokuHUD::HandleMatchRestarted);
		GomokuGameState->OnTickPlayerTime.RemoveDynamic(this, &AGomokuHUD::OnTickPlayerTime);
	}
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AGomokuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (HUDWidget && HUDWidget->IsInViewport()) return;
	if (!Canvas) return;
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	FString TextToDraw = bShowGameOver ? GameOverText : CurrentTurnText;
	if (GomokuGameState && GomokuGameState->MatchPhase == EMatchPhase::MiniGamePlaying)
	{
		TextToDraw = FString::Printf(TEXT("Mini-game P%d: complete five in a row (%.1fs)"),
			GomokuGameState->GetMiniGameInputPlayerIndex() + 1, GomokuGameState->MiniGameRemainingTime);
	}
	else if (GomokuGameState && GomokuGameState->MatchPhase == EMatchPhase::MiniGameResult)
	{
		TextToDraw = FString::Printf(TEXT("Mini-game result (resume in %.1fs)"), GomokuGameState->MiniGameResultRemainingTime);
	}
	else if (GomokuGameState && GomokuGameState->MatchPhase == EMatchPhase::Waiting)
	{
		TextToDraw = TEXT("Lobby: press T to ready, then host presses Enter to start");
	}
	if (!TextToDraw.IsEmpty())
	{
		DrawText(TextToDraw, FLinearColor::White, 40.f, 65.f, Font, 1.2f, false);
	}
	if (bShowGameOver)
	{
		DrawText(TEXT("Press R to restart"), FLinearColor::Yellow, 40.f, 105.f, Font, 1.0f, false);
	}
	float Y = 140.f;
	if (GomokuGameState)
	{
		const FString RoundText = FString::Printf(TEXT("Round %d | %s"),
			GomokuGameState->CurrentRoundIndex, *GetGomokuPhaseLabel(GomokuGameState->MatchPhase));
		DrawText(RoundText, FLinearColor(0.7f, 0.85f, 1.f), 40.f, Y, Font, 1.0f, false);
		Y += 22.f;
		if (GomokuGameState->MatchPhase == EMatchPhase::Waiting)
		{
			for (const APlayerState* ExistingState : GomokuGameState->PlayerArray)
			{
				const AGomokuPlayerState* LobbyPlayer = Cast<AGomokuPlayerState>(ExistingState);
				if (!LobbyPlayer)
				{
					continue;
				}
				const FString ReadyText = FString::Printf(TEXT("P%d: %s"), LobbyPlayer->GomokuPlayerId,
					LobbyPlayer->bReady ? TEXT("Ready") : TEXT("Not ready"));
				DrawText(ReadyText, LobbyPlayer->bReady ? FLinearColor::Green : FLinearColor::White,
					40.f, Y, Font, 1.0f, false);
				Y += 22.f;
			}
		}
		for (int32 PlayerIndex = 0; PlayerIndex < GomokuGameState->PlayerTimes.Num(); ++PlayerIndex)
		{
			const FGomokuPlayerTimeState& TimeState = GomokuGameState->PlayerTimes[PlayerIndex];
			const FString TimeText = FString::Printf(TEXT("P%d: %.1fs total | %.1fs turn"),
				PlayerIndex + 1, TimeState.PersonalRemaining,
				FMath::Max(0.0f, GomokuGameState->MaxTurnTime - TimeState.TurnElapsedThisTurn));
			const FLinearColor TimeColor = PlayerIndex == GomokuGameState->CurrentPlayerIndex
				? FLinearColor::Yellow : FLinearColor::White;
			DrawText(TimeText, TimeColor, 40.f, Y, Font, 1.0f, false);
			Y += 22.f;
		}
	}

	// Item targeting and the local player's inventory.
	if (AGomokuPlayerController* PC = Cast<AGomokuPlayerController>(GetOwningPlayerController()))
	{
		if (PC->bItemTargetingActive && PC->SelectedItemId > 0)
		{
			FString ItemText = FString::Printf(TEXT("Select target for Item %d"), PC->SelectedItemId);
			DrawText(ItemText, FLinearColor(1.f, 0.8f, 0.f), 40.f, Y, Font, 1.0f, false);
			Y += 22.f;
		}
		else
		{
			TArray<int32> ItemIds;
			int32 Energy = 0;
			if (GetNetMode() == NM_Standalone && GomokuGameState && GomokuGameState->GetRuleEngine()
				&& GomokuGameState->CurrentPlayerIndex >= 0)
			{
				const FGomokuPlayerStateData Data = GomokuGameState->GetRuleEngine()->GetPlayerStateData(
					GomokuGameState->CurrentPlayerIndex + 1);
				ItemIds = Data.ItemIds;
				Energy = Data.Energy;
			}
			else if (const AGomokuPlayerState* PlayerState = PC->GetPlayerState<AGomokuPlayerState>())
			{
				ItemIds = PlayerState->InventoryItemIds;
				Energy = PlayerState->Energy;
			}
			else if (GomokuGameState && GomokuGameState->GetRuleEngine() && GomokuGameState->CurrentPlayerIndex >= 0)
			{
				const FGomokuPlayerStateData Data = GomokuGameState->GetRuleEngine()->GetPlayerStateData(GomokuGameState->CurrentPlayerIndex + 1);
				ItemIds = Data.ItemIds;
				Energy = Data.Energy;
			}

			FString InventoryText = FString::Printf(TEXT("Energy: %d | Items:"), Energy);
			for (const int32 ItemId : ItemIds)
			{
				InventoryText += FString::Printf(TEXT(" [%s]"), *GetGomokuItemLabel(ItemId));
			}
			DrawText(InventoryText, FLinearColor(0.5f, 0.8f, 0.7f), 40.f, Y, Font, 1.0f, false);
			Y += 22.f;
		}
	}
	DrawText(TEXT("LMB: place/select | RMB drag: orbit | Wheel: zoom | F: reset view | R: restart"),
		FLinearColor(0.72f, 0.72f, 0.72f), 40.f, Y + 10.f, Font, 0.9f, false);
}

void AGomokuHUD::HandleTurnChanged(int32 PlayerIndex, int32 /*RoundIndex*/)
{
	bShowGameOver = false;
	GameOverText = TEXT("");

	if (PlayerIndex >= 0)
	{
		CurrentTurnText = FString::Printf(TEXT("Player %d's Turn"), PlayerIndex + 1);
	}
	else
	{
		CurrentTurnText = TEXT("Waiting...");
	}
}

void AGomokuHUD::HandleMatchEnded(const FGomokuWinResult& WinResult)
{
	bShowGameOver = true;

	if (WinResult.IsWin && WinResult.WinnerPlayerIndex != INDEX_NONE)
	{
		GameOverText = FString::Printf(TEXT("Game Over! Player %d Wins!"), WinResult.WinnerPlayerIndex + 1);
	}
	else
	{
		GameOverText = TEXT("Game Over! Draw!");
	}
}

void AGomokuHUD::HandleMatchRestarted()
{
	bShowGameOver = false;
	GameOverText = TEXT("");
	if (GomokuGameState && GomokuGameState->CurrentPlayerIndex >= 0)
	{
		CurrentTurnText = FString::Printf(TEXT("Player %d's Turn"), GomokuGameState->CurrentPlayerIndex + 1);
	}
	else
	{
		CurrentTurnText = TEXT("Game Restarted");
	}
}

void AGomokuHUD::HandleRestartKey()
{
	if (!GetWorld())
		return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
		return;

	AGomokuGameMode* GM = Cast<AGomokuGameMode>(GetWorld()->GetAuthGameMode());
	if (GM)
	{
		GM->RestartGame();
	}
}

void AGomokuHUD::OnTickPlayerTime(int32 PlayerIndex, const FGomokuPlayerTimeState& TimeState)
{
	if (PlayerIndex < 0 || !GomokuGameState)
		return;

	const int32 Count = GomokuGameState->PlayerTimes.Num();
	if (Count <= 0 || PlayerIndex >= Count)
		return;

	// FString owns heap state and must be constructed before assignment. Using
	// SetNumUninitialized here corrupted memory on the first player-time tick.
	PlayerTimeStrings.SetNum(Count);

	FString Label = FString::Printf(TEXT("P%d: %.1fs"), PlayerIndex + 1, TimeState.PersonalRemaining);
	PlayerTimeStrings[PlayerIndex] = Label;
}
