// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuHUDWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
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
		return TEXT("LOBBY  ·  READY UP TO BEGIN");
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
	// The HUD itself does not block board input, while inventory button children remain clickable.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CachePresentationWidgets();
	if (InventorySlotButton1)
	{
		InventorySlotButton1->OnClicked.AddUniqueDynamic(this, &UGomokuHUDWidget::HandleInventorySlot1Clicked);
	}
	if (InventorySlotButton2)
	{
		InventorySlotButton2->OnClicked.AddUniqueDynamic(this, &UGomokuHUDWidget::HandleInventorySlot2Clicked);
	}
	if (LobbyReadyButton)
	{
		LobbyReadyButton->OnClicked.AddUniqueDynamic(this, &UGomokuHUDWidget::HandleLobbyReadyClicked);
	}
	if (LobbyStartButton)
	{
		LobbyStartButton->OnClicked.AddUniqueDynamic(this, &UGomokuHUDWidget::HandleLobbyStartClicked);
	}
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
	InventorySlotButtons = { InventorySlotButton1, InventorySlotButton2 };
	InventorySlotTexts = { InventorySlotText1, InventorySlotText2 };

	if (!TopPanelImage || !HeadlineText || !PhaseText || !InventoryText || !HelpText || !PreviewLegendText
		|| !ItemStatusText || !GameOverPanel || !GameOverText || PlayerCardBorders.Contains(nullptr)
		|| PlayerCardTexts.Contains(nullptr) || InventorySlotButtons.Contains(nullptr)
		|| InventorySlotTexts.Contains(nullptr) || !LobbyPanel || !LobbyStatusText
		|| !LobbyReadyButton || !LobbyReadyButtonText || !LobbyStartButton || !LobbyStartButtonText
		|| !InventoryOfferPanel || !InventoryOfferText)
	{
		UE_LOG(LogGomokuHUDWidget, Error,
			TEXT("WBP_GameHUD is missing required named widgets; rerun Scripts/setup_editor_assets.py"));
	}
}

