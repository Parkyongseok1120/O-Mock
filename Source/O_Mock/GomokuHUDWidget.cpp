// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuHUDWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GomokuGameState.h"
#include "GomokuItemLibrary.h"
#include "GomokuPlayerController.h"
#include "GomokuPlayerState.h"
#include "GomokuRuleEngine.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuHUDWidget, Log, All);

static FString GetGomokuWidgetPhaseLabel(EMatchPhase Phase)
{
	switch (Phase)
	{
	case EMatchPhase::Waiting: return TEXT("LOBBY");
	case EMatchPhase::Playing: return TEXT("MATCH");
	case EMatchPhase::MiniGameIntro: return TEXT("MINI-GAME INCOMING");
	case EMatchPhase::MiniGamePlaying: return TEXT("MINI-GAME");
	case EMatchPhase::MiniGameResult: return TEXT("MINI-GAME RESULT");
	case EMatchPhase::GameOver: return TEXT("GAME OVER");
	default: return TEXT("OMOCK");
	}
}

static FString GetGomokuWidgetHeadline(const AGomokuGameState* GameState)
{
	if (!GameState)
	{
		return TEXT("LOADING MATCH");
	}

	switch (GameState->MatchPhase)
	{
	case EMatchPhase::Waiting:
		return TEXT("PRESS T TO READY  ·  HOST ENTER TO START");
	case EMatchPhase::MiniGameIntro:
		return TEXT("PREPARE FOR THE SEVEN-BY-SEVEN CHALLENGE");
	case EMatchPhase::MiniGamePlaying:
		return GameState->GetMiniGameInputPlayerIndex() != INDEX_NONE
			? FString::Printf(TEXT("PLAYER %d  ·  COMPLETE FIVE IN A ROW  ·  %.1fs"),
				GameState->GetMiniGameInputPlayerIndex() + 1, GameState->MiniGameRemainingTime)
			: FString::Printf(TEXT("COMPLETE FIVE IN A ROW  ·  %.1fs"), GameState->MiniGameRemainingTime);
	case EMatchPhase::MiniGameResult:
		return FString::Printf(TEXT("RESULT LOCKED  ·  RESUME IN %.1fs"), GameState->MiniGameResultRemainingTime);
	case EMatchPhase::GameOver:
		return GameState->WinnerPlayerIndex != INDEX_NONE
			? FString::Printf(TEXT("PLAYER %d WINS"), GameState->WinnerPlayerIndex + 1)
			: TEXT("DRAW");
	default:
		return GameState->CurrentPlayerIndex >= 0
			? FString::Printf(TEXT("PLAYER %d'S TURN"), GameState->CurrentPlayerIndex + 1)
			: TEXT("WAITING FOR TURN");
	}
}

static bool IsGomokuPlayerReady(const AGomokuGameState* GameState, int32 PlayerId)
{
	if (!GameState)
	{
		return false;
	}
	for (const APlayerState* ExistingState : GameState->PlayerArray)
	{
		const AGomokuPlayerState* PlayerState = Cast<AGomokuPlayerState>(ExistingState);
		if (PlayerState && PlayerState->GomokuPlayerId == PlayerId)
		{
			return PlayerState->bReady;
		}
	}
	return false;
}

void UGomokuHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	CachePresentationWidgets();
	if (TopPanelImage && PanelTexture)
	{
		FSlateBrush PanelBrush = TopPanelImage->GetBrush();
		PanelBrush.SetResourceObject(PanelTexture);
		PanelBrush.DrawAs = ESlateBrushDrawType::Box;
		PanelBrush.Margin = FMargin(0.075f);
		TopPanelImage->SetBrush(PanelBrush);
	}
	RefreshPresentation();
}

void UGomokuHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshPresentation();
	if (!bLoggedFirstRefresh && MyGeometry.GetLocalSize().X >= 320.f && MyGeometry.GetLocalSize().Y >= 240.f)
	{
		bLoggedFirstRefresh = true;
		UE_LOG(LogGomokuHUDWidget, Display,
			TEXT("Gomoku UMG presentation active: class=%s size=%.0fx%.0f panel=%s"),
			*GetClass()->GetName(), MyGeometry.GetLocalSize().X, MyGeometry.GetLocalSize().Y,
			PanelTexture ? TEXT("ready") : TEXT("fallback"));
	}
}

