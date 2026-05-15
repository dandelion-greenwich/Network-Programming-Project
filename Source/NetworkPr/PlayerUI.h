// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "PlayerUI.generated.h"

/**
 * 
 */
UCLASS()
class NETWORKPR_API UPlayerUI : public UUserWidget
{
	GENERATED_BODY()

public:

	// The meta tag for a widget in the Blueprint needs to be named EXACTLY
    // If it's named differently, the game will crash
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WaitingForSecondPlayerText;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* GameOverText;
	UPROPERTY(meta = (BindWidget))
	UProgressBar* Player1HealthBar;
	UPROPERTY(meta = (BindWidget))
	UProgressBar* Player2HealthBar;
	UPROPERTY(meta = (BindWidget))
	UButton* ReturnToMainMenuButton;

	UFUNCTION(BlueprintCallable) // Made it callable just in case
	void UpdateHealth(AActor* Player, float NewHealth);
	void RemoveWaitingText();
	UFUNCTION()
	void SetGameOverText(AActor* DeadPlayer);
	void SetGameOverButton();

protected:
	virtual void NativeConstruct() override;
	UFUNCTION()
	void OnReturnToMainMenuClicked();
};
