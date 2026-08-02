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

	/** Convert world location to board grid coordinate (X,Y). Returns false if outside. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	bool WorldToGrid(const FVector& WorldLoc, int32& OutX, int32& OutY) const;

	/** Get world position for a given cell center. */
	UFUNCTION(BlueprintPure, Category = "Gomoku|Board")
	FVector GridToWorld(int32 X, int32 Y) const;

	/** Add stone instance at (X,Y), returns instance index or INDEX_NONE on failure. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	int32 AddStoneAt(int32 X, int32 Y);

	/** Clear all stones from the board. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Stones")
	void ClearStones();

	/** Called by PlayerController on mouse click with screen coordinates. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Input")
	void OnScreenClick(int32 ScreenX, int32 ScreenY);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gomoku|Board")
	TObjectPtr<UStaticMeshComponent> BoardPlane;

	/** Internal board dimensions (synced from RuleEngine/Config). Default 15x15. */
	int32 BoardSizeX = 15;
	int32 BoardSizeY = 15;

private:
	FVector GetBoardOrigin() const;

	UPROPERTY()
	TObjectPtr<class AGomokuGameState> GS;
};
