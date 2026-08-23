// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"
#include "GomokuBoardActor.h"
#include "GomokuGameState.h"
#include "GomokuGameMode.h"
#include "GomokuItemLibrary.h"
#include "GomokuPlayerState.h"
#include "EngineUtils.h"

AGomokuPlayerController::AGomokuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AGomokuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		GomokuGSState = Cast<AGomokuGameState>(World->GetGameState());
		if (GomokuGSState)
		{
			LastObservedTurnIndex = GomokuGSState->CurrentPlayerIndex;
			LastObservedMatchPhase = GomokuGSState->MatchPhase;
		}
	}
	ResolveBoardActor();
	if (IsLocalController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void AGomokuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(InputComponent);

	// Use BindKey for left mouse button — no EnhancedInput/BindAction.
	InputComponent->BindKey(
		EKeys::LeftMouseButton,
		IE_Pressed,
		this,
		&AGomokuPlayerController::HandlePrimaryClick);

	// R key to restart game via GameMode::RestartGame → GameState::RestartMatch.
	InputComponent->BindKey(
		EKeys::R,
		IE_Pressed,
		this,
		&AGomokuPlayerController::HandleRestartKey);

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AGomokuPlayerController::HandleSelectItem1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AGomokuPlayerController::HandleSelectItem2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AGomokuPlayerController::HandleSelectItem3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AGomokuPlayerController::HandleSelectItem4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AGomokuPlayerController::HandleSelectItem5);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AGomokuPlayerController::CancelItemTargeting);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AGomokuPlayerController::HandleReadyKey);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AGomokuPlayerController::HandleStartMatchKey);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AGomokuPlayerController::HandleResetCameraKey);
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &AGomokuPlayerController::HandleCameraZoom);

	bShowMouseCursor = true;
}

void AGomokuPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	ResolveBoardActor();
	if (!GomokuGSState && GetWorld())
	{
		GomokuGSState = GetWorld()->GetGameState<AGomokuGameState>();
	}
	if (!IsLocalController())
	{
		return;
	}
	if (GomokuGSState && (LastObservedTurnIndex != GomokuGSState->CurrentPlayerIndex
		|| LastObservedMatchPhase != GomokuGSState->MatchPhase))
	{
		if (bItemTargetingActive)
		{
			ClearItemSelection();
			bItemUseRequestPending = false;
			SetItemFeedback(TEXT("Item selection cleared because the turn or phase changed."), false);
		}
		LastObservedTurnIndex = GomokuGSState->CurrentPlayerIndex;
		LastObservedMatchPhase = GomokuGSState->MatchPhase;
	}
	if (BoardActor && IsInputKeyDown(EKeys::RightMouseButton))
	{
		float MouseDeltaX = 0.f;
		float MouseDeltaY = 0.f;
		GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
		BoardActor->OrbitCamera(
			MouseDeltaX * CameraOrbitYawSensitivity,
			MouseDeltaY * CameraOrbitPitchSensitivity);
		if (GomokuGSState && GetNetMode() == NM_Standalone)
		{
			GomokuGSState->ClearHoveredCell();
		}
		return;
	}

	FVector2D MousePosition;
	if (GetMousePosition(MousePosition.X, MousePosition.Y))
	{
		HandleMouseMove(MousePosition);
	}
}

void AGomokuPlayerController::HandlePrimaryClick()
{
	if (!GetWorld()) return;

	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS || !GS->IsGameActive) return;

	ResolveBoardActor();
	if (!BoardActor) return;

	FVector2D MousePos;
	if (!GetMousePosition(MousePos.X, MousePos.Y)) return;

	FIntPoint Cell;
	if (BoardActor->ScreenToGrid(this, MousePos, Cell))
	{
		if (GS->MatchPhase == EMatchPhase::MiniGamePlaying)
		{
			if (GetNetMode() == NM_Standalone)
			{
				GS->SubmitMiniGameAnswer(GS->GetMiniGameInputPlayerIndex(), Cell);
			}
			else
			{
				Server_SubmitMiniGameAnswer(Cell);
			}
		}
		else if (bItemTargetingActive && SelectedItemId > 0)
		{
			RequestUseSelectedItem(Cell);
		}
		else
		{
			if (GetNetMode() == NM_Standalone)
			{
				GS->HandlePlaceStone(GS->CurrentPlayerIndex, Cell);
			}
			else
			{
				Server_RequestPlaceStone(Cell);
			}
		}
	}
}

