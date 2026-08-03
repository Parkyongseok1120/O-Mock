// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GomokuTypes.h"
#include "GomokuBoardActor.generated.h"

UCLASS()
class O_MOCK_API AGomokuBoardActor : public AActor
{
	GENERATED_BODY()

public:
	AGomokuBoardActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Board")
	float CellSize = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstances;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	FVector GridToWorld(int32 X, int32 Y) const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	int32 AddStoneAt(int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	void ClearStones();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Input")
	void OnScreenClick(int32 ScreenX, int32 ScreenY);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Board")
	void ApplyBoardSize(int32 InSizeX, int32 InSizeY);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Camera")
	void FitCameraToBoard();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandleStonePlaced(const FIntPoint& Cell);

	UFUNCTION()
	void HandleMatchRestarted();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UStaticMeshComponent> BoardPlane;

	int32 BoardSizeX = 15;
	int32 BoardSizeY = 15;

private:
	FVector GetBoardOrigin() const;

	UPROPERTY()
	TArray<int32> AnimatingInstanceIndices;

	float StoneAnimSpeed = 4.f;
};
