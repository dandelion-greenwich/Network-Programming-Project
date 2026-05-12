// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeteorManager.generated.h"

UCLASS()
class NETWORKPR_API AMeteorManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMeteorManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class AMeteorBase> MeteorBP;
	FTimerHandle TimerHandle;
	UPROPERTY(EditAnywhere)
	int32 RandRange;
	UPROPERTY(EditAnywhere)
	float TimeToSpawn;
	UPROPERTY(EditAnywhere)
	float ExplosionRadius;
	
	void SpawnMeteor();
	UFUNCTION()
	void SpawnMeteorTimer();
};
