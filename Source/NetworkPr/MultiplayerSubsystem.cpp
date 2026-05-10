#include "MultiplayerSubsystem.h"
#include "Online/OnlineSessionNames.h"

UMultiplayerSubsystem::UMultiplayerSubsystem()
{
    // Initialises default state variables for session management
    CreateServerAfterDestroy = false;
    DestroyServerName = "";
    ServerNameTofind = "";
    MySessionName = FName("NetworkPr Session Name"); // TODO: WRITE A PROPER SESSION
    
    OnlineSubSystem = nullptr;
}

void UMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    OnlineSubSystem = IOnlineSubsystem::Get();
    if (OnlineSubSystem)
    {
#if 1
       FName SubsystemName = OnlineSubSystem->GetSubsystemName();
       PrintString("Subsystem name is " + SubsystemName.ToString());
#endif
       
       // Gets the session interface and registers internal delegates 
       // to handle asynchronous session events such as creation, destruction, 
       // discovery, and joining.
       SessionInterface = OnlineSubSystem->GetSessionInterface();
       if (SessionInterface)
       {
          PrintString("Session Interface is valid");

          SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this,
             &UMultiplayerSubsystem::OnCreateSessionComplete);

          SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this,
             &UMultiplayerSubsystem::OnDestroySessionComplete);

          SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this,
             &UMultiplayerSubsystem::OnFindSessionsComplete);

          SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this,
             &UMultiplayerSubsystem::OnJoinSessionsComplete);
       }
    }
}

void UMultiplayerSubsystem::Deinitialize()
{
    //UE_LOG(LogTemp, Warning, TEXT("MSS Deinitialize"));
    //Super::Deinitialize();
}

void UMultiplayerSubsystem::PrintString(const FString& String) 
{
    if(GEngine)
       GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, String);
}

