// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerUI.h"
#include "NetworkPrPlayerController.generated.h"


/**
 * 
 */
UCLASS()
class NETWORKPR_API ANetworkPrPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> PlayerWidgetClass;
	UPlayerUI* PlayerWidgetReference;
	FTimerHandle InitTimerHandle;
	bool bP1Bound = false;
	bool bP2Bound = false;

	UFUNCTION()
	void TryInitialiseUI();
	UFUNCTION(Client, Reliable)
	void ClientRPC_SetWaitingText();
	
};
