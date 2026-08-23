// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuMainMenuHUD.h"

#include "GomokuMainMenuWidget.h"
#include "GameFramework/PlayerController.h"

AGomokuMainMenuHUD::AGomokuMainMenuHUD()
{
	MainMenuWidgetClass = UGomokuMainMenuWidget::StaticClass();
}

void AGomokuMainMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	TSubclassOf<UGomokuMainMenuWidget> EffectiveClass = MainMenuWidgetClass;
	if (!EffectiveClass)
	{
		EffectiveClass = UGomokuMainMenuWidget::StaticClass();
	}
	MainMenuWidget = CreateWidget<UGomokuMainMenuWidget>(PlayerController, EffectiveClass);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(100);
	}
}

void AGomokuMainMenuHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
