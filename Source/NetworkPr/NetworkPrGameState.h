// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "NetworkPrCharacter.h"
#include "NetworkPrGameState.generated.h"

UENUM()
enum class EGameState : uint8
{
	Waiting,
	Playing,
	Paused,
	GameOver
};

UCLASS()
class NETWORKPR_API ANetworkPrGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:

	FTimerHandle TimerHandle;
	UPROPERTY(Replicated)
	EGameState CurrentGameState;
	UPROPERTY(Replicated)
	ANetworkPrCharacter* Player1;
	UPROPERTY(Replicated)
	ANetworkPrCharacter* Player2;
	
	// Helper to register players (server only)
	void RegisterPlayer(ANetworkPrCharacter* NewPlayer);
	void TimerToLoadPCToRemoveWaitingUI();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Wait();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Play();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_GameOver();
	void GameOverTimer();
	void SetAllPlayersToBeInvincible();
	void SetFreezeTime(bool bFreeze);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
