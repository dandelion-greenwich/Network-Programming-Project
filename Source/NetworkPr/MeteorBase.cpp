// Fill out your copyright notice in the Description page of Project Settings.


#include "MeteorBase.h"
#include "HealthComponent.h"
#include "NetworkPrCharacter.h"
#include "NetworkPrGameState.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/OverlapResult.h"


// Sets default values
AMeteorBase::AMeteorBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComp = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootComp;
	
	MeteorComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeteorComp -> SetupAttachment(RootComp);
	MeteorComp -> SetVisibility(false);
	MeteorComp->SetNotifyRigidBodyCollision(true); 
	MeteorComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeteorComp->OnComponentHit.AddDynamic(this, &AMeteorBase::OnMeteorHit);
	
	bReplicates = true;
	SetReplicateMovement(true);
}

// Called when the game starts or when spawned
void AMeteorBase::BeginPlay()
{
	Super::BeginPlay();

	TArray<FHitResult> HitResults;
	FVector Start = GetActorLocation();

	FVector DownVector = -GetActorUpVector();
	FVector End =  Start + (DownVector * 1000.f);
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	//DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 3, 0, 1);

	bool hasHit = GetWorld()->LineTraceMultiByChannel(
		HitResults,
		Start,
		End,
		ECC_Visibility,
		CollisionParams);

	if (!hasHit) return;
	
	for (auto HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor) continue;
		
		if (HitActor && HitActor -> ActorHasTag("Floor"))
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnedLocation = HitResult.Location;
			PreviewActorToDestroy = GetWorld()->SpawnActor<AActor>(MeteorClass, SpawnedLocation, FRotator::ZeroRotator, SpawnParameters);
			break;
		}
	}
	
	GetWorld()->GetTimerManager().SetTimer(
	TimerHandle,
	this,
	&AMeteorBase::Multicast_DropMeteor,
	0.5,false);
}

void AMeteorBase::OnMeteorHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	Explosion();
}

void AMeteorBase::Multicast_DropMeteor_Implementation()
{
	MeteorComp -> SetVisibility(true);
	MeteorComp -> SetSimulatePhysics(true);
}

void AMeteorBase::Explosion()
{
	FVector StartVector = MeteorComp -> GetComponentLocation();
	FQuat SphereRotation = FQuat::Identity;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(AttackSphereRadius);
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;

	bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		StartVector,
		FQuat::Identity,
		ECC_Pawn,
		SphereShape,
		QueryParams
	);

	if (bHasOverlap)
	{
		for (auto OverlapResult : OverlapResults)
		{
			AActor* HitActor = OverlapResult.GetActor();
			UE_LOG(LogTemp, Warning, TEXT("Explosion Hit actor: %s"), *HitActor->GetActorNameOrLabel());
			if (!HitActor || !HitActor -> ActorHasTag("Player")) continue; // Go to the next index of for loop if HitActor is not a player

			ANetworkPrCharacter* Player = Cast<ANetworkPrCharacter>(HitActor);
			if (!Player || !Player -> HealthComp || Player -> HealthComp -> bIsInvincible) continue;
			Player -> ClientRpc_ShakeCamera();
			float DistanceDifference = FVector::Dist(StartVector, HitActor->GetActorLocation());

			// Logging event for playtesting session, will be removed afterwards
			FString PlayerNumber;
			ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
			if (GS && GS -> Player1 == Player)
				PlayerNumber = "Player1";
			else if (GS && GS -> Player2 == Player)
				PlayerNumber = "Player2";

			if (DistanceDifference > AttackSphereRadius / 2)
			{
				Player -> HealthComp -> TakeDamage(0.5f, EDamageType::Explosion);
			}
			else
			{
				Player -> HealthComp -> TakeDamage(1.f, EDamageType::Explosion);
			}
		}
	}
	
	//DrawDebugSphere(GetWorld(), StartVector, AttackSphereRadius, 10.f, FColor::Orange, false, 2.0f);
	if (PreviewActorToDestroy != nullptr) PreviewActorToDestroy -> Destroy();
	Multicast_ExplosionVFX();
	
	Destroy();
}

void AMeteorBase::Multicast_ExplosionVFX_Implementation()
{
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		ExplosionEffect,
		SpawnedLocation,
		GetActorRotation(),
		FVector(0.75f),      
		true,               
		true,               
		ENCPoolMethod::None 
	);
}

