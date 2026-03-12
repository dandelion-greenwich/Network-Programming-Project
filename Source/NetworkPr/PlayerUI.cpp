// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerUI.h"
#include "NetworkPrGameState.h"
#include "Kismet/GameplayStatics.h"

void UPlayerUI::UpdateHealth(AActor* Player, float NewHealth)
{
    ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
    if (!GS) return;

    if (Player == GS->Player1)
    {
        float NewPercent = NewHealth / 6.f;
        if (Player1HealthBar) Player1HealthBar -> SetPercent(NewPercent);
    }
    else if (Player == GS->Player2)
    {
        float NewPercent = NewHealth / 6.f;
        if (Player2HealthBar) Player2HealthBar -> SetPercent(NewPercent);
    }
}

void UPlayerUI::RemoveWaitingText()
{
    WaitingForSecondPlayerText->SetVisibility(ESlateVisibility::Hidden);
}

void UPlayerUI::SetGameOverText(AActor* DeadPlayer)
{
    ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
    if (!DeadPlayer || !GS) return;

    if (DeadPlayer == GS->Player1)
        GameOverText->SetText(FText::FromString("Player 2 win!!!"));
    else if (DeadPlayer == GS->Player2)
        GameOverText->SetText(FText::FromString("Player 1 win!!!"));

    ReturnToMainMenuButton -> SetVisibility(ESlateVisibility::Visible);
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC -> SetInputMode(FInputModeUIOnly());
        PC -> SetShowMouseCursor(true);
    }
}