void AGomokuPlayerController::Server_RequestPlaceStone_Implementation(FIntPoint Cell)
{
	if (!HasAuthority())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	AGomokuGameState* GS = Cast<AGomokuGameState>(World->GetGameState());
	if (!GS)
		return;

	const int32 RequestingPlayerIndex = ResolveNetworkPlayerIndex();
	if (RequestingPlayerIndex == INDEX_NONE)
	{
		return;
	}
	GS->HandlePlaceStone(RequestingPlayerIndex, Cell);
}

void AGomokuPlayerController::Server_RequestUseItem_Implementation(int32 ItemId, FIntPoint TargetCell, int32 TargetPlayerIndex)
{
	if (!HasAuthority() || !GetWorld())
	{
		Client_NotifyItemUseResult(false, ItemId, false);
		return;
	}

	AGomokuGameState* GS = Cast<AGomokuGameState>(GetWorld()->GetGameState());
	if (!GS)
	{
		Client_NotifyItemUseResult(false, ItemId, false);
		return;
	}

	const int32 RequestingPlayerIndex = ResolveNetworkPlayerIndex();
	if (RequestingPlayerIndex == INDEX_NONE)
	{
		Client_NotifyItemUseResult(false, ItemId, false);
		return;
	}
	const bool bSuccess = GS->HandleUseItem(RequestingPlayerIndex, ItemId, TargetCell, TargetPlayerIndex);
	FItemData ItemData;
	const bool bIsCellTarget = UGomokuItemLibrary::GetItemData(ItemId, ItemData)
		&& ItemData.TargetType == EItemTargetType::Cell;
	const bool bCanRetryTarget = !bSuccess && bIsCellTarget
		&& GS->MatchPhase == EMatchPhase::Playing
		&& GS->CurrentPlayerIndex == RequestingPlayerIndex
		&& UGomokuItemLibrary::CanUseItem(GS->GetRuleEngine(), RequestingPlayerIndex + 1, ItemId);
	Client_NotifyItemUseResult(bSuccess, ItemId, bCanRetryTarget);
}

void AGomokuPlayerController::Client_NotifyItemUseResult_Implementation(bool bSuccess, int32 ItemId, bool bCanRetryTarget)
{
	bItemUseRequestPending = false;
	FItemData ItemData;
	UGomokuItemLibrary::GetItemData(ItemId, ItemData);
	if (bSuccess)
	{
		ClearItemSelection();
		SetItemFeedback(FString::Printf(TEXT("%s used successfully."), *ItemData.DisplayName.ToString()), true);
	}
	else if (bCanRetryTarget)
	{
		SetItemFeedback(FString::Printf(TEXT("%s cannot be used there. Choose another target or press Esc."),
			*ItemData.DisplayName.ToString()), false);
	}
	else
	{
		ClearItemSelection();
		SetItemFeedback(FString::Printf(TEXT("%s is no longer usable; selection cleared."),
			*ItemData.DisplayName.ToString()), false);
	}
}

