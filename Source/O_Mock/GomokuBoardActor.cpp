// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuBoardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GomokuGameState.h"

AGomokuBoardActor::AGomokuBoardActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    BoardPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardPlane"));
    BoardPlane->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    StoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneInstances"));
    StoneInstances->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
}

void AGomokuBoardActor::BeginPlay()
{
    Super::BeginPlay();
    // BoardSizeX/Y can be overridden via Blueprint or set by GameMode/GameState.
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
        return GetBoardOrigin();

    FVector Origin = GetBoardOrigin();
    return FVector(
        Origin.X + X * CellSize,
        Origin.Y + Y * CellSize,
        5.f); // slight Z offset for stones
}

bool AGomokuBoardActor::WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const
{
    FVector Origin = GetBoardOrigin();
    float DX = (WorldLoc.X - Origin.X) / CellSize;
    float DY = (WorldLoc.Y - Origin.Y) / CellSize;

    int32 GX = FMath::RoundToInt(DX);
    int32 GY = FMath::RoundToInt(DY);

    if (GX < 0 || GY < 0 || GX >= BoardSizeX || GY >= BoardSizeY)
        return false;

    // Ensure click is within half-cell radius
    FVector Center = GridToWorld(GX, GY);
    float Dist2D = FVector(WorldLoc.X - Center.X, WorldLoc.Y - Center.Y, 0.f).Size();
    if (Dist2D > CellSize * 0.5f)
        return false;

    OutX = GX;
    OutY = GY;
    return true;
}

int32 AGomokuBoardActor::AddStoneAt(int32 X, int32 Y)
{
    if (!StoneInstances || X < 0 || Y < 0 || X >= BoardSizeX || Y >= BoardSizeY)
        return INDEX_NONE;

    FVector Pos = GridToWorld(X, Y);
    FTransform T;
    T.SetLocation(Pos);
    T.SetScale3D(FVector(0.01f)); // start small for animation
    int32 Index = StoneInstances->AddInstance(T);
    return Index;
}

void AGomokuBoardActor::ClearStones()
{
    if (!StoneInstances) return;
    StoneInstances->ClearInstances();
}

void AGomokuBoardActor::OnScreenClick(int32 ScreenX, int32 ScreenY)
{
    if (!GetWorld()) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    FVector WorldOrigin, WorldDir;
    if (!UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(ScreenX, ScreenY), WorldOrigin, WorldDir))
        return;

    FHitResult Hit;
    FCollisionQueryParams QP(FName(TEXT("BoardClick")), false, this);
    FVector End = WorldOrigin + WorldDir * 5000.f;

    if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, End, ECC_Visibility, QP))
        return;

    int32 GridX = 0, GridY = 0;
    if (!WorldToGrid(Hit.Location, GridX, GridY))
        return;

    AGomokuGameState* GSState = GetWorld()->GetGameState<AGomokuGameState>();
    if (!GSState || !GSState->IsGameActive)
        return;

    int32 PlayerIndex = GSState->CurrentPlayerIndex;
    if (PlayerIndex < 0 || PlayerIndex >= GSState->LocalPlayerCount)
        return;

    FIntPoint Cell(GridX, GridY);
    GSState->HandlePlaceStone(PlayerIndex, Cell);
}
