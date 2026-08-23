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
class APlayerController;
class AController;

UCLASS()
class O_MOCK_API AGomokuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGomokuGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gomoku|Hotseat", meta = (ClampMin = 2, ClampMax = 4))
	int32 DefaultHotseatPlayers = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Gomoku|Lobby", meta = (ClampMin = 2, ClampMax = 4))
	int32 MaxLobbyPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	bool bMatchStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	int32 RequestedBoardSize = 15;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	int32 RequestedBotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	int32 RequestedPersonalTimeSeconds = 120;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	int32 RequestedTurnTimeSeconds = 25;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	bool bRequestedItemsEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	bool bRequestedMiniGameEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Lobby")
	bool bPasswordProtected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Template")
	TObjectPtr<UGomokuBoardTemplateDataAsset> BoardTemplate;

	UGomokuRuleEngine* GetRuleEngine() const { return RuleEngine; }
	AGomokuGameState* GetGomokuGameState() const;
	AGomokuBoardActor* GetBoardActor() const { return BoardActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Gomoku")
	void RestartGame();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToMatch(const FString& MapName = TEXT("/Game/NewWorld"));

	/** Apply a board template (e.g. when changed in editor or via BP). */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Template")
	void ApplyBoardTemplate();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Lobby")
	bool SetPlayerReady(APlayerController* Player, bool bReady);

	UFUNCTION(BlueprintPure, Category = "Gomoku|Lobby")
	bool AreAllPlayersReady() const;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Lobby")
	bool TryStartMatch(APlayerController* RequestingPlayer, const FString& MapName = TEXT("/Game/NewWorld"));

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<UGomokuRuleEngine> RuleEngine;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGomokuBoardActor> BoardActor;

	FString ExpectedRoomPasswordHash;

	bool BuildMatchConfig(FGomokuMatchConfig& OutConfig) const;
	int32 SpawnRequestedBots();
	void InitializeMatchFromSettings();
	AGomokuBoardActor* EnsureBoardActor();
	void ConfigureBoardActor();
};
