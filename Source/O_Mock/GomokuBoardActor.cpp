// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBoardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GomokuGameState.h"

AGomokuBoardActor::AGomokuBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BoardPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardPlane"));
	BoardPlane->SetupAttachment(RootComponent);
	BoardPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardPlane->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	StoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstances"));
	StoneInstances->SetupAttachment(RootComponent);
}

void AGomokuBoardActor::BeginPlay()
{
	Super::BeginPlay();
	if (AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>())
	{
		GS->OnStonePlaced.AddDynamic(this, &AGomokuBoardActor::HandleStonePlaced);
		GS->OnMatchRestarted.AddDynamic(this, &AGomokuBoardActor::HandleMatchRestarted);
		if (UGomokuRuleEngine* Engine = GS->GetRuleEngine())
		{
			const FGomokuMatchConfig Cfg = Engine->GetMatchConfig();
			ApplyBoardSize(Cfg.BoardSizeX, Cfg.BoardSizeY);
		}
	}
	FitCameraToBoard();
}

void AGomokuBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!StoneInstances)
	{
		return;
	}

	for (int32 i = AnimatingInstanceIndices.Num() - 1; i >= 0; --i)
	{
		const int32 Idx = AnimatingInstanceIndices[i];
		FTransform T;
		if (!StoneInstances->GetInstanceTransform(Idx, T, true))
		{
			AnimatingInstanceIndices.RemoveAt(i);
			continue;
		}
		FVector Scale = T.GetScale3D();
		Scale = FMath::VInterpTo(Scale, FVector(1.f), DeltaSeconds, StoneAnimSpeed);
		T.SetScale3D(Scale);
		StoneInstances->UpdateInstanceTransform(Idx, T, true, true, true);
		if (Scale.X >= 0.98f)
		{
			T.SetScale3D(FVector(1.f));
			StoneInstances->UpdateInstanceTransform(Idx, T, true, true, true);
			AnimatingInstanceIndices.RemoveAt(i);
		}
	}
}

void AGomokuBoardActor::ApplyBoardSize(int32 InSizeX, int32 InSizeY)
{
	BoardSizeX = FMath::Max(1, InSizeX);
	BoardSizeY = FMath::Max(1, InSizeY);
	FitCameraToBoard();
}

void AGomokuBoardActor::FitCameraToBoard()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const float Extent = FMath::Max(BoardSizeX, BoardSizeY) * CellSize * 0.55f;
	const FVector CamLoc(0.f, 0.f, FMath::Max(800.f, Extent * 1.6f));
	PC->SetViewTarget(this);
	PC->SetInitialLocationAndRotation(CamLoc, FRotator(-90.f, 0.f, 0.f));
}

FVector AGomokuBoardActor::GetBoardOrigin() const
{
	return FVector(
		-((BoardSizeX - 1) * CellSize) * 0.5f,
		-((BoardSizeY - 1) * CellSize) * 0.5f,
		0.f);
}

FVector AGomokuBoardActor::GridToWorld(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return GetBoardOrigin();
	}
	const FVector Origin = GetBoardOrigin();
	return FVector(Origin.X + X * CellSize, Origin.Y + Y * CellSize, 5.f);
}

bool AGomokuBoardActor::WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const
{
	const FVector Origin = GetBoardOrigin();
	const float DX = (WorldLoc.X - Origin.X) / CellSize;
	const float DY = (WorldLoc.Y - Origin.Y) / CellSize;
	const int32 GX = FMath::RoundToInt(DX);
	const int32 GY = FMath::RoundToInt(DY);
	if (GX < 0 || GY < 0 || GX >= BoardSizeX || GY >= BoardSizeY)
	{
		return false;
	}
	const FVector Center = GridToWorld(GX, GY);
	const float Dist2D = FVector(WorldLoc.X - Center.X, WorldLoc.Y - Center.Y, 0.f).Size();
	if (Dist2D > CellSize * 0.5f)
	{
		return false;
	}
	OutX = GX;
	OutY = GY;
	return true;
}

int32 AGomokuBoardActor::AddStoneAt(int32 X, int32 Y)
{
	if (!StoneInstances || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return INDEX_NONE;
	}
	FTransform T;
	T.SetLocation(GridToWorld(X, Y));
	T.SetScale3D(FVector(0.01f));
	const int32 Index = StoneInstances->AddInstance(T);
	AnimatingInstanceIndices.Add(Index);
	return Index;
}

void AGomokuBoardActor::ClearStones()
{
	AnimatingInstanceIndices.Reset();
	if (StoneInstances)
	{
		StoneInstances->ClearInstances();
	}
}

void AGomokuBoardActor::HandleStonePlaced(const FIntPoint& Cell)
{
	AddStoneAt(Cell.X, Cell.Y);
}

void AGomokuBoardActor::HandleMatchRestarted()
{
	ClearStones();
}

void AGomokuBoardActor::OnScreenClick(int32 ScreenX, int32 ScreenY)
{
	if (!GetWorld())
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
	}

	FVector WorldOrigin;
	FVector WorldDir;
	if (!UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(ScreenX, ScreenY), WorldOrigin, WorldDir))
	{
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(BoardClick), false, this);
	const FVector End = WorldOrigin + WorldDir * 10000.f;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, End, ECC_Visibility, QP))
	{
		// Fallback: intersect Z=0 plane for prototype boards without mesh collision.
		if (FMath::Abs(WorldDir.Z) < KINDA_SMALL_NUMBER)
		{
			return;
		}
		const float T = -WorldOrigin.Z / WorldDir.Z;
		Hit.Location = WorldOrigin + WorldDir * T;
	}

	int32 GridX = 0;
	int32 GridY = 0;
	if (!WorldToGrid(Hit.Location, GridX, GridY))
	{
		return;
	}

	AGomokuGameState* GSState = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GSState || !GSState->IsGameActive)
	{
		return;
	}
	const int32 PlayerIndex = GSState->CurrentPlayerIndex;
	if (PlayerIndex < 0 || PlayerIndex >= GSState->LocalPlayerCount)
	{
		return;
	}
	GSState->HandlePlaceStone(PlayerIndex, FIntPoint(GridX, GridY));
}
