// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"
#include "GomokuBoardActor.h"
#include "GomokuGameState.h"
#include "GomokuGameMode.h"
#include "GomokuPlayerState.h"
#include "EngineUtils.h"

AGomokuPlayerController::AGomokuPlayerController()
{
	bShowMouseCursor = true;
}

void AGomokuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		GomokuGSState = Cast<AGomokuGameState>(World->GetGameState());
	}
	ResolveBoardActor();
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
	GS->HandleUseItem(RequestingPlayerIndex, ItemId, TargetCell, TargetPlayerIndex);
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

void AGomokuPlayerController::SelectItem(int32 ItemId)
{
	SelectedItemId = ItemId;
	bItemTargetingActive = (ItemId > 0);
}

void AGomokuPlayerController::CancelItemTargeting()
{
	SelectedItemId = 0;
	bItemTargetingActive = false;
}

void AGomokuPlayerController::RequestUseSelectedItem(const FIntPoint& TargetCell, int32 TargetPlayerIndex)
{
	if (!bItemTargetingActive || SelectedItemId <= 0)
	{
		return;
	}

	if (GetNetMode() == NM_Standalone)
	{
		if (AGomokuGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr)
		{
			GS->HandleUseItem(GS->CurrentPlayerIndex, SelectedItemId, TargetCell, TargetPlayerIndex);
		}
	}
	else
	{
		Server_RequestUseItem(SelectedItemId, TargetCell, TargetPlayerIndex);
	}
	CancelItemTargeting();
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
