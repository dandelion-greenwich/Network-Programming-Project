// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkPrPlayerController.h"
#include "HealthComponent.h"
#include "NetworkPrGameState.h"

void ANetworkPrPlayerController::BeginPlay()
{
	Super::BeginPlay();

if (IsLocalPlayerController() && PlayerWidgetClass)
    {
        PlayerWidgetReference = CreateWidget<UPlayerUI>(this, PlayerWidgetClass);
        if (PlayerWidgetReference)
        {
            PlayerWidgetReference->AddToViewport();
            
            GetWorldTimerManager().SetTimer(
                InitTimerHandle, 
                this, 
                &ANetworkPrPlayerController::TryInitialiseUI, 
                0.5f, 
                true
            );
        }
    }
}

void ANetworkPrPlayerController::TryInitialiseUI()
{
    ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
    
    if (!GS || !PlayerWidgetReference) return;
	
	if (!bP1Bound && GS->Player1)
	{
		UHealthComponent* HealthComp = GS->Player1->FindComponentByClass<UHealthComponent>();
		if (HealthComp)
		{
			HealthComp->OnHealthChanged.AddDynamic(PlayerWidgetReference, &UPlayerUI::UpdateHealth);
			HealthComp->OnDeath.AddDynamic(PlayerWidgetReference, &UPlayerUI::SetGameOverText);
			
			PlayerWidgetReference->UpdateHealth(GS->Player1, HealthComp->CurrentHealth);
			bP1Bound = true;
		}
	}

		
	if (!bP2Bound && GS->Player2)
	{
		UHealthComponent* HealthComp = GS->Player2->FindComponentByClass<UHealthComponent>();
		if (HealthComp)
		{
			HealthComp->OnHealthChanged.AddDynamic(PlayerWidgetReference, &UPlayerUI::UpdateHealth);
			HealthComp->OnDeath.AddDynamic(PlayerWidgetReference, &UPlayerUI::SetGameOverText);
			
			PlayerWidgetReference->UpdateHealth(GS->Player2, HealthComp->CurrentHealth);
			bP2Bound = true;
		}
	}

	if (bP1Bound && bP2Bound)
		GetWorldTimerManager().ClearTimer(InitTimerHandle);
	
}

void ANetworkPrPlayerController::ClientRPC_SetWaitingText_Implementation()
{
	if (PlayerWidgetReference)
		PlayerWidgetReference-> RemoveWaitingText();
	else
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, "PlayerWidgetReference is null in player controller");
}
