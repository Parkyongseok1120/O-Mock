// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuMainMenu, Log, All);

void UGomokuMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RoomButtons = { RoomButton1, RoomButton2, RoomButton3, RoomButton4, RoomButton5 };
	RoomTexts = { RoomText1, RoomText2, RoomText3, RoomText4, RoomText5 };
	if (!HostRoomButton || !MaxPlayersSpinBox || !BotCountSpinBox || !BoardSizeSpinBox || !PersonalTimeSpinBox
		|| !TurnTimeSpinBox || !ItemsEnabledCheckBox || !MiniGameEnabledCheckBox || !RoomPasswordTextBox
		|| !RefreshRoomsButton || !RoomCountText
		|| !JoinRoomButton || !JoinAddressTextBox || !MenuStatusText
		|| RoomButtons.Contains(nullptr) || RoomTexts.Contains(nullptr))
	{
		UE_LOG(LogGomokuMainMenu, Error,
			TEXT("WBP_MainMenu is missing required named widgets; rerun Scripts/setup_editor_assets.py"));
		return;
	}

	HostRoomButton->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleHostClicked);
	JoinRoomButton->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleJoinClicked);
	RefreshRoomsButton->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRefreshClicked);
	RoomButton1->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoom1Clicked);
	RoomButton2->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoom2Clicked);
	RoomButton3->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoom3Clicked);
	RoomButton4->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoom4Clicked);
	RoomButton5->OnClicked.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoom5Clicked);
	MaxPlayersSpinBox->SetMinValue(2.f);
	MaxPlayersSpinBox->SetMaxValue(4.f);
	MaxPlayersSpinBox->SetMinSliderValue(2.f);
	MaxPlayersSpinBox->SetMaxSliderValue(4.f);
	MaxPlayersSpinBox->SetDelta(1.f);
	MaxPlayersSpinBox->SetValue(FMath::Clamp(FMath::RoundToInt(MaxPlayersSpinBox->GetValue()), 2, 4));
	BotCountSpinBox->SetMinValue(0.f);
	BotCountSpinBox->SetMaxValue(3.f);
	BotCountSpinBox->SetMinSliderValue(0.f);
	BotCountSpinBox->SetMaxSliderValue(3.f);
	BotCountSpinBox->SetDelta(1.f);
	BotCountSpinBox->SetValue(FMath::Clamp(FMath::RoundToInt(BotCountSpinBox->GetValue()), 0, 3));
	BoardSizeSpinBox->SetMinValue(15.f);
	BoardSizeSpinBox->SetMaxValue(23.f);
	BoardSizeSpinBox->SetMinSliderValue(15.f);
	BoardSizeSpinBox->SetMaxSliderValue(23.f);
	BoardSizeSpinBox->SetDelta(2.f);
	PersonalTimeSpinBox->SetMinValue(30.f);
	PersonalTimeSpinBox->SetMaxValue(600.f);
	PersonalTimeSpinBox->SetMinSliderValue(30.f);
	PersonalTimeSpinBox->SetMaxSliderValue(600.f);
	PersonalTimeSpinBox->SetDelta(30.f);
	TurnTimeSpinBox->SetMinValue(5.f);
	TurnTimeSpinBox->SetMaxValue(120.f);
	TurnTimeSpinBox->SetMinSliderValue(5.f);
	TurnTimeSpinBox->SetMaxSliderValue(120.f);
	TurnTimeSpinBox->SetDelta(5.f);
	ItemsEnabledCheckBox->SetIsChecked(true);
	MiniGameEnabledCheckBox->SetIsChecked(true);
	if (JoinAddressTextBox->GetText().IsEmpty())
	{
		JoinAddressTextBox->SetText(FText::FromString(TEXT("127.0.0.1")));
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
	HandleRoomsUpdated(TArray<FGomokuRoomInfo>());
	if (UGomokuGameInstance* GameInstance = GetGomokuGameInstance())
	{
		GameInstance->OnRoomsUpdated.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleRoomsUpdated);
		GameInstance->OnSessionStatus.AddUniqueDynamic(this, &UGomokuMainMenuWidget::HandleSessionStatus);
		SetStatus(TEXT("Configure the room rules, host, or refresh the LAN room list."), true);
		GameInstance->FindLanRooms();
	}
	else
	{
		SetStatus(TEXT("GomokuGameInstance is not configured."), false);
	}
}

void UGomokuMainMenuWidget::NativeDestruct()
{
	if (UGomokuGameInstance* GameInstance = GetGomokuGameInstance())
	{
		GameInstance->OnRoomsUpdated.RemoveDynamic(this, &UGomokuMainMenuWidget::HandleRoomsUpdated);
		GameInstance->OnSessionStatus.RemoveDynamic(this, &UGomokuMainMenuWidget::HandleSessionStatus);
	}
	Super::NativeDestruct();
}

void UGomokuMainMenuWidget::HandleHostClicked()
{
	if (UGomokuGameInstance* GameInstance = GetGomokuGameInstance())
	{
		FGomokuRoomSettings Settings;
		Settings.MaxPlayers = FMath::RoundToInt(MaxPlayersSpinBox->GetValue());
		Settings.BotCount = FMath::RoundToInt(BotCountSpinBox->GetValue());
		Settings.BoardSize = FMath::RoundToInt(BoardSizeSpinBox->GetValue());
		Settings.PersonalTimeSeconds = FMath::RoundToInt(PersonalTimeSpinBox->GetValue());
		Settings.TurnTimeSeconds = FMath::RoundToInt(TurnTimeSpinBox->GetValue());
		Settings.bItemsEnabled = ItemsEnabledCheckBox->IsChecked();
		Settings.bMiniGameEnabled = MiniGameEnabledCheckBox->IsChecked();
		Settings.Password = RoomPasswordTextBox->GetText().ToString();
		Settings.Sanitize();
		SetStatus(FString::Printf(TEXT("Creating a %d-seat %dx%d LAN room with %d bot(s)..."),
			Settings.MaxPlayers, Settings.BoardSize, Settings.BoardSize, Settings.BotCount), true);
		GameInstance->HostCustomLanRoom(Settings);
	}
}

