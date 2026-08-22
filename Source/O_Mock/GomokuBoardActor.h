// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GomokuTypes.h"
#include "GomokuBoardActor.generated.h"

class USceneComponent;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APlayerController;

struct FGomokuAnimatingStone
{
	TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
	int32 InstanceIndex = INDEX_NONE;
	FVector TargetScale = FVector::OneVector;
};

UCLASS()
class O_MOCK_API AGomokuBoardActor : public AActor
{
	GENERATED_BODY()

public:
	AGomokuBoardActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Board", meta = (ClampMin = "1.0"))
	float CellSize = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstancesPlayer2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstancesPlayer3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstancesPlayer4;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> BoardGridInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> BlockedCellInstances;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	FVector GridToWorld(int32 X, int32 Y) const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	int32 AddStoneAt(int32 X, int32 Y, int32 PlayerId = 1);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	void ClearStones();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Input")
	void OnScreenClick(int32 ScreenX, int32 ScreenY);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	void ApplyBoardSize(int32 InSizeX, int32 InSizeY);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Camera")
	void FitCameraToBoard();

	/** Converts a screen position to a board coordinate, including a Z-plane fallback when mesh collision is unavailable. */
	bool ScreenToGrid(APlayerController* PlayerController, const FVector2D& ScreenPosition, FIntPoint& OutCell) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandleStonePlaced(const FIntPoint& Cell);

	UFUNCTION()
	void HandleMatchRestarted();

	UFUNCTION()
	void HandleReplicatedBoardChanged();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Camera")
	TObjectPtr<UCameraComponent> BoardCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UStaticMeshComponent> BoardPlane;

	int32 BoardSizeX = 15;
	int32 BoardSizeY = 15;

private:
	FVector GetBoardOrigin() const;
	float GetEffectiveCellSize() const;
	FVector GetStoneVisualScale() const;
	void RebuildBoardGrid();
	void AddBlockedCell(int32 X, int32 Y);
	void RefreshFromReplicatedBoard();
	void ConfigureStoneComponent(UInstancedStaticMeshComponent* Component, const FLinearColor& Color);
	UInstancedStaticMeshComponent* GetStoneComponentForPlayer(int32 PlayerId) const;
	int32 GetPlayerIdAtCell(const FIntPoint& Cell) const;

	TArray<FGomokuAnimatingStone> AnimatingStones;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterials;

	float StoneAnimSpeed = 4.f;
};
