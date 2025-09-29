// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkPr/Tinkering/TestingBoxActor.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ATestingBoxActor::ATestingBoxActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ReplicatedVar = 100.f;
}

// Called when the game starts or when spawned
void ATestingBoxActor::BeginPlay()
{
	Super::BeginPlay();
	
	SetReplicates(true);
	SetReplicateMovement(true);

	if (HasAuthority())
		GetWorld()->GetTimerManager().SetTimer(TestTimer, this, &ATestingBoxActor::DecreaseReplicatedVar, 1.0f, false);
	
}

// Called every frame
void ATestingBoxActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/*if (HasAuthority())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Blue, TEXT("Server"));
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, TEXT("Client"));	
	}*/
}

void ATestingBoxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATestingBoxActor, ReplicatedVar);
}

void ATestingBoxActor::OnRep_ReplicatedVar()
{
	if (HasAuthority())
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red,
				TEXT("Server: OnRep_ReplicatedVar"));

		FVector NewLocation = GetActorLocation() + FVector(0, 0, 200);
		//SetActorLocation(NewLocation);
	}
	else
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Blue,
				FString::Printf(TEXT("Client %d: OnRep_ReplicatedVar"), UE::GetPlayInEditorID()));
	}
}

void ATestingBoxActor::DecreaseReplicatedVar()
{
	ReplicatedVar--;
	OnRep_ReplicatedVar();
	if (ReplicatedVar > 0)
			GetWorld()->GetTimerManager().SetTimer(TestTimer, this, &ATestingBoxActor::DecreaseReplicatedVar, 1.0f, false);
}

