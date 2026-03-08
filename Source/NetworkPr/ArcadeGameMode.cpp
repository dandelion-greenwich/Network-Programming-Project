// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcadeGameMode.h"
#include "MultiplayerSubsystem.h"
#include "NetworkPrGameInstance.h"
#include "NetworkPrGameState.h"
#include "Kismet/GameplayStatics.h"

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
	if (!Subsystem) return;

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
}

void AArcadeGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	UNetworkPrGameInstance* GI = Cast<UNetworkPrGameInstance>(GetGameInstance());
	if (GI && GI->CurrentGameMode == EGameSessionMode::LocalCoop)
		AddSecondLocalPlayer();
}

void AArcadeGameMode::AddSecondLocalPlayer()
{
	UGameplayStatics::CreatePlayer(GetWorld(), 1, true);
}

void AArcadeGameMode::TryToStartMatch()
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();
    
	if (GS && GS->Player1 && GS->Player2)
	{
		OnStartMatch.Broadcast();
		GS -> Multicast_Play();

		// Logging Game Start
		float GameTime = GetWorld()->GetTimeSeconds();
		UMultiplayerSubsystem* MultiplayerSubsystem = GetGameInstance()->GetSubsystem<UMultiplayerSubsystem>();
		if (!MultiplayerSubsystem) return;


		FString GameMode;
		UNetworkPrGameInstance* GI = Cast<UNetworkPrGameInstance>(GetGameInstance());
		if (!GI) return;
		switch (GI -> CurrentGameMode)
		{
		case EGameSessionMode::LocalCoop:
			GameMode = FString::Printf(TEXT("Local Coop"));
			break;
		case EGameSessionMode::NetworkCoop:
			GameMode = FString::Printf(TEXT("Network Coop"));
			break;	
		}
		
		MultiplayerSubsystem -> LogEvent(GameTime, EGameEventType::MatchStart, "", FVector::ZeroVector, GameMode);
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
		GS -> ServerRPC_SetAllPlayersToBeInvincible();
		GS -> Multicast_GameOver();
	}

	// Logging Game End
	UMultiplayerSubsystem* MultiplayerSubsystem = GetGameInstance()->GetSubsystem<UMultiplayerSubsystem>();
	if (MultiplayerSubsystem)
	{
		float GameTime = GetWorld()->GetTimeSeconds();
		FString WinningPlayer;
		if (DeadPlayer ==  GS ->Player1)
			WinningPlayer = FString("Player2 Win");
		else if (DeadPlayer ==  GS ->Player2)
			WinningPlayer = FString("Player1 Win");
		
		MultiplayerSubsystem -> LogEvent(GameTime, EGameEventType::MatchEnd, "", FVector::ZeroVector, WinningPlayer);
		
		FString MatchID = FDateTime::Now().ToString(TEXT("%d%m%Y_%H%M%S"));
		MultiplayerSubsystem -> ExportToCSV(MatchID);
	}
	
}