void AGomokuPlayerController::RequestReplacePendingInventoryItem(int32 DiscardItemId)
{
	if (bItemUseRequestPending || DiscardItemId <= 0 || !GetWorld())
	{
		SetItemFeedback(TEXT("Wait for the current inventory request to finish."), false);
		return;
	}
	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS)
	{
		return;
	}
	int32 NewItemId = 0;
	if (GetNetMode() == NM_Standalone && GS->GetRuleEngine() && GS->CurrentPlayerIndex >= 0)
	{
		NewItemId = GS->GetRuleEngine()->GetPlayerStateData(GS->CurrentPlayerIndex + 1).PendingInventoryItemId;
		const bool bSuccess = GS->HandleReplacePendingInventoryItem(GS->CurrentPlayerIndex, DiscardItemId);
		Client_NotifyInventoryReplaceResult_Implementation(bSuccess, DiscardItemId, NewItemId);
		return;
	}
	if (const AGomokuPlayerState* LocalState = GetPlayerState<AGomokuPlayerState>())
	{
		NewItemId = LocalState->PendingInventoryItemId;
	}
	bItemUseRequestPending = true;
	SetItemFeedback(TEXT("Waiting for the server to replace the selected slot..."), true);
	Server_RequestReplacePendingInventoryItem(DiscardItemId);
}

void AGomokuPlayerController::Server_RequestReplacePendingInventoryItem_Implementation(int32 DiscardItemId)
{
	if (!HasAuthority() || !GetWorld())
	{
		Client_NotifyInventoryReplaceResult(false, DiscardItemId, 0);
		return;
	}
	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	const int32 PlayerIndex = ResolveNetworkPlayerIndex();
	if (!GS || PlayerIndex == INDEX_NONE)
	{
		Client_NotifyInventoryReplaceResult(false, DiscardItemId, 0);
		return;
	}
	const int32 NewItemId = GS->GetRuleEngine()
		? GS->GetRuleEngine()->GetPlayerStateData(PlayerIndex + 1).PendingInventoryItemId : 0;
	Client_NotifyInventoryReplaceResult(
		GS->HandleReplacePendingInventoryItem(PlayerIndex, DiscardItemId), DiscardItemId, NewItemId);
}

void AGomokuPlayerController::Client_NotifyInventoryReplaceResult_Implementation(
	bool bSuccess, int32 DiscardItemId, int32 NewItemId)
{
	bItemUseRequestPending = false;
	ClearItemSelection();
	FItemData DiscardData;
	FItemData NewData;
	UGomokuItemLibrary::GetItemData(DiscardItemId, DiscardData);
	UGomokuItemLibrary::GetItemData(NewItemId, NewData);
	SetItemFeedback(bSuccess
		? FString::Printf(TEXT("Discarded %s and stored %s. It unlocks next turn."),
			*DiscardData.DisplayName.ToString(), *NewData.DisplayName.ToString())
		: TEXT("That inventory replacement is no longer valid. The server kept your items unchanged."),
		bSuccess);
}

void AGomokuPlayerController::Server_SetReadyForLobby_Implementation(bool bReady)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	if (AGomokuGameMode* GM = Cast<AGomokuGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->SetPlayerReady(this, bReady);
	}
}

void AGomokuPlayerController::Server_RequestStartLobbyMatch_Implementation(const FString& MapName)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	if (AGomokuGameMode* GM = Cast<AGomokuGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->TryStartMatch(this, MapName);
	}
}

void AGomokuPlayerController::Server_SubmitMiniGameAnswer_Implementation(FIntPoint AnswerCell)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	AGomokuGameState* GS = Cast<AGomokuGameState>(GetWorld()->GetGameState());
	if (!GS)
	{
		return;
	}

	const int32 RequestingPlayerIndex = ResolveNetworkPlayerIndex();
	if (RequestingPlayerIndex == INDEX_NONE)
	{
		return;
	}
	GS->SubmitMiniGameAnswer(RequestingPlayerIndex, AnswerCell);
}

void AGomokuPlayerController::Server_UpdateHoveredCell_Implementation(FIntPoint Cell, bool bHasValidCell)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS || ResolveNetworkPlayerIndex() != GS->CurrentPlayerIndex)
	{
		return;
	}
	if (bHasValidCell)
	{
		GS->SetHoveredCell(Cell);
	}
	else
	{
		GS->ClearHoveredCell();
	}
}

