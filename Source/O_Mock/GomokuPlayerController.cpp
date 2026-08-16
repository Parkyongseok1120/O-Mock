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

	// Find board actor in world (spawned by GameMode); no per-cell Actor array used.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGomokuBoardActor> It(World); It; ++It)
		{
			BoardActor = *It;
			break;
		}

		GomokuGSState = Cast<AGomokuGameState>(World->GetGameState());
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

	// Mouse move via BindAxis for hover cell updates.
	InputComponent->BindAxis("MouseX", this, &AGomokuPlayerController::OnMouseMoveX);
	InputComponent->BindAxis("MouseY", this, &AGomokuPlayerController::OnMouseMoveY);

	bShowMouseCursor = true;
}

void AGomokuPlayerController::HandlePrimaryClick()
{
	if (!GetWorld()) return;

	AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS || !GS->IsGameActive) return;

	if (!BoardActor) return;

	FVector2D MousePos;
	if (!GetMousePosition(MousePos.X, MousePos.Y)) return;

	UWorld* World = GetWorld();
	FVector WorldOrigin, WorldDir;
	if (!UGameplayStatics::DeprojectScreenToWorld(this, MousePos, WorldOrigin, WorldDir))
		return;

	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("PlaceStoneTrace")), false, this);

	if (World->LineTraceSingleByChannel(Hit, WorldOrigin, WorldOrigin + WorldDir.GetSafeNormal() * 10000.f, ECC_Visibility, Params))
	{
		int32 GridX = 0;
		int32 GridY = 0;

		if (BoardActor->WorldToGrid(Hit.ImpactPoint, GridX, GridY))
		{
			FIntPoint Cell(GridX, GridY);

			if (GS->MatchPhase == EMatchPhase::MiniGamePlaying)
			{
				if (HasAuthority())
				{
					GS->SubmitMiniGameAnswer(GS->CurrentPlayerIndex, Cell);
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
				// Local hotseat / authority path: direct call without RPC.
				if (HasAuthority())
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

	int32 RequestingPlayerIndex = GS->CurrentPlayerIndex;
	if (const AGomokuPlayerState* GomokuState = GetPlayerState<AGomokuPlayerState>())
	{
		RequestingPlayerIndex = GomokuState->GomokuPlayerId - 1;
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

	int32 RequestingPlayerIndex = GS->CurrentPlayerIndex;
	if (const AGomokuPlayerState* GomokuState = GetPlayerState<AGomokuPlayerState>())
	{
		RequestingPlayerIndex = GomokuState->GomokuPlayerId - 1;
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

	int32 RequestingPlayerIndex = GS->CurrentPlayerIndex;
	if (const AGomokuPlayerState* GomokuState = GetPlayerState<AGomokuPlayerState>())
	{
		RequestingPlayerIndex = GomokuState->GomokuPlayerId - 1;
	}
	GS->SubmitMiniGameAnswer(RequestingPlayerIndex, AnswerCell);
}

void AGomokuPlayerController::HandleRestartKey()
{
	if (!GetWorld())
		return;

	AGomokuGameMode* GM = Cast<AGomokuGameMode>(GetWorld()->GetAuthGameMode());
	if (GM)
	{
		GM->RestartGame();
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
	if (!GetWorld() || !BoardActor || !GomokuGSState)
		return;

	FVector WorldOrigin;
	FVector WorldDir;
	if (!UGameplayStatics::DeprojectScreenToWorld(this, MousePos, WorldOrigin, WorldDir))
		return;

	const FVector End = WorldOrigin + WorldDir * 10000.f;
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(HoverTrace), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, End, ECC_Visibility, QP))
	{
		return;
	}

	int32 GridX = 0;
	int32 GridY = 0;
	if (BoardActor->WorldToGrid(Hit.Location, GridX, GridY))
	{
		GomokuGSState->SetHoveredCell(FIntPoint(GridX, GridY));
	}
	else
	{
		GomokuGSState->ClearHoveredCell();
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

	if (HasAuthority())
	{
		Server_RequestUseItem_Implementation(SelectedItemId, TargetCell, TargetPlayerIndex);
	}
	else
	{
		Server_RequestUseItem(SelectedItemId, TargetCell, TargetPlayerIndex);
	}
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
