// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"
#include "GomokuBoardActor.h"
#include "GomokuGameState.h"
#include "GomokuGameMode.h"
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
	if (!GetWorld())
		return;

	AGomokuGameState* GSState = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GSState || !GSState->IsGameActive)
	{
		// Ignore clicks after game over / when not active.
		return;
	}

	if (!BoardActor)
		return;

	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	BoardActor->OnScreenClick(X, Y);
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
