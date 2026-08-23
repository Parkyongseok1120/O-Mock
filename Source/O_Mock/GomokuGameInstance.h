// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "GomokuRoomSettings.h"
#include "GomokuGameInstance.generated.h"

class FOnlineSessionSearch;
class UNetDriver;

USTRUCT(BlueprintType)
struct FGomokuRoomInfo
{
	GENERATED_BODY()

	/** Stable index into the most recent search result set. */
	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 ResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 BotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 PingInMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 BoardSize = 15;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 PersonalTimeSeconds = 120;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	int32 TurnTimeSeconds = 25;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	bool bItemsEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	bool bMiniGameEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gomoku|LAN")
	bool bPasswordProtected = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGomokuRoomsUpdated, const TArray<FGomokuRoomInfo>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGomokuSessionStatus, const FString&, Message, bool, bSuccess);

/** Owns LAN session operations so they survive menu-to-lobby travel. */
UCLASS()
class O_MOCK_API UGomokuGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void HostLanRoom(int32 MaxPlayers);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void HostCustomLanRoom(const FGomokuRoomSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void FindLanRooms();

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void JoinLanRoom(int32 ResultIndex);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void JoinLanRoomWithPassword(int32 ResultIndex, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void JoinDirectAddress(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "Gomoku|LAN")
	void JoinDirectAddressWithPassword(const FString& Address, const FString& Password);

	/** Produces the salted BLAKE3 proof used by PreLogin. Empty passwords intentionally produce an empty digest. */
	static FString BuildPasswordDigest(const FString& Password, const FString& RoomSalt = FString());

	/** Stops advertising the room once the host starts the match. */
	void SetLanRoomJoinable(bool bJoinable);

	UFUNCTION(BlueprintPure, Category = "Gomoku|LAN")
	const TArray<FGomokuRoomInfo>& GetDiscoveredRooms() const { return DiscoveredRooms; }

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|LAN")
	FOnGomokuRoomsUpdated OnRoomsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Gomoku|LAN")
	FOnGomokuSessionStatus OnSessionStatus;

private:
	IOnlineSessionPtr GetSessionInterface() const;
	void CreatePendingRoom();
	void BroadcastStatus(const FString& Message, bool bSuccess);
	void ClearSessionDelegates();
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void CleanupClientSessionAfterFailure(const FString& UserMessage);

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FGomokuRoomInfo> DiscoveredRooms;
	FDelegateHandle CreateSessionDelegateHandle;
	FDelegateHandle StartSessionDelegateHandle;
	FDelegateHandle DestroySessionDelegateHandle;
	FDelegateHandle FindSessionsDelegateHandle;
	FDelegateHandle JoinSessionDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
	FDelegateHandle TravelFailureDelegateHandle;
	int32 PendingHostMaxPlayers = 4;
	FGomokuRoomSettings PendingRoomSettings;
	FString PendingRoomPasswordSalt;
	FString PendingJoinPasswordDigest;
	bool bCreateAfterDestroy = false;
	bool bJoinedSessionAsClient = false;
};
