// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcadeGameMode.h"
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
	
	// The Steam session cleanup used to be here. GameLift doesn't need it - the server doesn't
	// own a session to destroy, and it shouldn't go back to the menu either. One process runs
	// one match and then quits.

	// Local co-op still needs its extra players removed.
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
	// Server builds only, and game modes only exist on the server, so clients never see this.
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

	// Anywhere fleets get their details from the command line. On EC2 they come from environment
	// variables instead, so we leave this empty and let the SDK find them.
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
			// Each process on the machine needs its own id
			ServerParameters.m_processId = FString::Printf(
				TEXT("ProcessId_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S")));
		}

		UE_LOG(LogGameLift, Log, TEXT("  WebSocket URL: %s"), *ServerParameters.m_webSocketUrl);
		UE_LOG(LogGameLift, Log, TEXT("  Fleet ID:      %s"), *ServerParameters.m_fleetId);
		UE_LOG(LogGameLift, Log, TEXT("  Host ID:       %s"), *ServerParameters.m_hostId);
		UE_LOG(LogGameLift, Log, TEXT("  Process ID:    %s"), *ServerParameters.m_processId);
		UE_LOG(LogGameLift, Log, TEXT("  AWS Region:    %s"), *ServerParameters.m_awsRegion);
		// Not logging the token or keys on purpose - GameLift uploads these logs to S3.
	}

	FGameLiftGenericOutcome InitOutcome = GameLiftSdkModule->InitSDK(ServerParameters);
	if (!InitOutcome.IsSuccess())
	{
		UE_LOG(LogGameLift, Error, TEXT("InitSDK failed: %s"), *InitOutcome.GetError().m_errorMessage);
		return;
	}
	UE_LOG(LogGameLift, Log, TEXT("InitSDK succeeded"));

	ProcessParameters = MakeShared<FProcessParameters>();

	// GameLift gave us a match. Tell it we're ready for players.
	ProcessParameters->OnStartGameSession.BindLambda(
		[GameLiftSdkModule](Aws::GameLift::Server::Model::GameSession InGameSession)
		{
			UE_LOG(LogGameLift, Log, TEXT("Game session starting: %s"),
				*FString(InGameSession.GetGameSessionId()));
			GameLiftSdkModule->ActivateGameSession();
		});

	// GameLift is shutting us down. This is where the process ends - no travelling back to a menu.
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

	// Asked about once a minute. If we don't answer in time GameLift assumes we're unhealthy.
	ProcessParameters->OnHealthCheck.BindLambda(
		[]()
		{
			return true;
		});

	// The port players connect on. -port= overrides it so one machine can run several servers.
	ProcessParameters->port = FURL::UrlConfig.DefaultPort;
	int32 PortOverride = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("port="), PortOverride))
	{
		ProcessParameters->port = PortOverride;
	}

	// Relative to the packaged build root. GameLift uploads these to S3 when the match ends,
	// which is how you read the log afterwards.
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