void UMultiplayerSubsystem::CreateServer(FString ServerName)
{
    PrintString("Create Server");

    if (ServerName.IsEmpty())
    {
       PrintString("Server Name cannot be empty");
       ServerCreateDel.Broadcast(false);
       return;
    }
    
    // Checks if a session with this name already exists. If found, it destroys 
    // the existing session and sets a flag to trigger recreation immediately 
    // after the destruction process completes.
    FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(MySessionName);
    if (ExistingSession)
    {
       FString Msg = FString::Printf(TEXT("Session with name %s already exists, destroying it"),
          * MySessionName.ToString());
       PrintString(Msg);
       CreateServerAfterDestroy = true;
       DestroyServerName = ServerName;
       SessionInterface->DestroySession(MySessionName);
       return;
    }

    // Configures the session settings such as player count and advertising.
    // Logic specifically checks for the NULL subsystem to enable LAN matches,
    // while other subsystems (like Steam) will default to internet connections.
    FOnlineSessionSettings SessionSettings;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bIsDedicated = false;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.NumPublicConnections = 2;
    SessionSettings.bUseLobbiesIfAvailable = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bAllowJoinViaPresence = true;
    
    bool IsLAN = false;
    if (OnlineSubSystem->GetSubsystemName() == "NULL")
    {
       IsLAN = true; 
    }
    SessionSettings.bIsLANMatch = IsLAN; 
    SessionSettings.Set(FName("SERVER_NAME"), ServerName,
       EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    
    SessionInterface->CreateSession(0, MySessionName, SessionSettings);
}

void UMultiplayerSubsystem::FindServer(FString ServerName)
{
    PrintString("Find Server");

    if (ServerName.IsEmpty())
    {
       PrintString("Server Name cannot be empty");
       ServerJoinDel.Broadcast(false);
       return;    
    }

    // Initialises the session search object and configures query settings.
    // Distinguishes between LAN and online queries based on the active subsystem
    // before executing the search.
    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    bool IsLAN = false;
    if (OnlineSubSystem->GetSubsystemName() == "NULL")
    {
       IsLAN = true; 
    }
    SessionSearch->bIsLanQuery = IsLAN;
    SessionSearch->MaxSearchResults = 9999;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, 
       true, EOnlineComparisonOp::Equals);

    ServerNameTofind = ServerName;
    
    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UMultiplayerSubsystem::OnCreateSessionComplete(FName SessionName, bool WasSuccessful)
{
    PrintString(FString::Printf(TEXT("On Create Session Complete: %d"), WasSuccessful));
    ServerCreateDel.Broadcast(WasSuccessful);
    
    // Performs a server travel to the gameplay map upon successful session creation.
    // The "?listen" parameter ensures the map opens as a listen server.
    if (WasSuccessful)
    {
       FString Path = "/Game/Scenes/ThirdPersonMap?listen";

      if (!GameMapPath.IsEmpty())
      {
         Path = FString::Printf(TEXT("%s?Listen"), *GameMapPath);
      }
       
       GetWorld()->ServerTravel(Path); 
    }
}

void UMultiplayerSubsystem::OnDestroySessionComplete(FName SessionName, bool WasSuccessful)
{
    FString Msg = FString::Printf(TEXT("On Destroy Session Name %s Complete: %d"),
       *SessionName.ToString(), WasSuccessful);
    //PrintString(Msg);

    // Checks if a new server creation was requested to occur immediately 
    // after this destruction, and triggers it if necessary.
    if (CreateServerAfterDestroy)
    {
       CreateServerAfterDestroy = false;
       CreateServer(DestroyServerName);  
    }
}

void UMultiplayerSubsystem::OnFindSessionsComplete(bool WasSuccessful)
{
    PrintString(FString::Printf(TEXT("On Find Session Complete: %d"), WasSuccessful));

    if (!WasSuccessful) return;
    if (ServerNameTofind.IsEmpty()) return;
    
    // Iterates through search results to find a session matching the specific 
    // server name. If a match is found, attempts to join the session; 
    // otherwise, broadcasts a failure event.
    TArray<FOnlineSessionSearchResult> Results = SessionSearch->SearchResults;
    FOnlineSessionSearchResult* CorrectResult = 0;
    if (Results.Num() > 0)
    {
       FString Msg = FString::Printf(TEXT("%d sessions found"), Results.Num());
       //PrintString(Msg);

       for (FOnlineSessionSearchResult Result : Results)
       {
          if (Result.IsValid())
          {
             FString ServerName = "No-name";
             Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName  );

             if (ServerName.Equals(ServerNameTofind))
             {
                CorrectResult = &Result;
                FString Msg2 = FString::Printf(TEXT("Found server with name: %s"), *ServerName);
                //PrintString(Msg2);
                break;
             }
          }
       }
       if (CorrectResult)
       {
          CorrectResult->Session.SessionSettings.bUsesPresence = true;
          CorrectResult->Session.SessionSettings.bUseLobbiesIfAvailable = true;
          SessionInterface->JoinSession(0, MySessionName, *CorrectResult);
       }
       else
       {
          PrintString(FString::Printf(TEXT("Couldn't find server with name: %s"), *ServerNameTofind));
          ServerNameTofind = "";
          ServerJoinDel.Broadcast(false);
       }
    }
    else
    {
       PrintString("Zero sessions found");
       ServerJoinDel.Broadcast(false);
    }
}

void UMultiplayerSubsystem::OnJoinSessionsComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    ServerJoinDel.Broadcast(Result == EOnJoinSessionCompleteResult::Success);
    
    // Finalises the connection process. If the join was successful, retrieves 
    // the connection string (IP address) and instructs the player controller 
    // to travel to the server.
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
       FString Msg = FString::Printf(TEXT("Succsefully joined session %s"), *SessionName.ToString());
       //PrintString(Msg);

       FString Adress = "";
       bool Success = SessionInterface->GetResolvedConnectString(SessionName, Adress);
       if (Success)
       {
          //PrintString(FString::Printf(TEXT("Adress: %s"), *Adress));
          APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
          if (PlayerController)
          {
             PlayerController->ClientTravel(Adress, TRAVEL_Absolute);
          }
       }
       else
       {
          PrintString("GetResolvedConnectString returned false!");
       }
    }
    else
    {
       PrintString(FString::Printf(TEXT("On JoinSession Complete Failed, the state is: %s"),
          LexToString(Result)));
    }
}