void UGomokuHUDWidget::RefreshPresentation()
{
	if (!HeadlineText || !PhaseText || !InventoryText || !ItemStatusText || !HelpText
		|| !GameOverPanel || !GameOverText)
	{
		return;
	}

	const AGomokuGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr;
	const AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer());
	const AGomokuPlayerState* LocalPlayerState = PlayerController
		? PlayerController->GetPlayerState<AGomokuPlayerState>() : nullptr;
	const bool bLobby = GameState && GameState->MatchPhase == EMatchPhase::Waiting;
	if (LobbyPanel && LobbyStatusText && LobbyReadyButton && LobbyReadyButtonText
		&& LobbyStartButton && LobbyStartButtonText)
	{
		LobbyPanel->SetVisibility(bLobby ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bLobby)
		{
			const int32 CurrentPlayers = GameState->PlayerArray.Num();
			const int32 MaxPlayers = FMath::Clamp(GameState->LobbyMaxPlayers, 2, 4);
			const int32 PlannedBots = FMath::Min(GameState->LobbyBotCount,
				FMath::Max(0, MaxPlayers - CurrentPlayers));
			bool bAllReady = CurrentPlayers >= 1 && CurrentPlayers + PlannedBots >= 2;
			FString PlayerLines;
			for (int32 PlayerIndex = 0; PlayerIndex < GameState->PlayerArray.Num(); ++PlayerIndex)
			{
				const AGomokuPlayerState* LobbyPlayer = Cast<AGomokuPlayerState>(GameState->PlayerArray[PlayerIndex]);
				if (!LobbyPlayer)
				{
					continue;
				}
				bAllReady &= LobbyPlayer->bReady;
				PlayerLines += FString::Printf(TEXT("PLAYER %d  ·  %s%s"), LobbyPlayer->GomokuPlayerId,
					LobbyPlayer->bReady ? TEXT("READY") : TEXT("NOT READY"),
					PlayerIndex + 1 < GameState->PlayerArray.Num() ? TEXT("\n") : TEXT(""));
			}
			LobbyStatusText->SetText(FText::FromString(FString::Printf(
				TEXT("PLAYERS %d/%d  ·  BOTS %d  ·  BOARD %dx%d  ·  %s\nPERSONAL %ds  ·  TURN %ds  ·  ITEMS %s  ·  MINI %s\nMATCH REJOIN AFTER START: OFF\n\n%s\n\nAll human players must be ready; bots fill open planned seats."),
				CurrentPlayers, MaxPlayers, PlannedBots, GameState->LobbyBoardSize, GameState->LobbyBoardSize,
				GameState->bLobbyPasswordProtected ? TEXT("LOCKED") : TEXT("OPEN"),
				FMath::RoundToInt(GameState->MaxPersonalTime), FMath::RoundToInt(GameState->MaxTurnTime),
				GameState->bItemsEnabled ? TEXT("ON") : TEXT("OFF"),
				GameState->bMiniGameEnabled ? TEXT("ON") : TEXT("OFF"), *PlayerLines)));
			LobbyReadyButton->SetIsEnabled(LocalPlayerState != nullptr);
			LobbyReadyButtonText->SetText(FText::FromString(LocalPlayerState && LocalPlayerState->bReady
				? TEXT("CANCEL READY") : TEXT("READY")));
			const bool bIsHost = LocalPlayerState && LocalPlayerState->GomokuPlayerId == 1;
			LobbyStartButton->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			LobbyStartButton->SetIsEnabled(bIsHost && bAllReady);
			LobbyStartButtonText->SetText(FText::FromString(bAllReady
				? TEXT("START MATCH") : TEXT("WAITING FOR READY PLAYERS")));
		}
	}
	FString Headline = GetGomokuWidgetHeadline(GameState);
	if (GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying
		&& GetWorld() && GetWorld()->GetNetMode() != NM_Standalone && LocalPlayerState)
	{
		const int32 LocalPlayerIndex = LocalPlayerState->GomokuPlayerId - 1;
		Headline = GameState->MiniGameSubmittedPlayerIndices.Contains(LocalPlayerIndex)
			? FString::Printf(TEXT("PLAYER %d  ·  ANSWER SUBMITTED  ·  %.1fs"),
				LocalPlayerIndex + 1, GameState->MiniGameRemainingTime)
			: FString::Printf(TEXT("PLAYER %d  ·  COMPLETE FIVE IN A ROW  ·  %.1fs"),
				LocalPlayerIndex + 1, GameState->MiniGameRemainingTime);
	}
	HeadlineText->SetText(FText::FromString(Headline));
	PhaseText->SetText(FText::FromString(GameState
		? FString::Printf(TEXT("ROUND %d  ·  %s  ·  %s"),
			GameState->CurrentRoundIndex, *GetGomokuWidgetPhaseLabel(GameState->MatchPhase),
			GameState->bMiniGameEnabled
				? *FString::Printf(TEXT("MINI AFTER ROUND %d"), GameState->GetNextMinigameRound())
				: TEXT("MINI DISABLED"))
		: TEXT("CONNECTING TO MATCH STATE")));

	const int32 PlayerCount = GameState
		? (bLobby ? FMath::Clamp(GameState->PlayerArray.Num(), 0, 4) : FMath::Clamp(GameState->LocalPlayerCount, 2, 4))
		: 2;
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

		const bool bStandaloneMiniGame = GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying
			&& GetWorld() && GetWorld()->GetNetMode() == NM_Standalone;
		const bool bNetworkMiniGameLocal = GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying
			&& GetWorld() && GetWorld()->GetNetMode() != NM_Standalone && LocalPlayerState
			&& LocalPlayerState->GomokuPlayerId - 1 == PlayerIndex
			&& !GameState->MiniGameSubmittedPlayerIndices.Contains(PlayerIndex);
		const bool bMiniGameTurn = bStandaloneMiniGame
			? GameState->GetMiniGameInputPlayerIndex() == PlayerIndex
			: bNetworkMiniGameLocal;
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
			else if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
			{
				DetailText = TEXT("WAITING FOR HOTSEAT TURN");
			}
			else
			{
				DetailText = TEXT("ANSWERING NOW");
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
		bool bBotSeat = false;
		if (GameState)
		{
			for (APlayerState* ExistingState : GameState->PlayerArray)
			{
				const AGomokuPlayerState* GomokuState = Cast<AGomokuPlayerState>(ExistingState);
				if (GomokuState && GomokuState->GomokuPlayerId == PlayerIndex + 1)
				{
					bBotSeat = GomokuState->bGomokuBot;
					break;
				}
			}
		}
		CardText->SetText(FText::FromString(FString::Printf(TEXT("PLAYER %d%s%s\n%s"),
			PlayerIndex + 1, bBotSeat ? TEXT("  ·  BOT") : TEXT(""),
			bCurrent ? TEXT("  ·  ACTIVE") : TEXT(""), *DetailText)));
	}

	TArray<int32> ItemIds;
	TArray<int32> LockedItemIds;
	int32 Energy = 0;
	bool bUsedItemThisTurn = false;
	int32 PendingItemId = 0;
	ResolveLocalInventory(ItemIds, Energy, LockedItemIds, bUsedItemThisTurn, PendingItemId);
	DisplayedInventoryItemIds = ItemIds;
	DisplayedPendingInventoryItemId = PendingItemId;
	FItemData PendingItemData;
	const bool bHasPendingItem = UGomokuItemLibrary::GetItemData(PendingItemId, PendingItemData);
	if (InventoryOfferPanel && InventoryOfferText)
	{
		InventoryOfferPanel->SetVisibility(bHasPendingItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasPendingItem)
		{
			InventoryOfferText->SetText(FText::FromString(FString::Printf(
				TEXT("NEW ITEM OFFER  ·  [%d] %s\n%s\nCHOOSE SLOT 1 OR 2 BELOW TO DISCARD AND REPLACE"),
				PendingItemId, *PendingItemData.DisplayName.ToString(), *PendingItemData.Description.ToString())));
		}
	}
	InventoryText->SetText(FText::FromString(FString::Printf(
		TEXT("INVENTORY %d/2  ·  ENERGY %d/%d%s"),
		ItemIds.Num(), Energy, UGomokuItemLibrary::MaxEnergy,
		GameState && !GameState->bItemsEnabled ? TEXT("  ·  ITEMS OFF") : TEXT(""))));
	for (int32 SlotIndex = 0; SlotIndex < InventorySlotButtons.Num(); ++SlotIndex)
	{
		UButton* SlotButton = InventorySlotButtons[SlotIndex];
		UTextBlock* SlotText = InventorySlotTexts[SlotIndex];
		if (!SlotButton || !SlotText)
		{
			continue;
		}
		const int32 ItemId = ItemIds.IsValidIndex(SlotIndex) ? ItemIds[SlotIndex] : 0;
		FItemData ItemData;
		const bool bHasItem = UGomokuItemLibrary::GetItemData(ItemId, ItemData);
		// Keep every card hit-testable. Disabled Slate buttons return Unhandled and can leak the
		// click into the board beneath the HUD; validation feedback belongs in SelectItem instead.
		SlotButton->SetIsEnabled(true);
		if (!bHasItem)
		{
			SlotText->SetText(FText::FromString(FString::Printf(TEXT("SLOT %d  ·  EMPTY\nAn item is granted when your turn starts."), SlotIndex + 1)));
			SlotText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.52f, 0.48f)));
			SlotButton->SetBackgroundColor(FLinearColor(0.025f, 0.045f, 0.04f, 0.90f));
			SlotButton->SetToolTipText(FText::FromString(TEXT("Empty inventory slot")));
			continue;
		}

		const bool bLocked = LockedItemIds.Contains(ItemId);
		const bool bEnoughEnergy = Energy >= ItemData.EnergyCost;
		const bool bSelected = PlayerController && PlayerController->bItemTargetingActive
			&& PlayerController->SelectedItemId == ItemId;
		FString Availability = bHasPendingItem ? TEXT("CLICK TO DISCARD THIS ITEM") : TEXT("READY");
		if (bLocked)
		{
			Availability = TEXT("NEW · UNLOCKS NEXT TURN");
		}
		else if (bUsedItemThisTurn)
		{
			Availability = TEXT("ITEM ALREADY USED THIS TURN");
		}
		else if (!bEnoughEnergy)
		{
			Availability = TEXT("NOT ENOUGH ENERGY");
		}
		SlotText->SetText(FText::FromString(FString::Printf(
			TEXT("SLOT %d  ·  [%d] %s  ·  ENERGY %d\n%s  ·  %s"),
			SlotIndex + 1, ItemId, *ItemData.DisplayName.ToString(), ItemData.EnergyCost,
			*ItemData.Description.ToString(), *Availability)));
		SlotText->SetColorAndOpacity(FSlateColor(bHasPendingItem
			? FLinearColor(1.0f, 0.58f, 0.30f)
			: (bSelected
			? FLinearColor(1.0f, 0.82f, 0.28f)
			: (bLocked || bUsedItemThisTurn || !bEnoughEnergy
				? FLinearColor(0.62f, 0.65f, 0.60f)
				: FLinearColor(0.86f, 0.96f, 0.90f)))));
		SlotButton->SetBackgroundColor(bHasPendingItem
			? FLinearColor(0.30f, 0.07f, 0.025f, 0.98f)
			: (bSelected
			? FLinearColor(0.25f, 0.16f, 0.035f, 0.98f)
			: (bLocked || bUsedItemThisTurn || !bEnoughEnergy
				? FLinearColor(0.08f, 0.07f, 0.06f, 0.94f)
				: FLinearColor(0.025f, 0.13f, 0.09f, 0.96f))));
		SlotButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s\n%s"),
			*ItemData.Description.ToString(), *ItemData.TargetInstruction.ToString())));
	}
	const FString InventoryStatus = bHasPendingItem
		? TEXT("Inventory full: compare the new offer, then click one existing slot to discard it.")
		: PlayerController && !PlayerController->ItemFeedbackText.IsEmpty()
		? PlayerController->ItemFeedbackText
		: TEXT("Click an owned card to select it. New items unlock next turn; one use per turn.");
	ItemStatusText->SetText(FText::FromString(InventoryStatus));
	ItemStatusText->SetColorAndOpacity(FSlateColor(PlayerController && !PlayerController->ItemFeedbackText.IsEmpty()
		? (PlayerController->bLastItemFeedbackSuccess
			? FLinearColor(0.38f, 1.0f, 0.58f)
			: FLinearColor(1.0f, 0.42f, 0.30f))
		: FLinearColor(0.62f, 0.74f, 0.68f)));
	if (GameState && GameState->MatchPhase == EMatchPhase::MiniGamePlaying)
	{
		const int32 HelpPlayerIndex = GetWorld() && GetWorld()->GetNetMode() != NM_Standalone && LocalPlayerState
			? LocalPlayerState->GomokuPlayerId - 1
			: GameState->GetMiniGameInputPlayerIndex();
		HelpText->SetText(FText::FromString(FString::Printf(
			TEXT("PLAYER %d: LMB SUBMIT  ·  RMB DRAG ORBIT  ·  WHEEL ZOOM  ·  F RESET VIEW"),
			HelpPlayerIndex + 1)));
	}
	else
	{
		FItemData SelectedItemData;
		const bool bHasSelectedItem = PlayerController && PlayerController->bItemTargetingActive
			&& UGomokuItemLibrary::GetItemData(PlayerController->SelectedItemId, SelectedItemData);
		HelpText->SetText(FText::FromString(bHasSelectedItem
			? FString::Printf(TEXT("%s TARGETING  ·  %s  ·  LMB USE  ·  ESC CANCEL"),
				*SelectedItemData.DisplayName.ToString(), *SelectedItemData.TargetInstruction.ToString())
			: TEXT("LMB PLACE  ·  CLICK INVENTORY ITEM  ·  RMB DRAG ORBIT  ·  WHEEL ZOOM  ·  F VIEW  ·  R RESTART")));
	}
	HelpText->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.94f, 0.88f)));
	if (PreviewLegendText)
	{
		FString Legend = TEXT("PREDICTION  ·  GOLD CURRENT WIN  ·  RED OPPONENT WIN THREAT");
		if (PlayerController && PlayerController->bItemTargetingActive && PlayerController->SelectedItemId == 2)
		{
			Legend += TEXT("  ·  CYAN PULL RESULT  ·  GREEN PULLED SOURCE");
		}
		else if (PlayerController && PlayerController->bItemTargetingActive && PlayerController->SelectedItemId == 3)
		{
			Legend += TEXT("  ·  MAGENTA STEALABLE STONE");
		}
		PreviewLegendText->SetText(FText::FromString(Legend));
		PreviewLegendText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.84f, 0.52f)));
		PreviewLegendText->SetVisibility(GameState && GameState->MatchPhase == EMatchPhase::Playing
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const bool bGameOver = GameState && GameState->MatchPhase == EMatchPhase::GameOver;
	GameOverPanel->SetVisibility(bGameOver ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bGameOver)
	{
		GameOverText->SetText(FText::FromString(GetGomokuWidgetHeadline(GameState)
			+ TEXT("\n\nPRESS R TO START A NEW MATCH")));
	}
}

