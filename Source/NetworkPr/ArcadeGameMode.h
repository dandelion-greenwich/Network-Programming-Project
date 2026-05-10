// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArcadeGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMatch);

UCLASS()
class NETWORKPR_API AArcadeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void AddSecondLocalPlayer();
	void TryToStartMatch();
	void WaitForTheSecondPlayer();
	void ContinueGame();
	void GameOver(AActor* DeadPlayer);

	FTimerHandle TimerHandle;
	FOnStartMatch OnStartMatch;
};
