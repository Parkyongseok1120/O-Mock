// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBoardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "GomokuGameState.h"
#include "GomokuRuleEngine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGomokuBoardActor::AGomokuBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BoardPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardPlane"));
	BoardPlane->SetupAttachment(SceneRoot);
	BoardPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardPlane->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoardPlane->SetRelativeLocation(FVector(0.f, 0.f, -2.f));

	StoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer1"));
	StoneInstances->SetupAttachment(SceneRoot);
	StoneInstancesPlayer2 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer2"));
	StoneInstancesPlayer2->SetupAttachment(SceneRoot);
	StoneInstancesPlayer3 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer3"));
	StoneInstancesPlayer3->SetupAttachment(SceneRoot);
	StoneInstancesPlayer4 = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstancesPlayer4"));
	StoneInstancesPlayer4->SetupAttachment(SceneRoot);
	BoardGridInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoardGridInstances"));
	BoardGridInstances->SetupAttachment(SceneRoot);
	BlockedCellInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlockedCellInstances"));
	BlockedCellInstances->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoardMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StoneMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BoardMeshFinder.Succeeded())
	{
		BoardPlane->SetStaticMesh(BoardMeshFinder.Object);
	}
	if (BasicMaterialFinder.Succeeded())
	{
		BoardPlane->SetMaterial(0, BasicMaterialFinder.Object);
	}

	UInstancedStaticMeshComponent* StoneComponents[] = {
		StoneInstances.Get(), StoneInstancesPlayer2.Get(), StoneInstancesPlayer3.Get(), StoneInstancesPlayer4.Get()
	};
	for (UInstancedStaticMeshComponent* Component : StoneComponents)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (StoneMeshFinder.Succeeded())
		{
			Component->SetStaticMesh(StoneMeshFinder.Object);
		}
		if (BasicMaterialFinder.Succeeded())
		{
			Component->SetMaterial(0, BasicMaterialFinder.Object);
		}
	}
	UInstancedStaticMeshComponent* SurfaceComponents[] = {BoardGridInstances.Get(), BlockedCellInstances.Get()};
	for (UInstancedStaticMeshComponent* Component : SurfaceComponents)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CubeMeshFinder.Succeeded())
		{
			Component->SetStaticMesh(CubeMeshFinder.Object);
		}
		if (BasicMaterialFinder.Succeeded())
		{
			Component->SetMaterial(0, BasicMaterialFinder.Object);
		}
	}

	BoardCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BoardCamera"));
	BoardCamera->SetupAttachment(SceneRoot);
	BoardCamera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	BoardCamera->SetRelativeLocation(FVector(0.f, 0.f, 1000.f));
	BoardCamera->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
}

void AGomokuBoardActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (AGomokuGameState* GS = World->GetGameState<AGomokuGameState>())
		{
			GS->OnMatchRestarted.AddDynamic(this, &AGomokuBoardActor::HandleMatchRestarted);
			GS->OnReplicatedBoardChanged.AddDynamic(this, &AGomokuBoardActor::HandleReplicatedBoardChanged);
			if (UGomokuRuleEngine* Engine = GS->GetRuleEngine())
			{
				const FGomokuMatchConfig Cfg = Engine->GetMatchConfig();
				ApplyBoardSize(Cfg.BoardSizeX, Cfg.BoardSizeY);
				RefreshFromReplicatedBoard();
			}
			else if (GS->ReplicatedBoardSizeX > 0 && GS->ReplicatedBoardSizeY > 0)
			{
				RefreshFromReplicatedBoard();
			}
		}
	}

	ConfigureStoneComponent(StoneInstances, FLinearColor(0.03f, 0.03f, 0.03f));
	ConfigureStoneComponent(StoneInstancesPlayer2, FLinearColor(0.92f, 0.92f, 0.92f));
	ConfigureStoneComponent(StoneInstancesPlayer3, FLinearColor(0.15f, 0.45f, 1.0f));
	ConfigureStoneComponent(StoneInstancesPlayer4, FLinearColor(0.95f, 0.12f, 0.08f));
	ConfigureStoneComponent(BoardGridInstances, FLinearColor(0.12f, 0.08f, 0.035f));
	ConfigureStoneComponent(BlockedCellInstances, FLinearColor(0.45f, 0.08f, 0.06f));
	if (BoardPlane && BoardPlane->GetMaterial(0))
	{
		UMaterialInstanceDynamic* BoardMaterial = UMaterialInstanceDynamic::Create(BoardPlane->GetMaterial(0), this);
		if (BoardMaterial)
		{
			const FLinearColor BoardColor(0.76f, 0.48f, 0.20f);
			BoardMaterial->SetVectorParameterValue(TEXT("Color"), BoardColor);
			BoardMaterial->SetVectorParameterValue(TEXT("BaseColor"), BoardColor);
			BoardPlane->SetMaterial(0, BoardMaterial);
			RuntimeMaterials.Add(BoardMaterial);
		}
	}
	FitCameraToBoard();
}

void AGomokuBoardActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (AGomokuGameState* GS = World->GetGameState<AGomokuGameState>())
		{
			GS->OnMatchRestarted.RemoveDynamic(this, &AGomokuBoardActor::HandleMatchRestarted);
			GS->OnReplicatedBoardChanged.RemoveDynamic(this, &AGomokuBoardActor::HandleReplicatedBoardChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGomokuBoardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 i = AnimatingStones.Num() - 1; i >= 0; --i)
	{
		FGomokuAnimatingStone& Animation = AnimatingStones[i];
		UInstancedStaticMeshComponent* Component = Animation.Component.Get();
		if (!Component)
		{
			AnimatingStones.RemoveAtSwap(i);
			continue;
		}
		FTransform T;
		if (!Component->GetInstanceTransform(Animation.InstanceIndex, T, true))
		{
			AnimatingStones.RemoveAtSwap(i);
			continue;
		}
		FVector Scale = T.GetScale3D();
		Scale = FMath::VInterpTo(Scale, Animation.TargetScale, DeltaSeconds, StoneAnimSpeed);
		T.SetScale3D(Scale);
		Component->UpdateInstanceTransform(Animation.InstanceIndex, T, true, true, true);
		if (Scale.Equals(Animation.TargetScale, 0.01f))
		{
			T.SetScale3D(Animation.TargetScale);
			Component->UpdateInstanceTransform(Animation.InstanceIndex, T, true, true, true);
			AnimatingStones.RemoveAtSwap(i);
		}
	}
}

void AGomokuBoardActor::ApplyBoardSize(int32 InSizeX, int32 InSizeY)
{
	BoardSizeX = FMath::Max(1, InSizeX);
	BoardSizeY = FMath::Max(1, InSizeY);
	if (BoardPlane)
	{
		const float Width = BoardSizeX * GetEffectiveCellSize();
		const float Height = BoardSizeY * GetEffectiveCellSize();
		BoardPlane->SetRelativeScale3D(FVector(Width / 100.f, Height / 100.f, 1.f));
	}
	RebuildBoardGrid();
	FitCameraToBoard();
}

void AGomokuBoardActor::RebuildBoardGrid()
{
	if (!BoardGridInstances || !BlockedCellInstances)
	{
		return;
	}

	BoardGridInstances->ClearInstances();
	BlockedCellInstances->ClearInstances();
	const float EffectiveCellSize = GetEffectiveCellSize();
	const float Width = BoardSizeX * EffectiveCellSize;
	const float Height = BoardSizeY * EffectiveCellSize;
	const float Left = -Width * 0.5f;
	const float Bottom = -Height * 0.5f;
	const float Thickness = FMath::Clamp(EffectiveCellSize * 0.035f, 1.0f, 3.0f);

	for (int32 Y = 0; Y <= BoardSizeY; ++Y)
	{
		FTransform Line;
		Line.SetLocation(FVector(0.f, Bottom + Y * EffectiveCellSize, 0.f));
		Line.SetScale3D(FVector(Width / 100.f, Thickness / 100.f, 0.01f));
		BoardGridInstances->AddInstance(Line);
	}
	for (int32 X = 0; X <= BoardSizeX; ++X)
	{
		FTransform Line;
		Line.SetLocation(FVector(Left + X * EffectiveCellSize, 0.f, 0.f));
		Line.SetScale3D(FVector(Thickness / 100.f, Height / 100.f, 0.01f));
		BoardGridInstances->AddInstance(Line);
	}
}

