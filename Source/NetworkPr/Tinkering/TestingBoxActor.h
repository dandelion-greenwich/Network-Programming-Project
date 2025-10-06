// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystem.h"
#include "TestingBoxActor.generated.h"

UCLASS()
class NETWORKPR_API ATestingBoxActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestingBoxActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ReplicatedVar)
	float ReplicatedVar;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable)
	void OnRep_ReplicatedVar();

	void DecreaseReplicatedVar();
	FTimerHandle TestTimer;

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable)
	void MulticastRPCFunction();

	UPROPERTY(EditAnywhere)
	UNiagaraSystem* ExplosionEffect;
};
