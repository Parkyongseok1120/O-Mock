// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GomokuHUDWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * Read-only presentation widget for the replicated Gomoku match state.
 * Game rules stay in RuleEngine/GameState; this widget only displays their public state.
 */
UCLASS(Blueprintable)
class O_MOCK_API UGomokuHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gomoku|UI")
	TObjectPtr<UTexture2D> PanelTexture;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void CachePresentationWidgets();
	void RefreshPresentation();
	void ResolveLocalInventory(TArray<int32>& OutItemIds, int32& OutEnergy,
		TArray<int32>& OutLockedItemIds, bool& bOutUsedItemThisTurn, int32& OutPendingItemId) const;
	void SelectDisplayedInventorySlot(int32 SlotIndex);

	UFUNCTION()
	void HandleInventorySlot1Clicked();

	UFUNCTION()
	void HandleInventorySlot2Clicked();

	UFUNCTION()
	void HandleLobbyReadyClicked();

	UFUNCTION()
	void HandleLobbyStartClicked();

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UImage> TopPanelImage;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> PlayerCard1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> PlayerCard2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> PlayerCard3;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> PlayerCard4;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerCardText1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerCardText2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerCardText3;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerCardText4;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PlayerCardBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PlayerCardTexts;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> InventoryText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> InventorySlotButton1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> InventorySlotButton2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> InventorySlotText1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> InventorySlotText2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemStatusText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> InventoryOfferPanel;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> InventoryOfferText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> LobbyPanel;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> LobbyStatusText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> LobbyReadyButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> LobbyReadyButtonText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> LobbyStartButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> LobbyStartButtonText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> InventorySlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> InventorySlotTexts;

	TArray<int32> DisplayedInventoryItemIds;
	int32 DisplayedPendingInventoryItemId = 0;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> HelpText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> PreviewLegendText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UBorder> GameOverPanel;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> GameOverText;

	bool bLoggedFirstRefresh = false;
};
