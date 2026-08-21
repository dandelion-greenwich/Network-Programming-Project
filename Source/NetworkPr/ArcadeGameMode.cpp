// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcadeGameMode.h"
#include "MultiplayerSubsystem.h"
#include "NetworkPrGameInstance.h"
#include "NetworkPrGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/GameStateBase.h"

#if WITH_GAMELIFT
#include "GameLiftServerSDK.h"
#include "GameLiftServerSDKModels.h"
#endif

DEFINE_LOG_CATEGORY(LogGameLift);


void AArcadeGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);
	
	GetWorld()->GetTimerManager().SetTimer(
	TimerHandle,
	this,
	&AArcadeGameMode::TryToStartMatch,
	0.5,false);
}

void AArcadeGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	
	UMultiplayerSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UMultiplayerSubsystem>();
	// SessionInterface stays null when there is no online subsystem, which is the normal case on
	// a dedicated server. Dereferencing it below would crash the moment a player disconnects.
	if (!Subsystem || !Subsystem->SessionInterface.IsValid()) return;

	FName SessionName = Subsystem->MySessionName;
	FNamedOnlineSession* ExistingSession = Subsystem -> SessionInterface->
		GetNamedSession(SessionName);
	
	if (ExistingSession)
	{
		FString Msg = FString::Printf(TEXT("Destroying session: %s"),
		   * SessionName.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, Msg);
		Subsystem -> SessionInterface->DestroySession(SessionName);
		GetWorld()->ServerTravel("/Game/Scenes/MainMenu");
	}

	UGameInstance* GI = GetGameInstance();
	for (int32 i = GI->GetNumLocalPlayers() - 1; i > 0; --i)
	{
		GI->RemoveLocalPlayer(GI->GetLocalPlayerByIndex(i));
	}
}

int32 AArcadeGameMode::GetPlayerSlot(AController* Player) const
{
	if (!Player) return INDEX_NONE;

	UNetworkPrGameInstance* GI = Cast<UNetworkPrGameInstance>(GetGameInstance());
	if (!GI) return INDEX_NONE;

	if (GI->CurrentGameMode == EGameSessionMode::LocalCoop)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				return LP->GetLocalPlayerIndex() + 1;
			}
		}
		return INDEX_NONE;
	}

	AGameStateBase* GS = GetGameState<AGameStateBase>();
	return GS ? GS->PlayerArray.Num() : INDEX_NONE;
}

void AArcadeGameMode::BeginPlay()
{
	Super::BeginPlay();

#if WITH_GAMELIFT
	// Only compiled into Server builds, and the game mode only ever exists on the server anyway,
	// so this can never run on a client.
	InitGameLift();
#endif

	UNetworkPrGameInstance* GI = Cast<UNetworkPrGameInstance>(GetGameInstance());
	if (GI && GI->CurrentGameMode == EGameSessionMode::LocalCoop)
		AddSecondLocalPlayer();
}

