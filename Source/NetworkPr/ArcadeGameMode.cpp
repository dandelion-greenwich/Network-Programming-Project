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



