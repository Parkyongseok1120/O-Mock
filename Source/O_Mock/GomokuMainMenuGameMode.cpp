// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuMainMenuGameMode.h"

#include "GomokuMainMenuHUD.h"

AGomokuMainMenuGameMode::AGomokuMainMenuGameMode()
{
	HUDClass = AGomokuMainMenuHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
