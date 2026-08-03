// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "GomokuGameMode.h"
#include "GomokuPlayerController.h"
#include "Engine/Engine.h"

AGomokuHUD::AGomokuHUD()
{
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
}

void AGomokuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const FString TextToDraw = bShowGameOver ? GameOverText : CurrentTurnText;
	if (!TextToDraw.IsEmpty())
	{
		DrawText(TextToDraw, FLinearColor::White, 40.f, 65.f, Font, 1.2f, false);
	}
	if (bShowGameOver)
	{
		DrawText(TEXT("Press R to restart"), FLinearColor::Yellow, 40.f, 105.f, Font, 1.0f, false);
	}
	float Y = 140.f;
	for (int32 i = 0; i < PlayerTimeStrings.Num(); ++i)
	{
		if (!PlayerTimeStrings[i].IsEmpty())
		{
			DrawText(PlayerTimeStrings[i], FLinearColor::White, 40.f, Y, Font, 1.0f, false);
			Y += 22.f;
		}
	}

	// Item targeting / inventory HUD (Stage6 placeholder)
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
			DrawText(TEXT("ItemSlots: [placeholder]"), FLinearColor(0.5f, 0.5f, 0.7f), 40.f, Y, Font, 1.0f, false);
		}
	}
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

	const int32 Count = GomokuGameState->GetNumActivePlayers();
	if (Count <= 0 || PlayerIndex >= Count)
		return;

	PlayerTimeStrings.SetNumUninitialized(Count);

	FString Label = FString::Printf(TEXT("P%d: %.1fs"), PlayerIndex + 1, TimeState.PersonalRemaining);
	PlayerTimeStrings[PlayerIndex] = Label;
}