void AGomokuPlayerController::Server_RequestAbandonMatch_Implementation()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	const int32 PlayerIndex = ResolveNetworkPlayerIndex();
	if (PlayerIndex == INDEX_NONE)
	{
		return;
	}
	if (AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>())
	{
		GS->RequestAbandonPlayer(PlayerIndex + 1);
	}
}

void AGomokuPlayerController::Server_RequestRestartMatch_Implementation()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	AGomokuGameMode* GM = GetWorld()->GetAuthGameMode<AGomokuGameMode>();
	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GM || !GS || GS->PlayerArray.IsEmpty() || PlayerState != GS->PlayerArray[0])
	{
		return;
	}
	GM->RestartGame();
}

void AGomokuPlayerController::HandleRestartKey()
{
	if (!GetWorld())
		return;

	if (GetNetMode() == NM_Standalone)
	{
		if (AGomokuGameMode* GM = GetWorld()->GetAuthGameMode<AGomokuGameMode>())
		{
			GM->RestartGame();
		}
	}
	else
	{
		Server_RequestRestartMatch();
	}
}

void AGomokuPlayerController::OnMouseMoveX(float Value)
{
	if (!GetWorld()) return;
	FVector2D MousePos;
	GetMousePosition(MousePos.X, MousePos.Y);
	HandleMouseMove(MousePos);
}

void AGomokuPlayerController::OnMouseMoveY(float Value)
{
	OnMouseMoveX(Value); // reuse same handler via current mouse position.
}

void AGomokuPlayerController::HandleMouseMove(FVector2D MousePos)
{
	ResolveBoardActor();
	if (!GetWorld() || !BoardActor || !GomokuGSState)
		return;

	FIntPoint Cell;
	const bool bValid = BoardActor->ScreenToGrid(this, MousePos, Cell);
	const int32 HoverPlayerIndex = GomokuGSState->MatchPhase == EMatchPhase::MiniGamePlaying
		? GomokuGSState->GetMiniGameInputPlayerIndex()
		: GomokuGSState->CurrentPlayerIndex;
	if (bValid == bLastReportedHoverWasValid
		&& (!bValid || Cell == LastReportedHoveredCell)
		&& HoverPlayerIndex == LastReportedHoverPlayerIndex)
	{
		return;
	}
	bLastReportedHoverWasValid = bValid;
	LastReportedHoveredCell = bValid ? Cell : FIntPoint(-1, -1);
	LastReportedHoverPlayerIndex = HoverPlayerIndex;
	if (GetNetMode() == NM_Standalone)
	{
		if (bValid)
		{
			GomokuGSState->SetHoveredCell(Cell);
		}
		else
		{
			GomokuGSState->ClearHoveredCell();
		}
	}
	else
	{
		Server_UpdateHoveredCell(Cell, bValid);
	}
}

bool AGomokuPlayerController::SelectItem(int32 ItemId)
{
	if (bItemUseRequestPending)
	{
		SetItemFeedback(TEXT("Wait for the server to finish validating the current item."), false);
		return false;
	}
	FString UnavailableReason;
	if (!CanSelectItemForCurrentPlayer(ItemId, UnavailableReason))
	{
		ClearItemSelection();
		SetItemFeedback(UnavailableReason, false);
		return false;
	}

	SelectedItemId = ItemId;
	bItemTargetingActive = true;
	FItemData ItemData;
	UGomokuItemLibrary::GetItemData(ItemId, ItemData);
	SetItemFeedback(FString::Printf(TEXT("Selected %s. %s"),
		*ItemData.DisplayName.ToString(), *ItemData.TargetInstruction.ToString()), true);

	// Player-target items have a deterministic next-player target in this prototype.
	// They execute directly from the inventory card/hotkey and still remain server-authoritative.
	if (ItemData.TargetType == EItemTargetType::Player)
	{
		RequestUseSelectedItem(FIntPoint(-1, -1), -1);
	}
	return true;
}

