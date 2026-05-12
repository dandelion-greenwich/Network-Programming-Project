// Fill out your copyright notice in the Description page of Project Settings.


#include "KillZone.h"
#include "NetworkPrCharacter.h"
#include "HealthComponent.h"
#include "NetworkPrGameState.h"

// Sets default values
AKillZone::AKillZone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);
	Collider = CreateDefaultSubobject<UBoxComponent>(FName("Collision Mesh"));
	Collider -> SetupAttachment(Root);

	Collider->SetCollisionProfileName(TEXT("Trigger"));
	
}

// Called when the game starts or when spawned
void AKillZone::BeginPlay()
{
	Super::BeginPlay();

	// Moved it from constructor cause the bp was not generating event
	Collider->OnComponentBeginOverlap.AddDynamic(this, &AKillZone::OnColliderOverlap); 
}

// Called every frame
void AKillZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AKillZone::OnColliderOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	
	ANetworkPrCharacter* MyCharacter = Cast<ANetworkPrCharacter>(OtherActor);
	if (MyCharacter)MyCharacter -> RespawnPlayer();
	
	UHealthComponent* HealthComp = OtherActor -> FindComponentByClass<UHealthComponent>();
	if (HealthComp) HealthComp -> TakeDamage(1.0f, EDamageType::Fall);
}

