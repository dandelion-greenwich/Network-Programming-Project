// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "ArcadeGameMode.h"
#include "NetworkPrCharacter.h"
#include "NetworkPrGameState.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UHealthComponent::UHealthComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
	bIsInvincible = false;
}


// Called when the game starts
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = MaxHealth;
	OnHealthChanged.Broadcast(GetOwner(), CurrentHealth);
}


// Called every frame
void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

void UHealthComponent::TakeDamage(float DamageAmount, EDamageType DamageType)
{
	if (!GetOwner()->HasAuthority() || CurrentHealth <= 0.0f || bIsInvincible) return;

	// Break CurrentHealth into to variables to not deal damage to the next heart
	float WholeHearts = FMath::FloorToFloat(CurrentHealth);
	float Fragment = CurrentHealth - WholeHearts;
	if (CurrentHealth > 0 && Fragment <= 0)
	{
		WholeHearts -= 1.0f;
		Fragment = 1.0f;
	}
	float NewFragment = FMath::Clamp(Fragment - DamageAmount, 0.0f, 1.0f);
	
	GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle);
	CurrentHealth = WholeHearts + NewFragment;
	ANetworkPrCharacter* Player = Cast<ANetworkPrCharacter>(GetOwner());
	
	if (DamageType == EDamageType::Fall)
	{
		bIsInvincible = true;
		OnRep_Invincible();
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle, this, &UHealthComponent::InvincibleTimer,
			InvincibleResetTimer, false, -1.0);
	}
	OnRep_Health(); // Update Server UI manually
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHealthComponent, bIsInvincible);
}

void UHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(GetOwner(), CurrentHealth);
	if (CurrentHealth <= 0.0f)
	{
		AActor* Owner = GetOwner();
		OnDeath.Broadcast(Owner);
		AArcadeGameMode* GM = GetWorld()->GetAuthGameMode<AArcadeGameMode>();
		if (GM) GM->GameOver(Owner);
	}
}

void UHealthComponent::OnRep_Invincible()
{
	ANetworkPrCharacter* Player = Cast<ANetworkPrCharacter>(GetOwner());
	ANetworkPrGameState* GS = GetWorld() -> GetGameState<ANetworkPrGameState>();
	if (!Player || !GS) return;
	
	if (bIsInvincible && GS -> CurrentGameState != EGameState::GameOver)
		Player->ApplyHitMaterial();
}

void UHealthComponent::InvincibleTimer()
{
	bIsInvincible = false;
	ANetworkPrCharacter* Player = Cast<ANetworkPrCharacter>(GetOwner());
	if (Player) Player -> Multicast_SetDefaultMaterial();
}