void UGomokuHUDWidget::CachePresentationWidgets()
{
	PlayerCardBorders = { PlayerCard1, PlayerCard2, PlayerCard3, PlayerCard4 };
	PlayerCardTexts = { PlayerCardText1, PlayerCardText2, PlayerCardText3, PlayerCardText4 };

	if (!TopPanelImage || !HeadlineText || !PhaseText || !InventoryText || !HelpText
		|| !GameOverPanel || !GameOverText || PlayerCardBorders.Contains(nullptr) || PlayerCardTexts.Contains(nullptr))
	{
		UE_LOG(LogGomokuHUDWidget, Error,
			TEXT("WBP_GameHUD is missing required named widgets; rerun Scripts/setup_editor_assets.py"));
	}
}

void UGomokuHUDWidget::RefreshPresentation()
{
	if (!HeadlineText || !PhaseText || !InventoryText || !HelpText || !GameOverPanel || !GameOverText)
	{
		return;
	}

	const AGomokuGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr;
	const AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer());
	HeadlineText->SetText(FText::FromString(GetGomokuWidgetHeadline(GameState)));
	PhaseText->SetText(FText::FromString(GameState
		? FString::Printf(TEXT("ROUND %d  ·  %s  ·  MINI-GAME AFTER ROUND %d"),
			GameState->CurrentRoundIndex, *GetGomokuWidgetPhaseLabel(GameState->MatchPhase),
			GameState->GetNextMinigameRound())
		: TEXT("CONNECTING TO MATCH STATE")));

	const int32 PlayerCount = GameState ? FMath::Clamp(GameState->LocalPlayerCount, 2, 4) : 2;
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerCardTexts.Num(); ++PlayerIndex)
	{
		UBorder* Card = PlayerCardBorders.IsValidIndex(PlayerIndex) ? PlayerCardBorders[PlayerIndex] : nullptr;
		UTextBlock* CardText = PlayerCardTexts[PlayerIndex];
		if (!Card || !CardText)
		{
			continue;
		}
		const bool bVisible = PlayerIndex < PlayerCount;
		Card->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bVisible)
		{
			continue;
		}

		const bool bMiniGameTurn = GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying
			&& GameState->GetMiniGameInputPlayerIndex() == PlayerIndex;
		const bool bCurrent = GameState && (GameState->MatchPhase == EMatchPhase::MiniGamePlaying
			? bMiniGameTurn
			: GameState->CurrentPlayerIndex == PlayerIndex);
		const FLinearColor PlayerColor = GameState && GameState->PlayerColors.IsValidIndex(PlayerIndex)
			? GameState->PlayerColors[PlayerIndex]
			: FLinearColor::White;
		Card->SetBrushColor(bCurrent
			? FLinearColor(0.14f, 0.12f, 0.065f, 0.97f)
			: FLinearColor(0.025f, 0.04f, 0.035f, 0.92f));
		const FLinearColor InactiveTextColor = PlayerColor.GetLuminance() < 0.2f
			? FLinearColor(0.78f, 0.78f, 0.72f)
			: PlayerColor;
		CardText->SetColorAndOpacity(FSlateColor(
			bCurrent ? FLinearColor(0.95f, 0.75f, 0.34f) : InactiveTextColor));

		FString DetailText;
		if (GameState && (GameState->MatchPhase == EMatchPhase::MiniGamePlaying
			|| GameState->MatchPhase == EMatchPhase::MiniGameResult))
		{
			if (GameState->MiniGameCorrectPlayerIndices.Contains(PlayerIndex))
			{
				DetailText = TEXT("SOLVED  ·  ENERGY AWARDED");
			}
			else if (GameState->MiniGameSubmittedPlayerIndices.Contains(PlayerIndex))
			{
				DetailText = TEXT("SUBMITTED");
			}
			else if (bMiniGameTurn)
			{
				DetailText = TEXT("CHOOSE THE WINNING CELL");
			}
			else if (GameState->MatchPhase == EMatchPhase::MiniGameResult)
			{
				DetailText = TEXT("NO SUBMISSION");
			}
			else
			{
				DetailText = TEXT("WAITING FOR HOTSEAT TURN");
			}
		}
		else if (GameState && GameState->MatchPhase == EMatchPhase::Waiting)
		{
			DetailText = IsGomokuPlayerReady(GameState, PlayerIndex + 1) ? TEXT("READY") : TEXT("NOT READY");
		}
		else if (GameState && GameState->PlayerTimes.IsValidIndex(PlayerIndex))
		{
			const FGomokuPlayerTimeState& TimeState = GameState->PlayerTimes[PlayerIndex];
			const float TurnRemaining = FMath::Max(0.f, GameState->MaxTurnTime - TimeState.TurnElapsedThisTurn);
			DetailText = FString::Printf(TEXT("TOTAL %.1fs  ·  TURN %.1fs"), TimeState.PersonalRemaining, TurnRemaining);
		}
		else
		{
			DetailText = TEXT("WAITING");
		}
		CardText->SetText(FText::FromString(FString::Printf(TEXT("PLAYER %d%s\n%s"),
			PlayerIndex + 1, bCurrent ? TEXT("  ·  ACTIVE") : TEXT(""), *DetailText)));
	}

	TArray<int32> ItemIds;
	int32 Energy = 0;
	ResolveLocalInventory(ItemIds, Energy);
	FString InventoryLine = FString::Printf(TEXT("ENERGY %d/%d"), Energy, UGomokuItemLibrary::MaxEnergy);
	for (int32 ItemId = 1; ItemId <= 5; ++ItemId)
	{
		FItemData ItemData;
		UGomokuItemLibrary::GetItemData(ItemId, ItemData);
		int32 Count = 0;
		for (const int32 OwnedItemId : ItemIds)
		{
			Count += OwnedItemId == ItemId ? 1 : 0;
		}
		const bool bSelected = PlayerController && PlayerController->bItemTargetingActive
			&& PlayerController->SelectedItemId == ItemId;
		InventoryLine += FString::Printf(TEXT("    %s%d %s E%d x%d%s"),
			bSelected ? TEXT("[") : TEXT(""), ItemId, *ItemData.DisplayName.ToString(),
			ItemData.EnergyCost, Count, bSelected ? TEXT("]") : TEXT(""));
	}
	InventoryText->SetText(FText::FromString(InventoryLine));
	if (GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying)
	{
		HelpText->SetText(FText::FromString(FString::Printf(
			TEXT("PLAYER %d: LMB SUBMIT  ·  RMB DRAG ORBIT  ·  WHEEL ZOOM  ·  F RESET VIEW"),
			GameState->GetMiniGameInputPlayerIndex() + 1)));
	}
	else
	{
		HelpText->SetText(FText::FromString(PlayerController && PlayerController->bItemTargetingActive
			? FString::Printf(TEXT("ITEM %d TARGETING  ·  LMB USE  ·  ESC CANCEL"), PlayerController->SelectedItemId)
			: TEXT("LMB PLACE  ·  RMB DRAG ORBIT  ·  WHEEL ZOOM  ·  F VIEW  ·  R RESTART")));
	}

	const bool bGameOver = GameState && GameState->MatchPhase == EMatchPhase::GameOver;
	GameOverPanel->SetVisibility(bGameOver ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bGameOver)
	{
		GameOverText->SetText(FText::FromString(GetGomokuWidgetHeadline(GameState)
			+ TEXT("\n\nPRESS R TO START A NEW MATCH")));
	}
}

