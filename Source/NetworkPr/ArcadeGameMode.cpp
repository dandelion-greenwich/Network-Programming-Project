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

	bP1IsPursuing = false;
	bP2IsPursuing = false;
	GetWorldTimerManager().SetTimer(
		PursuitCheckTimer,
		this,
		&AArcadeGameMode::CheckPursuitState,
		0.2f,
		true);
	
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

// Logging event for playtesting session, will be removed afterwards
void AArcadeGameMode::CheckPursuitState()
{
	ANetworkPrGameState* GS = GetGameState<ANetworkPrGameState>();

    if (!GS -> Player1 || !GS -> Player2) return;

    // Get necessary math vectors
    FVector P1Loc = GS -> Player1->GetActorLocation();
    FVector P2Loc = GS -> Player2->GetActorLocation();
    
    FVector P1VelocityDir = GS -> Player1->GetVelocity().GetSafeNormal();
    FVector P2VelocityDir = GS -> Player2->GetVelocity().GetSafeNormal();

    FVector DirP1ToP2 = (P2Loc - P1Loc).GetSafeNormal();
    FVector DirP2ToP1 = (P1Loc - P2Loc).GetSafeNormal();

    // Calculate Dot Products
    float P1Dot = FVector::DotProduct(P1VelocityDir, DirP1ToP2);
    float P2Dot = FVector::DotProduct(P2VelocityDir, DirP2ToP1);

    // We consider it a "pursuit" if they are moving fast enough AND the dot product is > 0.7
    float MinSpeed = 50.0f; // Ignore tiny stick nudges
    bool bP1CurrentlyPursuing = (P1Dot > 0.7f) && (GS -> Player1->GetVelocity().Size() > MinSpeed);
    bool bP2CurrentlyPursuing = (P2Dot > 0.7f) && (GS -> Player2->GetVelocity().Size() > MinSpeed);

    UMultiplayerSubsystem* MS = GetGameInstance()->GetSubsystem<UMultiplayerSubsystem>();
    float Time = GetWorld()->GetTimeSeconds();

    // CHECK PLAYER 1 STATE CHANGES
    // If they just STARTED pursuing
    if (bP1CurrentlyPursuing && !bP1IsPursuing)
    {
        bP1IsPursuing = true;
        if (MS) MS->LogEvent(Time, EGameEventType::PlayerPursuitStart, TEXT("Player1"), P1Loc, TEXT("Started chasing P2"));
    }
    // If they just STOPPED pursuing
    else if (!bP1CurrentlyPursuing && bP1IsPursuing)
    {
        bP1IsPursuing = false;
        if (MS) MS->LogEvent(Time, EGameEventType::PlayerPursuitStop, TEXT("Player1"), P1Loc, TEXT("Stopped chasing P2"));
    }

    // CHECK PLAYER 2 STATE CHANGES
    if (bP2CurrentlyPursuing && !bP2IsPursuing)
    {
        bP2IsPursuing = true;
        if (MS) MS->LogEvent(Time, EGameEventType::PlayerPursuitStart, TEXT("Player2"), P2Loc, TEXT("Started chasing P1"));
    }
    else if (!bP2CurrentlyPursuing && bP2IsPursuing)
    {
        bP2IsPursuing = false;
        if (MS) MS->LogEvent(Time, EGameEventType::PlayerPursuitStop, TEXT("Player2"), P2Loc, TEXT("Stopped chasing P1"));
    }
}



