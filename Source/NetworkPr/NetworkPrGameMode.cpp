// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPrGameMode.h"
#include "NetworkPrCharacter.h"
#include "UObject/ConstructorHelpers.h"

ANetworkPrGameMode::ANetworkPrGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
