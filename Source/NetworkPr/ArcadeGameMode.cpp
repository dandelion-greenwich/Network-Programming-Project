// Fill out your copyright notice in the Description page of Project Settings.


#include "ArcadeGameMode.h"
#include "MultiplayerSubsystem.h"
#include "NetworkPrGameInstance.h"
#include "NetworkPrGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"


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
	
	UNetworkPrGameInstance* GI = Cast<UNetworkPrGameInstance>(GetGameInstance());
	if (GI && GI->CurrentGameMode == EGameSessionMode::LocalCoop)
		AddSecondLocalPlayer();
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