void AGomokuPlayerController::CancelItemTargeting()
{
	if (bItemUseRequestPending)
	{
		SetItemFeedback(TEXT("The item request is already being validated by the server."), false);
		return;
	}
	if (bItemTargetingActive)
	{
		ClearItemSelection();
		SetItemFeedback(TEXT("Item selection canceled."), false);
	}
}

void AGomokuPlayerController::RequestUseSelectedItem(const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!bItemTargetingActive || SelectedItemId <= 0)
	{
		return;
	}
	if (bItemUseRequestPending)
	{
		SetItemFeedback(TEXT("Waiting for the previous item request."), false);
		return;
	}

	if (GetNetMode() == NM_Standalone)
	{
		if (AGomokuGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr)
		{
			const int32 AttemptedItemId = SelectedItemId;
			const bool bSuccess = GS->HandleUseItem(
				GS->CurrentPlayerIndex, AttemptedItemId, TargetCell, TargetPlayerIndex);
			FItemData ItemData;
			const bool bCanRetryTarget = !bSuccess
				&& UGomokuItemLibrary::GetItemData(AttemptedItemId, ItemData)
				&& ItemData.TargetType == EItemTargetType::Cell
				&& UGomokuItemLibrary::CanUseItem(GS->GetRuleEngine(), GS->CurrentPlayerIndex + 1, AttemptedItemId);
			Client_NotifyItemUseResult_Implementation(bSuccess, AttemptedItemId, bCanRetryTarget);
		}
	}
	else
	{
		bItemUseRequestPending = true;
		SetItemFeedback(TEXT("Waiting for server item validation..."), true);
		Server_RequestUseItem(SelectedItemId, TargetCell, TargetPlayerIndex);
	}
}

bool AGomokuPlayerController::CanSelectItemForCurrentPlayer(int32 ItemId, FString& OutReason) const
{
	OutReason.Reset();
	if (bItemUseRequestPending)
	{
		OutReason = TEXT("Wait for the server to finish validating the current item.");
		return false;
	}
	FItemData ItemData;
	if (!UGomokuItemLibrary::GetItemData(ItemId, ItemData))
	{
		OutReason = TEXT("Unknown item.");
		return false;
	}

	const AGomokuGameState* GS = GomokuGSState
		? GomokuGSState.Get()
		: (GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr);
	if (!GS || !GS->IsGameActive || GS->MatchPhase != EMatchPhase::Playing)
	{
		OutReason = TEXT("Items are available only during the main match.");
		return false;
	}
	if (!GS->bItemsEnabled)
	{
		OutReason = TEXT("Items are disabled in this room.");
		return false;
	}

	TArray<int32> Inventory;
	TArray<int32> LockedItems;
	int32 Energy = 0;
	bool bUsedThisTurn = false;
	int32 PendingItemId = 0;
	int32 LocalPlayerIndex = INDEX_NONE;
	if (GetNetMode() == NM_Standalone && GS->GetRuleEngine() && GS->CurrentPlayerIndex >= 0)
	{
		LocalPlayerIndex = GS->CurrentPlayerIndex;
		const FGomokuPlayerStateData Data = GS->GetRuleEngine()->GetPlayerStateData(LocalPlayerIndex + 1);
		Inventory = Data.ItemIds;
		LockedItems = Data.ItemIdsGainedThisTurn.Array();
		Energy = Data.Energy;
		bUsedThisTurn = Data.bUsedItemThisTurn;
		PendingItemId = Data.PendingInventoryItemId;
	}
	else if (const AGomokuPlayerState* GomokuState = GetPlayerState<AGomokuPlayerState>())
	{
		LocalPlayerIndex = GomokuState->GomokuPlayerId - 1;
		Inventory = GomokuState->InventoryItemIds;
		LockedItems = GomokuState->LockedInventoryItemIds;
		Energy = GomokuState->Energy;
		bUsedThisTurn = GomokuState->bUsedItemThisTurn;
		PendingItemId = GomokuState->PendingInventoryItemId;
	}

	if (LocalPlayerIndex != GS->CurrentPlayerIndex)
	{
		OutReason = TEXT("Wait for your turn before using an item.");
		return false;
	}
	if (PendingItemId > 0)
	{
		OutReason = TEXT("Choose an inventory slot to discard before using an item.");
		return false;
	}
	if (!Inventory.Contains(ItemId))
	{
		OutReason = FString::Printf(TEXT("You do not own %s."), *ItemData.DisplayName.ToString());
		return false;
	}
	if (LockedItems.Contains(ItemId))
	{
		OutReason = TEXT("New items unlock at the start of your next turn.");
		return false;
	}
	if (bUsedThisTurn)
	{
		OutReason = TEXT("Only one item can be used each turn.");
		return false;
	}
	if (Energy < ItemData.EnergyCost)
	{
		OutReason = FString::Printf(TEXT("Not enough energy: %d required, %d available."),
			ItemData.EnergyCost, Energy);
		return false;
	}
	return true;
}

