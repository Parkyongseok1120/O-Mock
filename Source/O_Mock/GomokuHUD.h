// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GomokuGameState.h"
#include "GomokuHUD.generated.h"

class UGomokuHUDWidget;

UCLASS()
class O_MOCK_API AGomokuHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGomokuHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	UPROPERTY(EditDefaultsOnly, Category = "Gomoku|UI")
	TSubclassOf<UGomokuHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGomokuHUDWidget> HUDWidget;

private:
	UFUNCTION()
	void HandleTurnChanged(int32 PlayerIndex, int32 RoundIndex);

	UFUNCTION()
	void HandleMatchEnded(const FGomokuWinResult& WinResult);

	UFUNCTION()
	void HandleMatchRestarted();

	UFUNCTION()
	void HandleRestartKey();

	UFUNCTION()
	void OnTickPlayerTime(int32 PlayerIndex, const FGomokuPlayerTimeState& TimeState);

	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<AGomokuGameState> GomokuGameState;

	FString CurrentTurnText;
	FString GameOverText;
	bool bShowGameOver = false;

	// Time display strings for each player (indexed by PlayerIndex).
	TArray<FString> PlayerTimeStrings;
};
