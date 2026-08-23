// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GomokuTypes.h"
#include "GomokuBoardActor.generated.h"

class USceneComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USkyAtmosphereComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
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

	/** Smallest orbit-arm multiplier allowed by wheel zoom. Lower values permit a closer view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Camera", meta = (ClampMin = "0.20", ClampMax = "1.00"))
	float MinimumCameraDistanceMultiplier = 0.42f;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> BoardFrameInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> HoverCellInstances;

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

	/** Orbit the perspective camera around the board. Positive yaw turns clockwise. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Camera")
	void OrbitCamera(float YawDelta, float PitchDelta);

	/** Zoom the orbit camera. Positive input moves closer. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Camera")
	void ZoomCamera(float ZoomInput);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Camera")
	void ResetCameraView();

	/** Converts a screen position to a board coordinate, including a Z-plane fallback when mesh collision is unavailable. */
	bool ScreenToGrid(APlayerController* PlayerController, const FVector2D& ScreenPosition, FIntPoint& OutCell) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UStaticMeshComponent> BoardPlane;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UStaticMeshComponent> BoardPedestal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UStaticMeshComponent> GroundPlane;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UInstancedStaticMeshComponent> HillInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UInstancedStaticMeshComponent> TreeTrunkInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UInstancedStaticMeshComponent> TreeCanopyInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Environment")
	TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

	int32 BoardSizeX = 15;
	int32 BoardSizeY = 15;

private:
	FVector GetBoardOrigin() const;
	float GetEffectiveCellSize() const;
	FVector GetStoneVisualScale() const;
	void RebuildBoardGrid();
	void RebuildEnvironmentScenery();
	void AddBlockedCell(int32 X, int32 Y);
	void RefreshFromReplicatedBoard();
	void UpdateHoverIndicator();
	void ConfigureStoneComponent(UInstancedStaticMeshComponent* Component, const FLinearColor& Color);
	UInstancedStaticMeshComponent* GetStoneComponentForPlayer(int32 PlayerId) const;
	int32 GetPlayerIdAtCell(const FIntPoint& Cell) const;

	TArray<FGomokuAnimatingStone> AnimatingStones;
	FIntPoint DisplayedHoveredCell = FIntPoint(-1, -1);
	FIntPoint LastCameraViewportSize = FIntPoint::ZeroValue;
	float CameraBaseArmLength = 1600.f;
	float CameraDistanceMultiplier = 1.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterials;

	float StoneAnimSpeed = 4.f;
};