void AGomokuBoardActor::AddBlockedCell(int32 X, int32 Y)
{
	if (!BlockedCellInstances || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return;
	}
	const float PadSize = GetEffectiveCellSize() * 0.82f;
	FTransform Pad;
	Pad.SetLocation(GridToWorld(X, Y) + FVector(0.f, 0.f, -2.f));
	Pad.SetScale3D(FVector(PadSize / 100.f, PadSize / 100.f, 0.025f));
	BlockedCellInstances->AddInstance(Pad);
}

void AGomokuBoardActor::FitCameraToBoard()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalController() || !BoardCamera)
	{
		return;
	}

	const float EffectiveCellSize = GetEffectiveCellSize();
	const float OrthoWidth = FMath::Max(BoardSizeX, BoardSizeY) * EffectiveCellSize * 1.1f;
	BoardCamera->SetOrthoWidth(OrthoWidth);
	PC->SetViewTarget(this);
}

float AGomokuBoardActor::GetEffectiveCellSize() const
{
	return FMath::Max(CellSize, 1.0f);
}

FVector AGomokuBoardActor::GetStoneVisualScale() const
{
	const float DiameterScale = (GetEffectiveCellSize() * 0.72f) / 100.f;
	return FVector(DiameterScale, DiameterScale, 0.12f);
}

FVector AGomokuBoardActor::GetBoardOrigin() const
{
	const float EffectiveCellSize = GetEffectiveCellSize();
	return FVector(
		-((BoardSizeX - 1) * EffectiveCellSize) * 0.5f,
		-((BoardSizeY - 1) * EffectiveCellSize) * 0.5f,
		0.f);
}

FVector AGomokuBoardActor::GridToWorld(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return GetBoardOrigin();
	}
	const float EffectiveCellSize = GetEffectiveCellSize();
	const FVector Origin = GetBoardOrigin();
	return FVector(Origin.X + X * EffectiveCellSize, Origin.Y + Y * EffectiveCellSize, 5.f);
}

bool AGomokuBoardActor::WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const
{
	const float EffectiveCellSize = GetEffectiveCellSize();
	const FVector Origin = GetBoardOrigin();
	const float DX = (WorldLoc.X - Origin.X) / EffectiveCellSize;
	const float DY = (WorldLoc.Y - Origin.Y) / EffectiveCellSize;
	const int32 GX = FMath::RoundToInt(DX);
	const int32 GY = FMath::RoundToInt(DY);
	if (GX < 0 || GY < 0 || GX >= BoardSizeX || GY >= BoardSizeY)
	{
		return false;
	}
	const FVector Center = GridToWorld(GX, GY);
	const float Dist2D = FVector(WorldLoc.X - Center.X, WorldLoc.Y - Center.Y, 0.f).Size();
	if (Dist2D > EffectiveCellSize * 0.5f)
	{
		return false;
	}
	OutX = GX;
	OutY = GY;
	return true;
}