void UGomokuMainMenuWidget::HandleJoinClicked()
{
	FString Address = JoinAddressTextBox->GetText().ToString();
	Address.TrimStartAndEndInline();
	if (Address.IsEmpty())
	{
		SetStatus(TEXT("Enter the host PC's IPv4 address, for example 192.168.0.74."), false);
		return;
	}
	if (!Address.Contains(TEXT(":")))
	{
		Address += TEXT(":7777");
	}

	UGomokuGameInstance* GameInstance = GetGomokuGameInstance();
	if (!GameInstance)
	{
		SetStatus(TEXT("GomokuGameInstance is not configured."), false);
		return;
	}

	UE_LOG(LogGomokuMainMenu, Display, TEXT("Main menu join requested: %s"), *Address);
	GameInstance->JoinDirectAddressWithPassword(Address, RoomPasswordTextBox->GetText().ToString());
}

void UGomokuMainMenuWidget::HandleRefreshClicked()
{
	if (UGomokuGameInstance* GameInstance = GetGomokuGameInstance())
	{
		GameInstance->FindLanRooms();
	}
}

void UGomokuMainMenuWidget::HandleRoom1Clicked() { JoinDisplayedRoom(0); }
void UGomokuMainMenuWidget::HandleRoom2Clicked() { JoinDisplayedRoom(1); }
void UGomokuMainMenuWidget::HandleRoom3Clicked() { JoinDisplayedRoom(2); }
void UGomokuMainMenuWidget::HandleRoom4Clicked() { JoinDisplayedRoom(3); }
void UGomokuMainMenuWidget::HandleRoom5Clicked() { JoinDisplayedRoom(4); }

void UGomokuMainMenuWidget::HandleRoomsUpdated(const TArray<FGomokuRoomInfo>& Rooms)
{
	DisplayedResultIndices.Reset();
	DisplayedPasswordFlags.Reset();
	if (RoomCountText)
	{
		RoomCountText->SetText(FText::FromString(FString::Printf(TEXT("JOINABLE ROOMS: %d"), Rooms.Num())));
	}
	for (int32 DisplayIndex = 0; DisplayIndex < RoomButtons.Num(); ++DisplayIndex)
	{
		UButton* Button = RoomButtons[DisplayIndex];
		UTextBlock* Text = RoomTexts.IsValidIndex(DisplayIndex) ? RoomTexts[DisplayIndex] : nullptr;
		if (!Button || !Text)
		{
			continue;
		}
		if (!Rooms.IsValidIndex(DisplayIndex))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FGomokuRoomInfo& Room = Rooms[DisplayIndex];
		DisplayedResultIndices.Add(Room.ResultIndex);
		DisplayedPasswordFlags.Add(Room.bPasswordProtected);
		Button->SetVisibility(ESlateVisibility::Visible);
		Button->SetIsEnabled(Room.CurrentPlayers < Room.MaxPlayers);
		Text->SetText(FText::FromString(FString::Printf(
			TEXT("%s  ·  %d/%d  ·  %s  ·  %dx%d  ·  BOTS %d\nPERSONAL %ds  ·  TURN %ds  ·  ITEMS %s  ·  MINI %s  ·  %dms"),
			*Room.RoomName.Left(18), Room.CurrentPlayers, Room.MaxPlayers,
			Room.bPasswordProtected ? TEXT("LOCKED") : TEXT("OPEN"), Room.BoardSize, Room.BoardSize, Room.BotCount,
			Room.PersonalTimeSeconds, Room.TurnTimeSeconds,
			Room.bItemsEnabled ? TEXT("ON") : TEXT("OFF"),
			Room.bMiniGameEnabled ? TEXT("ON") : TEXT("OFF"), Room.PingInMs)));
	}
}

void UGomokuMainMenuWidget::HandleSessionStatus(const FString& Message, bool bSuccess)
{
	SetStatus(Message, bSuccess);
}

void UGomokuMainMenuWidget::JoinDisplayedRoom(int32 DisplayIndex)
{
	if (!DisplayedResultIndices.IsValidIndex(DisplayIndex))
	{
		SetStatus(TEXT("That room is no longer displayed. Refresh the room list."), false);
		return;
	}
	if (UGomokuGameInstance* GameInstance = GetGomokuGameInstance())
	{
		GameInstance->JoinLanRoomWithPassword(
			DisplayedResultIndices[DisplayIndex], RoomPasswordTextBox->GetText().ToString());
	}
}

UGomokuGameInstance* UGomokuMainMenuWidget::GetGomokuGameInstance() const
{
	return GetGameInstance<UGomokuGameInstance>();
}

void UGomokuMainMenuWidget::SetStatus(const FString& Message, bool bSuccess)
{
	if (!MenuStatusText)
	{
		return;
	}
	MenuStatusText->SetText(FText::FromString(Message));
	MenuStatusText->SetColorAndOpacity(FSlateColor(bSuccess
		? FLinearColor(0.55f, 0.92f, 0.70f)
		: FLinearColor(1.0f, 0.38f, 0.28f)));
}