void AArcadeGameMode::InitGameLift()
{
#if WITH_GAMELIFT
	UE_LOG(LogGameLift, Log, TEXT("Initialising GameLift..."));

	FGameLiftServerSDKModule* GameLiftSdkModule =
		&FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));

	// An Anywhere fleet passes the compute's identity on the command line. A managed EC2 fleet
	// supplies the same values through environment variables, so the struct is left empty there
	// and the SDK picks them up itself.
	FServerParameters ServerParameters;

	if (FParse::Param(FCommandLine::Get(), TEXT("glAnywhere")))
	{
		UE_LOG(LogGameLift, Log, TEXT("Configuring server parameters for an Anywhere fleet"));

		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereWebSocketUrl="), ServerParameters.m_webSocketUrl);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereFleetId="), ServerParameters.m_fleetId);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereHostId="), ServerParameters.m_hostId);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereAuthToken="), ServerParameters.m_authToken);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereAwsRegion="), ServerParameters.m_awsRegion);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereAccessKey="), ServerParameters.m_accessKey);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereSecretKey="), ServerParameters.m_secretKey);
		FParse::Value(FCommandLine::Get(), TEXT("glAnywhereSessionToken="), ServerParameters.m_sessionToken);

		if (!FParse::Value(FCommandLine::Get(), TEXT("glAnywhereProcessId="), ServerParameters.m_processId))
		{
			// GameLift needs a process id that is unique per process on the compute
			ServerParameters.m_processId = FString::Printf(
				TEXT("ProcessId_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S")));
		}

		UE_LOG(LogGameLift, Log, TEXT("  WebSocket URL: %s"), *ServerParameters.m_webSocketUrl);
		UE_LOG(LogGameLift, Log, TEXT("  Fleet ID:      %s"), *ServerParameters.m_fleetId);
		UE_LOG(LogGameLift, Log, TEXT("  Host ID:       %s"), *ServerParameters.m_hostId);
		UE_LOG(LogGameLift, Log, TEXT("  Process ID:    %s"), *ServerParameters.m_processId);
		UE_LOG(LogGameLift, Log, TEXT("  AWS Region:    %s"), *ServerParameters.m_awsRegion);
		// Auth token, access key, secret key and session token are deliberately not logged.
		// GameLift uploads these log files to S3 at the end of a session.
	}

	FGameLiftGenericOutcome InitOutcome = GameLiftSdkModule->InitSDK(ServerParameters);
	if (!InitOutcome.IsSuccess())
	{
		UE_LOG(LogGameLift, Error, TEXT("InitSDK failed: %s"), *InitOutcome.GetError().m_errorMessage);
		return;
	}
	UE_LOG(LogGameLift, Log, TEXT("InitSDK succeeded"));

	ProcessParameters = MakeShared<FProcessParameters>();

	// GameLift has placed a game session on this process. Once we are able to accept players,
	// we tell it so by calling ActivateGameSession.
	ProcessParameters->OnStartGameSession.BindLambda(
		[GameLiftSdkModule](Aws::GameLift::Server::Model::GameSession InGameSession)
		{
			UE_LOG(LogGameLift, Log, TEXT("Game session starting: %s"),
				*FString(InGameSession.GetGameSessionId()));
			GameLiftSdkModule->ActivateGameSession();
		});

	// GameLift is about to shut this process down. One process hosts one game session, so the
	// process ends here rather than travelling back to the menu the way the Steam path does.
	ProcessParameters->OnTerminate.BindLambda(
		[GameLiftSdkModule]()
		{
			UE_LOG(LogGameLift, Log, TEXT("Server process is terminating"));

			FGameLiftGenericOutcome ProcessEndingOutcome = GameLiftSdkModule->ProcessEnding();
			if (!ProcessEndingOutcome.IsSuccess())
			{
				UE_LOG(LogGameLift, Error, TEXT("ProcessEnding failed: %s"),
					*ProcessEndingOutcome.GetError().m_errorMessage);
			}

			FGameLiftGenericOutcome DestroyOutcome = GameLiftSdkModule->Destroy();
			if (!DestroyOutcome.IsSuccess())
			{
				UE_LOG(LogGameLift, Error, TEXT("Destroy failed: %s"),
					*DestroyOutcome.GetError().m_errorMessage);
			}
		});

	// Polled roughly every 60 seconds. GameLift assumes false if we do not answer in time.
	ProcessParameters->OnHealthCheck.BindLambda(
		[]()
		{
			return true;
		});

	// The port players connect on. -port= on the command line wins, so a fleet can run several
	// processes on one compute.
	ProcessParameters->port = FURL::UrlConfig.DefaultPort;
	int32 PortOverride = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("port="), PortOverride))
	{
		ProcessParameters->port = PortOverride;
	}

	// Paths are relative to the root of the packaged server build. GameLift uploads whatever it
	// finds here to S3 when the session ends, which is how you read the log after the fact.
	ProcessParameters->logParameters = TArray<FString>{ TEXT("NetworkPr/Saved/Logs/server.log") };

	UE_LOG(LogGameLift, Log, TEXT("Calling ProcessReady on port %d..."), ProcessParameters->port);
	FGameLiftGenericOutcome ProcessReadyOutcome = GameLiftSdkModule->ProcessReady(*ProcessParameters);
	if (ProcessReadyOutcome.IsSuccess())
	{
		UE_LOG(LogGameLift, Log, TEXT("Process Ready!"));
	}
	else
	{
		UE_LOG(LogGameLift, Error, TEXT("ProcessReady failed: %s"),
			*ProcessReadyOutcome.GetError().m_errorMessage);
	}
#endif
}

void AArcadeGameMode::AddSecondLocalPlayer()
{
	UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
}

void AArcadeGameMode::TryToStartMatch()
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();
    
	if (GS && GS->Player1 && GS->Player2)
	{
		OnStartMatch.Broadcast();
		GS -> Multicast_Play();
	}
	else if (GS && GS->Player1 && !GS->Player2)
	{
		GS -> Multicast_Wait();
	}
}

void AArcadeGameMode::WaitForTheSecondPlayer()
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();
	if (GS) GS->Multicast_Wait();
}

void AArcadeGameMode::ContinueGame()
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();
	if (GS) GS -> Multicast_Play();
}

void AArcadeGameMode::GameOver(AActor* DeadPlayer)
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();
	if (GS)
	{
		GS -> SetAllPlayersToBeInvincible();
		GS -> Multicast_GameOver();
	}
}



