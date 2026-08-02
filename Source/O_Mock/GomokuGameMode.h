// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "GomokuGameMode.generated.h"

class AGomokuGameState;
class AGomokuBoardActor;

UCLASS()
class O_MOCK_API AGomokuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGomokuGameMode();

	UGomokuRuleEngine* GetRuleEngine() const { return RuleEngine; }
	AGomokuGameState* GetGomokuGameState() const;
	AGomokuBoardActor* GetBoardActor() const { return BoardActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void RestartGame();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<UGomokuRuleEngine> RuleEngine;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGomokuBoardActor> BoardActor;
};