void UGomokuHUDWidget::ResolveLocalInventory(TArray<int32>& OutItemIds, int32& OutEnergy) const
{
	OutItemIds.Reset();
	OutEnergy = 0;
	const UWorld* World = GetWorld();
	const AGomokuGameState* GameState = World ? World->GetGameState<AGomokuGameState>() : nullptr;
	const AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer());
	if (!World || !GameState)
	{
		return;
	}

	const int32 LocalPlayerIndex = GameState->MatchPhase == EMatchPhase::MiniGamePlaying
		? GameState->GetMiniGameInputPlayerIndex()
		: GameState->CurrentPlayerIndex;
	if (World->GetNetMode() == NM_Standalone && GameState->GetRuleEngine() && LocalPlayerIndex >= 0)
	{
		const FGomokuPlayerStateData Data = GameState->GetRuleEngine()->GetPlayerStateData(LocalPlayerIndex + 1);
		OutItemIds = Data.ItemIds;
		OutEnergy = Data.Energy;
		return;
	}

	if (PlayerController)
	{
		if (const AGomokuPlayerState* PlayerState = PlayerController->GetPlayerState<AGomokuPlayerState>())
		{
			OutItemIds = PlayerState->InventoryItemIds;
			OutEnergy = PlayerState->Energy;
		}
	}
}