void AGomokuPlayerController::ClearItemSelection()
{
	SelectedItemId = 0;
	bItemTargetingActive = false;
}

void AGomokuPlayerController::SetItemFeedback(const FString& Message, bool bSuccess)
{
	ItemFeedbackText = Message;
	bLastItemFeedbackSuccess = bSuccess;
}

void AGomokuPlayerController::RequestAbandonMatch()
{
	if (!GetWorld())
	{
		return;
	}
	if (GetNetMode() == NM_Standalone)
	{
		if (AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>())
		{
			GS->RequestAbandonCurrentPlayer();
		}
	}
	else
	{
		Server_RequestAbandonMatch();
	}
}

void AGomokuPlayerController::HandleSelectItem1() { SelectItem(1); }
void AGomokuPlayerController::HandleSelectItem2() { SelectItem(2); }
void AGomokuPlayerController::HandleSelectItem3() { SelectItem(3); }
void AGomokuPlayerController::HandleSelectItem4() { SelectItem(4); }
void AGomokuPlayerController::HandleSelectItem5() { SelectItem(5); }
void AGomokuPlayerController::HandleReadyKey() { SetReadyForLobby(true); }
void AGomokuPlayerController::HandleStartMatchKey() { RequestStartLobbyMatch(TEXT("")); }

void AGomokuPlayerController::HandleCameraZoom(float Value)
{
	ResolveBoardActor();
	if (BoardActor)
	{
		BoardActor->ZoomCamera(Value);
	}
}

void AGomokuPlayerController::HandleResetCameraKey()
{
	ResolveBoardActor();
	if (BoardActor)
	{
		BoardActor->ResetCameraView();
	}
}

void AGomokuPlayerController::ResolveBoardActor()
{
	if (IsValid(BoardActor) || !GetWorld())
	{
		return;
	}
	for (TActorIterator<AGomokuBoardActor> It(GetWorld()); It; ++It)
	{
		BoardActor = *It;
		break;
	}
}

int32 AGomokuPlayerController::ResolveNetworkPlayerIndex() const
{
	const AGomokuPlayerState* GomokuState = GetPlayerState<AGomokuPlayerState>();
	if (!GomokuState || GomokuState->GomokuPlayerId <= 0)
	{
		return INDEX_NONE;
	}
	return GomokuState->GomokuPlayerId - 1;
}

void AGomokuPlayerController::SetReadyForLobby(bool bReady)
{
	if (HasAuthority())
	{
		Server_SetReadyForLobby_Implementation(bReady);
	}
	else
	{
		Server_SetReadyForLobby(bReady);
	}
}

void AGomokuPlayerController::RequestStartLobbyMatch(const FString& MapName)
{
	if (HasAuthority())
	{
		Server_RequestStartLobbyMatch_Implementation(MapName);
	}
	else
	{
		Server_RequestStartLobbyMatch(MapName);
	}
}
