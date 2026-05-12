// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, AActor*, Player, float, CurrentHealth);

UENUM()
enum class EDamageType : uint8
{
	Explosion,
	Fall
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NETWORKPR_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHealthComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FTimerHandle TimerHandle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	int32 MaxHealth;
	UPROPERTY(ReplicatedUsing=OnRep_Health, EditAnywhere, Category = "Health")
	float CurrentHealth;
	UPROPERTY(ReplicatedUsing=OnRep_Invincible, EditAnywhere, Category = "Health")
	bool bIsInvincible;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float InvincibleResetTimer;

	// Events:
	FOnHealthChanged OnHealthChanged;
	FOnDeath OnDeath;
	
	UFUNCTION(BlueprintCallable)
	void TakeDamage(float DamageAmount, EDamageType DamageType);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION()
	void OnRep_Health();
	UFUNCTION()
	void OnRep_Invincible();
	void InvincibleTimer();
};
