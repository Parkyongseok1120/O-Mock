// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GomokuMinigameTypes.h"
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
	bool SelectItem(int32 ItemId);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void CancelItemTargeting();

	/** Sends the selected item and target to the server. TargetPlayerIndex is the active-player-list index. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void RequestUseSelectedItem(const FIntPoint& TargetCell, int32 TargetPlayerIndex = -1);

	/** Chooses an existing slot to discard when the server has offered a new item to a full inventory. */
	UFUNCTION(BlueprintCallable, Category = "Gomoku|Items")
	void RequestReplacePendingInventoryItem(int32 DiscardItemId);

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

	/** Last inventory action/validation message presented by the interactive HUD. */
	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Items")
	FString ItemFeedbackText;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Items")
	bool bLastItemFeedbackSuccess = false;

	/** Prevents a second click from submitting the same item before the reliable server result returns. */
	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|Items")
	bool bItemUseRequestPending = false;

	/** Mouse-drag yaw degrees applied per raw mouse delta unit. Editable on controller class defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Camera", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float CameraOrbitYawSensitivity = 0.85f;

	/** Mouse-drag pitch degrees applied per raw mouse delta unit. Editable on controller class defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gomoku|Camera", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float CameraOrbitPitchSensitivity = 0.70f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestPlaceStone(FIntPoint Cell);

	UFUNCTION(Server, Reliable)
	void Server_RequestUseItem(int32 ItemId, FIntPoint TargetCell, int32 TargetPlayerIndex);

	UFUNCTION(Client, Reliable)
	void Client_NotifyItemUseResult(bool bSuccess, int32 ItemId, bool bCanRetryTarget);

	UFUNCTION(Server, Reliable)
	void Server_RequestReplacePendingInventoryItem(int32 DiscardItemId);

	UFUNCTION(Client, Reliable)
	void Client_NotifyInventoryReplaceResult(bool bSuccess, int32 DiscardItemId, int32 NewItemId);

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
	void HandleCameraZoom(float Value);
	void HandleResetCameraKey();
	void ResolveBoardActor();
	int32 ResolveNetworkPlayerIndex() const;
	bool CanSelectItemForCurrentPlayer(int32 ItemId, FString& OutReason) const;
	void ClearItemSelection();
	void SetItemFeedback(const FString& Message, bool bSuccess);

	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<AGomokuBoardActor> BoardActor;

	// Cached GameState reference for hover/time queries.
	UPROPERTY(VisibleAnywhere, Category = "Gomoku")
	TObjectPtr<AGomokuGameState> GomokuGSState;

	FIntPoint LastReportedHoveredCell = FIntPoint(-1, -1);
	bool bLastReportedHoverWasValid = false;
	int32 LastReportedHoverPlayerIndex = INDEX_NONE;
	int32 LastObservedTurnIndex = INDEX_NONE;
	EMatchPhase LastObservedMatchPhase = EMatchPhase::Waiting;
};
