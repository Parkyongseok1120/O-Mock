// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GomokuRuleEngine.h"
#include "GomokuTypes.h"
#include "GomokuBoardTemplateDataAsset.h"
#include "GomokuGameMode.generated.h"

class AGomokuGameState;
class AGomokuBoardActor;

UCLASS()
class O_MOCK_API AGomokuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGomokuGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gomoku|Hotseat", meta = (ClampMin = 2, ClampMax = 4))
	int32 DefaultHotseatPlayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	TObjectPtr<UGomokuBoardTemplateDataAsset> BoardTemplate;

	UGomokuRuleEngine* GetRuleEngine() const { return RuleEngine; }
	AGomokuGameState* GetGomokuGameState() const;
	AGomokuBoardActor* GetBoardActor() const { return BoardActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void RestartGame();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToMatch(const FString& MapName = TEXT("/Game/Maps/GomokuMatch"));

	/** Apply a board template (e.g. when changed in editor or via BP). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Template")
	void ApplyBoardTemplate();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<UGomokuRuleEngine> RuleEngine;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGomokuBoardActor> BoardActor;
};
