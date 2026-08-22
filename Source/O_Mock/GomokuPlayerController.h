// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GomokuPlayerController.generated.h"

class AGomokuBoardActor;
class AGomokuGameState;

UCLASS()
class O_MOCK_API AGomokuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGomokuPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void SelectItem(int32 ItemId);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void CancelItemTargeting();

	/** Sends the selected item and target to the server. TargetPlayerIndex is the active-player-list index. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void RequestUseSelectedItem(const FIntPoint& TargetCell, int32 TargetPlayerIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Lobby")
	void SetReadyForLobby(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Lobby")
	void RequestStartLobbyMatch(const FString& MapName = TEXT("/Game/NewWorld"));

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Match")
	void RequestAbandonMatch();

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Items")
	bool bItemTargetingActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Items")
	int32 SelectedItemId = 0;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestPlaceStone(FIntPoint Cell);

	UFUNCTION(Server, Reliable)
	void Server_RequestUseItem(int32 ItemId, FIntPoint TargetCell, int32 TargetPlayerIndex);

	UFUNCTION(Server, Reliable)
	void Server_SetReadyForLobby(bool bReady);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartLobbyMatch(const FString& MapName);

	UFUNCTION(Server, Reliable)
	void Server_SubmitMiniGameAnswer(FIntPoint AnswerCell);

	UFUNCTION(Server, Unreliable)
	void Server_UpdateHoveredCell(FIntPoint Cell, bool bHasValidCell);

	UFUNCTION(Server, Reliable)
	void Server_RequestAbandonMatch();

	UFUNCTION(Server, Reliable)
	void Server_RequestRestartMatch();

	UFUNCTION()
	void HandlePrimaryClick();

	UFUNCTION()
	void HandleRestartKey();

	UFUNCTION()
	void HandleMouseMove(FVector2D MousePos);

	UFUNCTION()
	void OnMouseMoveX(float Value);

	UFUNCTION()
	void OnMouseMoveY(float Value);

	void HandleSelectItem1();
	void HandleSelectItem2();
	void HandleSelectItem3();
	void HandleSelectItem4();
	void HandleSelectItem5();
	void HandleReadyKey();
	void HandleStartMatchKey();
	void ResolveBoardActor();
	int32 ResolveNetworkPlayerIndex() const;

	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<AGomokuBoardActor> BoardActor;

	// Cached GameState reference for hover/time queries.
	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<AGomokuGameState> GomokuGSState;

	FIntPoint LastReportedHoveredCell = FIntPoint(-1, -1);
	bool bLastReportedHoverWasValid = false;
	int32 LastReportedHoverPlayerIndex = INDEX_NONE;
};
