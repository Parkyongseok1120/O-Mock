// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GomokuMainMenuHUD.generated.h"

class UGomokuMainMenuWidget;

UCLASS()
class O_MOCK_API AGomokuMainMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGomokuMainMenuHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Gomoku|UI")
	TSubclassOf<UGomokuMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGomokuMainMenuWidget> MainMenuWidget;
};
