// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h"
#include "MeteorBase.generated.h"

UCLASS()
class NETWORKPR_API AMeteorBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMeteorBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Hierarchy components:
	class USceneComponent* RootComp;
	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* MeteorComp;
	FVector SpawnedLocation;

	// Supportive variables
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> MeteorClass;
	float AttackSphereRadius;
	AActor* PreviewActorToDestroy;
	UPROPERTY(EditAnywhere)
	UNiagaraSystem* ExplosionEffect;
	
	UFUNCTION()
	void OnMeteorHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRPC_Explosion();
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ExplosionVFX();
	UFUNCTION(Server, Reliable)
	void ServerRPC_LogEvent(EGameEventType GameType, const FString& PlayerNumber, FVector Location, const FString& ExtraData);
};
