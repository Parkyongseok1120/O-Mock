// Copyright Epic Games, Inc. All Rights Reserved.

#include "GomokuGameInstance.h"

#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Hash/Blake3.h"
#include "Misc/Guid.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGomokuSessions, Log, All);

static const FName GomokuRoomNameKey(TEXT("ROOM_NAME"));
static const FName GomokuGameMarkerKey(TEXT("GOMOKU_GAME"));
static const FName GomokuBoardSizeKey(TEXT("BOARD_SIZE"));
static const FName GomokuBotCountKey(TEXT("BOT_COUNT"));
static const FName GomokuPersonalTimeKey(TEXT("PERSONAL_TIME"));
static const FName GomokuTurnTimeKey(TEXT("TURN_TIME"));
static const FName GomokuItemsEnabledKey(TEXT("ITEMS_ENABLED"));
static const FName GomokuMiniGameEnabledKey(TEXT("MINIGAME_ENABLED"));
static const FName GomokuPasswordProtectedKey(TEXT("PASSWORD_PROTECTED"));
static const FName GomokuPasswordSaltKey(TEXT("PASSWORD_SALT"));

FString UGomokuGameInstance::BuildPasswordDigest(const FString& Password, const FString& RoomSalt)
{
	FString Normalized = Password;
	Normalized.TrimStartAndEndInline();
	if (Normalized.IsEmpty())
	{
		return FString();
	}
	const FString EffectiveSalt = RoomSalt.IsEmpty() ? TEXT("compatibility") : RoomSalt;
	const FString ChallengeInput = TEXT("O-Mock-LAN-Room-v2:") + EffectiveSalt + TEXT(":") + Normalized;
	FTCHARToUTF8 Utf8(*ChallengeInput);
	return LexToString(FBlake3::HashBuffer(Utf8.Get(), static_cast<uint64>(Utf8.Length())));
}

void UGomokuGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogGomokuSessions, Display, TEXT("LAN game instance initialized: subsystem=%s"),
		IOnlineSubsystem::Get() ? *IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("None"));
	if (GEngine)
	{
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &UGomokuGameInstance::HandleNetworkFailure);
		TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(
			this, &UGomokuGameInstance::HandleTravelFailure);
	}
}

void UGomokuGameInstance::Shutdown()
{
	if (GEngine)
	{
		if (NetworkFailureDelegateHandle.IsValid()) GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
		if (TravelFailureDelegateHandle.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
	}
	NetworkFailureDelegateHandle.Reset();
	TravelFailureDelegateHandle.Reset();
	ClearSessionDelegates();
	SessionSearch.Reset();
	Super::Shutdown();
}

IOnlineSessionPtr UGomokuGameInstance::GetSessionInterface() const
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

void UGomokuGameInstance::HostLanRoom(int32 MaxPlayers)
{
	FGomokuRoomSettings Settings;
	Settings.MaxPlayers = MaxPlayers;
	HostCustomLanRoom(Settings);
}

void UGomokuGameInstance::HostCustomLanRoom(const FGomokuRoomSettings& Settings)
{
	PendingRoomSettings = Settings;
	PendingRoomSettings.Sanitize();
	PendingRoomPasswordSalt = PendingRoomSettings.Password.IsEmpty()
		? FString()
		: FGuid::NewGuid().ToString(EGuidFormats::Digits);
	PendingHostMaxPlayers = PendingRoomSettings.MaxPlayers;
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		BroadcastStatus(TEXT("LAN session service is unavailable."), false);
		return;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bCreateAfterDestroy = true;
		BroadcastStatus(TEXT("Closing the previous room..."), true);
		DestroySessionDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleDestroySessionComplete));
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
			DestroySessionDelegateHandle.Reset();
			bCreateAfterDestroy = false;
			BroadcastStatus(TEXT("Could not close the previous room."), false);
		}
		return;
	}

	CreatePendingRoom();
}

void UGomokuGameInstance::CreatePendingRoom()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		BroadcastStatus(TEXT("LAN session service is unavailable."), false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = PendingHostMaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = true;
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = false;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinViaPresence = false;
	Settings.bUseLobbiesIfAvailable = false;
	Settings.Set(GomokuGameMarkerKey, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuRoomNameKey,
		FString(FPlatformProcess::ComputerName()),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuBoardSizeKey, PendingRoomSettings.BoardSize,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuBotCountKey, PendingRoomSettings.BotCount,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuPersonalTimeKey, PendingRoomSettings.PersonalTimeSeconds,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuTurnTimeKey, PendingRoomSettings.TurnTimeSeconds,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuItemsEnabledKey, PendingRoomSettings.bItemsEnabled,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuMiniGameEnabledKey, PendingRoomSettings.bMiniGameEnabled,
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GomokuPasswordProtectedKey, !PendingRoomSettings.Password.IsEmpty(),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (!PendingRoomPasswordSalt.IsEmpty())
	{
		Settings.Set(GomokuPasswordSaltKey, PendingRoomPasswordSalt,
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	CreateSessionDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleCreateSessionComplete));
	BroadcastStatus(FString::Printf(TEXT("Creating a LAN room for %d players..."), PendingHostMaxPlayers), true);
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
		BroadcastStatus(TEXT("The LAN room request could not be started."), false);
	}
}

void UGomokuGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && CreateSessionDelegateHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
		CreateSessionDelegateHandle.Reset();
	}
	if (!bWasSuccessful || !Sessions.IsValid())
	{
		BroadcastStatus(TEXT("LAN room creation failed."), false);
		return;
	}

	StartSessionDelegateHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleStartSessionComplete));
	if (!Sessions->StartSession(SessionName))
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartSessionDelegateHandle);
		StartSessionDelegateHandle.Reset();
		BroadcastStatus(TEXT("The LAN room was created but could not be started."), false);
	}
}

void UGomokuGameInstance::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && StartSessionDelegateHandle.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartSessionDelegateHandle);
		StartSessionDelegateHandle.Reset();
	}
	if (!bWasSuccessful || !GetWorld())
	{
		BroadcastStatus(TEXT("The LAN room could not enter the lobby."), false);
		return;
	}

	const FString PasswordDigest = BuildPasswordDigest(PendingRoomSettings.Password, PendingRoomPasswordSalt);
	FString TravelUrl = FString::Printf(
		TEXT("/Game/NewWorld?listen?MaxPlayers=%d?BotCount=%d?BoardSize=%d?PersonalTime=%d?TurnTime=%d?Items=%d?MiniGame=%d?PasswordProtected=%d"),
		PendingRoomSettings.MaxPlayers, PendingRoomSettings.BotCount, PendingRoomSettings.BoardSize,
		PendingRoomSettings.PersonalTimeSeconds, PendingRoomSettings.TurnTimeSeconds,
		PendingRoomSettings.bItemsEnabled ? 1 : 0, PendingRoomSettings.bMiniGameEnabled ? 1 : 0,
		PasswordDigest.IsEmpty() ? 0 : 1);
	if (!PasswordDigest.IsEmpty())
	{
		TravelUrl += FString::Printf(TEXT("?RoomPasswordHash=%s"), *PasswordDigest);
	}
	UE_LOG(LogGomokuSessions, Display,
		TEXT("LAN host ready: session=%s board=%d players=%d bots=%d passwordProtected=%s"),
		*SessionName.ToString(), PendingRoomSettings.BoardSize, PendingRoomSettings.MaxPlayers,
		PendingRoomSettings.BotCount,
		PasswordDigest.IsEmpty() ? TEXT("false") : TEXT("true"));
	BroadcastStatus(TEXT("Room created. Entering the lobby..."), true);
	GetWorld()->ServerTravel(TravelUrl);
}

void UGomokuGameInstance::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && DestroySessionDelegateHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
		DestroySessionDelegateHandle.Reset();
	}
	const bool bShouldCreate = bCreateAfterDestroy;
	bCreateAfterDestroy = false;
	if (bWasSuccessful && bShouldCreate)
	{
		CreatePendingRoom();
	}
	else if (!bWasSuccessful)
	{
		if (bJoinedSessionAsClient && Sessions.IsValid())
		{
			Sessions->RemoveNamedSession(NAME_GameSession);
		}
		BroadcastStatus(FString::Printf(TEXT("Could not close session %s."), *SessionName.ToString()), false);
	}
	bJoinedSessionAsClient = false;
}

void UGomokuGameInstance::FindLanRooms()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		BroadcastStatus(TEXT("LAN session service is unavailable."), false);
		return;
	}
	if (SessionSearch.IsValid() && SessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		BroadcastStatus(TEXT("A LAN room search is already running."), true);
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = 50;
	FindSessionsDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleFindSessionsComplete));
	BroadcastStatus(TEXT("Searching the local network for rooms..."), true);
	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
		SessionSearch.Reset();
		BroadcastStatus(TEXT("The LAN room search could not be started."), false);
	}
}

void UGomokuGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && FindSessionsDelegateHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
		FindSessionsDelegateHandle.Reset();
	}

	DiscoveredRooms.Reset();
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];
			bool bIsGomokuRoom = false;
			Result.Session.SessionSettings.Get(GomokuGameMarkerKey, bIsGomokuRoom);
			const int32 MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			const int32 CurrentPlayers = MaxPlayers - Result.Session.NumOpenPublicConnections;
			if (!bIsGomokuRoom || MaxPlayers < 2 || Result.Session.NumOpenPublicConnections <= 0)
			{
				continue;
			}

			FGomokuRoomInfo Info;
			Info.ResultIndex = Index;
			Info.MaxPlayers = MaxPlayers;
			Info.CurrentPlayers = FMath::Clamp(CurrentPlayers, 0, MaxPlayers);
			Info.PingInMs = Result.PingInMs;
			Result.Session.SessionSettings.Get(GomokuBoardSizeKey, Info.BoardSize);
			Result.Session.SessionSettings.Get(GomokuBotCountKey, Info.BotCount);
			Result.Session.SessionSettings.Get(GomokuPersonalTimeKey, Info.PersonalTimeSeconds);
			Result.Session.SessionSettings.Get(GomokuTurnTimeKey, Info.TurnTimeSeconds);
			Result.Session.SessionSettings.Get(GomokuItemsEnabledKey, Info.bItemsEnabled);
			Result.Session.SessionSettings.Get(GomokuMiniGameEnabledKey, Info.bMiniGameEnabled);
			Result.Session.SessionSettings.Get(GomokuPasswordProtectedKey, Info.bPasswordProtected);
			if (!Result.Session.SessionSettings.Get(GomokuRoomNameKey, Info.RoomName) || Info.RoomName.IsEmpty())
			{
				Info.RoomName = Result.Session.OwningUserName.IsEmpty()
					? TEXT("O-Mock LAN Room")
					: Result.Session.OwningUserName;
			}
			DiscoveredRooms.Add(Info);
		}
	}

	OnRoomsUpdated.Broadcast(DiscoveredRooms);
	BroadcastStatus(bWasSuccessful
		? FString::Printf(TEXT("Found %d joinable LAN room(s)."), DiscoveredRooms.Num())
		: TEXT("LAN room search failed."), bWasSuccessful);
	UE_LOG(LogGomokuSessions, Display, TEXT("LAN search complete: success=%s raw=%d joinable=%d"),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0, DiscoveredRooms.Num());
}

void UGomokuGameInstance::JoinLanRoom(int32 ResultIndex)
{
	JoinLanRoomWithPassword(ResultIndex, FString());
}

void UGomokuGameInstance::JoinLanRoomWithPassword(int32 ResultIndex, const FString& Password)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		BroadcastStatus(TEXT("That room is no longer available. Refresh the room list."), false);
		return;
	}
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[ResultIndex];
	if (!Result.IsValid() || Result.Session.NumOpenPublicConnections <= 0)
	{
		BroadcastStatus(TEXT("That room is full or invalid. Refresh the room list."), false);
		return;
	}
	bool bPasswordProtected = false;
	Result.Session.SessionSettings.Get(GomokuPasswordProtectedKey, bPasswordProtected);
	FString PasswordSalt;
	Result.Session.SessionSettings.Get(GomokuPasswordSaltKey, PasswordSalt);
	PendingJoinPasswordDigest = BuildPasswordDigest(Password, PasswordSalt);
	if (bPasswordProtected && PendingJoinPasswordDigest.IsEmpty())
	{
		BroadcastStatus(TEXT("This room is locked. Enter its password before joining."), false);
		return;
	}
	if (bPasswordProtected && PasswordSalt.IsEmpty())
	{
		BroadcastStatus(TEXT("This locked room uses an unsupported password format. Refresh or recreate it."), false);
		return;
	}

	JoinSessionDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleJoinSessionComplete));
	BroadcastStatus(TEXT("Joining the selected LAN room..."), true);
	if (!Sessions->JoinSession(0, NAME_GameSession, Result))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
		BroadcastStatus(TEXT("The room join request could not be started."), false);
	}
}

void UGomokuGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && JoinSessionDelegateHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		JoinSessionDelegateHandle.Reset();
	}
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid())
	{
		PendingJoinPasswordDigest.Reset();
		BroadcastStatus(FString::Printf(TEXT("Could not join the room: %s"), LexToString(Result)), false);
		return;
	}

	FString ConnectString;
	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!Sessions->GetResolvedConnectString(SessionName, ConnectString) || !PlayerController)
	{
		BroadcastStatus(TEXT("The room joined, but its network address could not be resolved."), false);
		bJoinedSessionAsClient = true;
		CleanupClientSessionAfterFailure(TEXT("The joined room address was invalid. Refresh and try again."));
		return;
	}
	UE_LOG(LogGomokuSessions, Display, TEXT("LAN join resolved: %s"), *ConnectString);
	BroadcastStatus(TEXT("Connected. Entering the lobby..."), true);
	if (!PendingJoinPasswordDigest.IsEmpty())
	{
		ConnectString += FString::Printf(TEXT("?RoomPasswordHash=%s"), *PendingJoinPasswordDigest);
	}
	PendingJoinPasswordDigest.Reset();
	bJoinedSessionAsClient = true;
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UGomokuGameInstance::JoinDirectAddress(const FString& Address)
{
	JoinDirectAddressWithPassword(Address, FString());
}

void UGomokuGameInstance::JoinDirectAddressWithPassword(const FString& Address, const FString& Password)
{
	FString NormalizedPassword = Password;
	NormalizedPassword.TrimStartAndEndInline();
	if (!NormalizedPassword.IsEmpty())
	{
		BroadcastStatus(TEXT("Locked rooms must be joined from the LAN room list so their room salt can be verified."), false);
		return;
	}
	FString NormalizedAddress = Address;
	NormalizedAddress.TrimStartAndEndInline();
	if (NormalizedAddress.IsEmpty())
	{
		BroadcastStatus(TEXT("Enter the host PC's IPv4 address."), false);
		return;
	}
	if (!NormalizedAddress.Contains(TEXT(":")))
	{
		NormalizedAddress += TEXT(":7777");
	}
	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController)
	{
		BroadcastStatus(TEXT("No local player controller is available."), false);
		return;
	}
	BroadcastStatus(FString::Printf(TEXT("Connecting directly to %s..."), *NormalizedAddress), true);
	PlayerController->ClientTravel(NormalizedAddress, TRAVEL_Absolute);
}

void UGomokuGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (bJoinedSessionAsClient)
	{
		CleanupClientSessionAfterFailure(FString::Printf(
			TEXT("Connection failed (%s). The client session was cleared; refresh and try again."),
			ENetworkFailure::ToString(FailureType)));
	}
}

void UGomokuGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (bJoinedSessionAsClient)
	{
		CleanupClientSessionAfterFailure(FString::Printf(
			TEXT("Travel failed (%s). The client session was cleared; refresh and try again."),
			ETravelFailure::ToString(FailureType)));
	}
}

void UGomokuGameInstance::CleanupClientSessionAfterFailure(const FString& UserMessage)
{
	PendingJoinPasswordDigest.Reset();
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		if (DestroySessionDelegateHandle.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
			DestroySessionDelegateHandle.Reset();
		}
		bCreateAfterDestroy = false;
		DestroySessionDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UGomokuGameInstance::HandleDestroySessionComplete));
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
			DestroySessionDelegateHandle.Reset();
			Sessions->RemoveNamedSession(NAME_GameSession);
			bJoinedSessionAsClient = false;
		}
	}
	else
	{
		bJoinedSessionAsClient = false;
	}
	BroadcastStatus(UserMessage, false);
}

void UGomokuGameInstance::SetLanRoomJoinable(bool bJoinable)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}
	FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(NAME_GameSession);
	if (!NamedSession)
	{
		return;
	}
	FOnlineSessionSettings UpdatedSettings = NamedSession->SessionSettings;
	UpdatedSettings.bShouldAdvertise = bJoinable;
	UpdatedSettings.bAllowJoinInProgress = bJoinable;
	Sessions->UpdateSession(NAME_GameSession, UpdatedSettings, true);
	UE_LOG(LogGomokuSessions, Display, TEXT("LAN room joinability changed: %s"), bJoinable ? TEXT("open") : TEXT("closed"));
}

void UGomokuGameInstance::BroadcastStatus(const FString& Message, bool bSuccess)
{
	UE_LOG(LogGomokuSessions, Display, TEXT("%s"), *Message);
	OnSessionStatus.Broadcast(Message, bSuccess);
}

void UGomokuGameInstance::ClearSessionDelegates()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}
	if (CreateSessionDelegateHandle.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
	if (StartSessionDelegateHandle.IsValid()) Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartSessionDelegateHandle);
	if (DestroySessionDelegateHandle.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionDelegateHandle);
	if (FindSessionsDelegateHandle.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsDelegateHandle);
	if (JoinSessionDelegateHandle.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
	CreateSessionDelegateHandle.Reset();
	StartSessionDelegateHandle.Reset();
	DestroySessionDelegateHandle.Reset();
	FindSessionsDelegateHandle.Reset();
	JoinSessionDelegateHandle.Reset();
}