int32 AGomokuBoardActor::AddStoneAt(int32 X, int32 Y, int32 PlayerId)
{
	UInstancedStaticMeshComponent* Component = GetStoneComponentForPlayer(PlayerId);
	if (!Component || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
	{
		return INDEX_NONE;
	}
	FTransform T;
	T.SetLocation(GridToWorld(X, Y));
	const FVector TargetScale = GetStoneVisualScale();
	T.SetScale3D(TargetScale * 0.01f);
	const int32 Index = Component->AddInstance(T);
	FGomokuAnimatingStone Animation;
	Animation.Component = Component;
	Animation.InstanceIndex = Index;
	Animation.TargetScale = TargetScale;
	AnimatingStones.Add(Animation);
	return Index;
}

void AGomokuBoardActor::ClearStones()
{
	AnimatingStones.Reset();
	UInstancedStaticMeshComponent* StoneComponents[] = {
		StoneInstances.Get(), StoneInstancesPlayer2.Get(), StoneInstancesPlayer3.Get(), StoneInstancesPlayer4.Get()
	};
	for (UInstancedStaticMeshComponent* Component : StoneComponents)
	{
		if (Component)
		{
			Component->ClearInstances();
		}
	}
}

void AGomokuBoardActor::HandleStonePlaced(const FIntPoint& Cell)
{
	AddStoneAt(Cell.X, Cell.Y, GetPlayerIdAtCell(Cell));
}

void AGomokuBoardActor::HandleMatchRestarted()
{
	ClearStones();
}

void AGomokuBoardActor::HandleReplicatedBoardChanged()
{
	RefreshFromReplicatedBoard();
}

void AGomokuBoardActor::RefreshFromReplicatedBoard()
{
	if (!GetWorld())
	{
		return;
	}

	const AGomokuGameState* GS = GetWorld()->GetGameState<AGomokuGameState>();
	if (!GS || GS->ReplicatedBoardSizeX <= 0 || GS->ReplicatedBoardSizeY <= 0)
	{
		return;
	}

	const bool bMiniGame = GS->MatchPhase == EMatchPhase::MiniGamePlaying || GS->MatchPhase == EMatchPhase::MiniGameResult;
	const int32 SourceSizeX = bMiniGame ? 7 : GS->ReplicatedBoardSizeX;
	const int32 SourceSizeY = bMiniGame ? 7 : GS->ReplicatedBoardSizeY;
	const TArray<ECellState>& SourceCells = bMiniGame ? GS->MiniGamePuzzleCells : GS->ReplicatedBoardCells;
	if (SourceSizeX <= 0 || SourceSizeY <= 0)
	{
		return;
	}

	ApplyBoardSize(SourceSizeX, SourceSizeY);
	ClearStones();
	for (int32 Index = 0; Index < SourceCells.Num(); ++Index)
	{
		const ECellState State = SourceCells[Index];
		if (State < ECellState::Player1 || State > ECellState::Player4)
		{
			if (State == ECellState::Blocked)
			{
				const int32 X = Index % SourceSizeX;
				const int32 Y = Index / SourceSizeX;
				AddBlockedCell(X, Y);
			}
			continue;
		}

		const int32 X = Index % SourceSizeX;
		const int32 Y = Index / SourceSizeX;
		AddStoneAt(X, Y, UGomokuRuleEngine::CellStateToPlayerId(State));
	}
}

bool AGomokuBoardActor::ScreenToGrid(APlayerController* PlayerController, const FVector2D& ScreenPosition, FIntPoint& OutCell) const
{
	if (!PlayerController || !GetWorld())
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PlayerController, ScreenPosition, WorldOrigin, WorldDirection))
	{
		return false;
	}

	FVector HitLocation;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GomokuBoardScreenToGrid), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, WorldOrigin + WorldDirection * 10000.f, ECC_Visibility, Params))
	{
		HitLocation = Hit.ImpactPoint;
	}
	else
	{
		if (FMath::Abs(WorldDirection.Z) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float Distance = -WorldOrigin.Z / WorldDirection.Z;
		if (Distance < 0.f)
		{
			return false;
		}
		HitLocation = WorldOrigin + WorldDirection * Distance;
	}

	return WorldToGrid(HitLocation, OutCell.X, OutCell.Y);
}

void AGomokuBoardActor::ConfigureStoneComponent(UInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component || !Component->GetMaterial(0))
	{
		return;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Component->GetMaterial(0), this);
	if (!Material)
	{
		return;
	}
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Component->SetMaterial(0, Material);
	RuntimeMaterials.Add(Material);
}

UInstancedStaticMeshComponent* AGomokuBoardActor::GetStoneComponentForPlayer(int32 PlayerId) const
{
	switch (PlayerId)
	{
	case 2: return StoneInstancesPlayer2;
	case 3: return StoneInstancesPlayer3;
	case 4: return StoneInstancesPlayer4;
	default: return StoneInstances;
	}
}

int32 AGomokuBoardActor::GetPlayerIdAtCell(const FIntPoint& Cell) const
{
	const AGomokuGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGomokuGameState>() : nullptr;
	if (!GS)
	{
		return 1;
	}
	if (const UGomokuRuleEngine* Engine = GS->GetRuleEngine())
	{
		return FMath::Max(1, UGomokuRuleEngine::CellStateToPlayerId(Engine->GetCellState(Cell.X, Cell.Y)));
	}
	if (GS->ReplicatedBoardSizeX > 0)
	{
		const int32 Index = Cell.Y * GS->ReplicatedBoardSizeX + Cell.X;
		if (GS->ReplicatedBoardCells.IsValidIndex(Index))
		{
			return FMath::Max(1, UGomokuRuleEngine::CellStateToPlayerId(GS->ReplicatedBoardCells[Index]));
		}
	}
	return 1;
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

	FIntPoint Cell;
	if (!ScreenToGrid(PC, FVector2D(ScreenX, ScreenY), Cell))
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
	GSState->HandlePlaceStone(PlayerIndex, Cell);
}
