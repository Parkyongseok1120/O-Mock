// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GomokuGameInstance.h"
#include "GomokuMainMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class USpinBox;
class UTextBlock;

/** LAN prototype entry screen: create a listen-server room or join one by IPv4 address. */
UCLASS(Blueprintable)
class O_MOCK_API UGomokuMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleRoom1Clicked();

	UFUNCTION()
	void HandleRoom2Clicked();

	UFUNCTION()
	void HandleRoom3Clicked();

	UFUNCTION()
	void HandleRoom4Clicked();

	UFUNCTION()
	void HandleRoom5Clicked();

	UFUNCTION()
	void HandleRoomsUpdated(const TArray<FGomokuRoomInfo>& Rooms);

	UFUNCTION()
	void HandleSessionStatus(const FString& Message, bool bSuccess);

	void SetStatus(const FString& Message, bool bSuccess);
	void JoinDisplayedRoom(int32 DisplayIndex);
	UGomokuGameInstance* GetGomokuGameInstance() const;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> HostRoomButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<USpinBox> MaxPlayersSpinBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<USpinBox> BotCountSpinBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<USpinBox> BoardSizeSpinBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<USpinBox> PersonalTimeSpinBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<USpinBox> TurnTimeSpinBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UCheckBox> ItemsEnabledCheckBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UCheckBox> MiniGameEnabledCheckBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> RoomPasswordTextBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RefreshRoomsButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomCountText;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RoomButton1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RoomButton2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RoomButton3;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RoomButton4;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> RoomButton5;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomText1;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomText2;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomText3;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomText4;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomText5;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UButton> JoinRoomButton;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> JoinAddressTextBox;

	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UTextBlock> MenuStatusText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RoomButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RoomTexts;

	TArray<int32> DisplayedResultIndices;
	TArray<bool> DisplayedPasswordFlags;
};