void UGomokuHUDWidget::ResolveLocalInventory(TArray<int32>& OutItemIds, int32& OutEnergy,
	TArray<int32>& OutLockedItemIds, bool& bOutUsedItemThisTurn, int32& OutPendingItemId) const
{
	OutItemIds.Reset();
	OutLockedItemIds.Reset();
	OutEnergy = 0;
	bOutUsedItemThisTurn = false;
	OutPendingItemId = 0;
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
		OutLockedItemIds = Data.ItemIdsGainedThisTurn.Array();
		OutEnergy = Data.Energy;
		bOutUsedItemThisTurn = Data.bUsedItemThisTurn;
		OutPendingItemId = Data.PendingInventoryItemId;
		return;
	}

	if (PlayerController)
	{
		if (const AGomokuPlayerState* PlayerState = PlayerController->GetPlayerState<AGomokuPlayerState>())
		{
			OutItemIds = PlayerState->InventoryItemIds;
			OutLockedItemIds = PlayerState->LockedInventoryItemIds;
			OutEnergy = PlayerState->Energy;
			bOutUsedItemThisTurn = PlayerState->bUsedItemThisTurn;
			OutPendingItemId = PlayerState->PendingInventoryItemId;
		}
	}
}

void UGomokuHUDWidget::HandleInventorySlot1Clicked()
{
	SelectDisplayedInventorySlot(0);
}

void UGomokuHUDWidget::HandleInventorySlot2Clicked()
{
	SelectDisplayedInventorySlot(1);
}

void UGomokuHUDWidget::SelectDisplayedInventorySlot(int32 SlotIndex)
{
	if (!DisplayedInventoryItemIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer()))
	{
		if (DisplayedPendingInventoryItemId > 0)
		{
			PlayerController->RequestReplacePendingInventoryItem(DisplayedInventoryItemIds[SlotIndex]);
		}
		else
		{
			PlayerController->SelectItem(DisplayedInventoryItemIds[SlotIndex]);
		}
	}
}

void UGomokuHUDWidget::HandleLobbyReadyClicked()
{
	AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		return;
	}
	const AGomokuPlayerState* PlayerState = PlayerController->GetPlayerState<AGomokuPlayerState>();
	PlayerController->SetReadyForLobby(!(PlayerState && PlayerState->bReady));
}

void UGomokuHUDWidget::HandleLobbyStartClicked()
{
	if (AGomokuPlayerController* PlayerController = Cast<AGomokuPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestStartLobbyMatch(TEXT(""));
	}
}
