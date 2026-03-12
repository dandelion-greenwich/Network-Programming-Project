// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPrCharacter.h"

#include "Engine/LocalPlayer.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "HealthComponent.h"
#include "MultiplayerSubsystem.h"
#include "NetworkPrGameState.h"


DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ANetworkPrCharacter

ANetworkPrCharacter::ANetworkPrCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	/*Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;*/
	
	PushEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PushEffect"));
	PushEffect->SetupAttachment(RootComponent);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	HealthComp = CreateDefaultSubobject<UHealthComponent>("Health System");
	CanPush = true;
}

void ANetworkPrCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
}

void ANetworkPrCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	RespawnPos = GetActorLocation();
	DefaultMaterial = GetMesh() -> GetMaterial(0);
	
	if (!HasAuthority()) return;
	
	ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
	if (!GS) return;
	GS->RegisterPlayer(this);
	
	if (this == GS -> Player1 && Player1Material)
		DefaultMaterial = Player1Material;
	else if (this == GS -> Player2 && Player2Material)
		DefaultMaterial = Player2Material;
	
	Multicast_SetDefaultMaterial();
}

void ANetworkPrCharacter::PrintString(const FString& String) 
{
	if(GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, String);
}

void ANetworkPrCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle,
		this,
		&ANetworkPrCharacter::SetCamera,
		0.2,false);
}


void ANetworkPrCharacter::SetCamera()
{
	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("MainCamera"), FoundCameras);

	if (FoundCameras.Num() > 0)
	{
		AActor* NewCamera = FoundCameras[0];
		ClientRPC_SetCamera(NewCamera);
	}
	else
	{
		PrintString("No camera found");
	}

	GetWorldTimerManager().ClearTimer(TimerHandle);
}

void ANetworkPrCharacter::ClientRpc_ShakeCamera_Implementation()
{
	if (IsPlayerControlled()) 
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(CameraShakeClass, 1.0f);
		}
	}
}

void ANetworkPrCharacter::ClientRPC_SetCamera_Implementation(AActor* NewCamera)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		PrintString("Player controller is null");
		return;
	}

	PC->SetViewTarget(NewCamera);
}

void ANetworkPrCharacter::ServerRPC_Attack_Implementation()
{
	if (!CanPush) return;
	CanPush = false;

	Multicast_PushVFX();

	// Logging event for playtesting session, will be removed afterwards
	ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
	if (GS && GS -> Player1 == this)
		ServerRPC_LogEvent(EGameEventType::PlayerPushAttempt, "Player1", GetActorLocation(), "");
	else if (GS && GS -> Player2 == this)
		ServerRPC_LogEvent(EGameEventType::PlayerPushAttempt, "Player2", GetActorLocation(), "");
	
	GetWorld()->GetTimerManager().SetTimer(
	TimerHandle,                
	this,                       
	&ANetworkPrCharacter::ResetPush,
	PushResetTime,                        
	false,                       
	-1.0);
	
	// Parameters for SweepMultiByChannel
	FVector StartVector = GetActorLocation() + (GetActorForwardVector() * 60.f ) - (GetActorUpVector() * 70.f);
	FVector EndVector = StartVector + (GetActorForwardVector() * 500.f);
	AttackSphereRadius = 60.f; // JUST FOR EXAMPLE WILL HAVE TO BE CONFIGURED IN BLUEPRINT
	FQuat SphereRotation = FQuat::Identity;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(AttackSphereRadius);
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;
	
	//DrawDebugSphere(GetWorld(), StartVector, AttackSphereRadius, 10.f, FColor::Red, false, 2.0f);
	
	// Checks if any objects are within the radius of a multicast
	bool HasHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		StartVector,
		StartVector,
		SphereRotation,
		ECC_Pawn,
		SphereShape,
		QueryParams);
	
	if (!HasHit) 
		return;

	for (auto HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		UE_LOG(LogTemp, Warning, TEXT("Hit actor: %s"), *HitActor->GetActorNameOrLabel());

		if (!HitActor || !HitActor -> ActorHasTag("Player")) continue; // Go to the next iteration of for loop if HitActor is a wall for instance

		ANetworkPrCharacter* Character = Cast<ANetworkPrCharacter>(HitActor);
		if (!Character || Character->HealthComp->bIsInvincible) continue;
		
		FVector LaunchVelocity = GetActorForwardVector() * 1000.f; 
		LaunchVelocity.Z += 500.f; // Add a little jump
		Character->LaunchCharacter(LaunchVelocity, true, true);
		
		// Logging event for playtesting session, will be removed afterwards
		if (GS && GS -> Player1 == this)
			ServerRPC_LogEvent(EGameEventType::PlayerPushSuccess, "Player1", GetActorLocation(), "");
		else if (GS && GS -> Player2 == this)
			ServerRPC_LogEvent(EGameEventType::PlayerPushSuccess, "Player2", GetActorLocation(), "");
		if (GS && GS -> Player1 == Character)
			ServerRPC_LogEvent(EGameEventType::BeenPushedRecently, "Player1", GetActorLocation(), "");
		else if (GS && GS -> Player2 == Character)
			ServerRPC_LogEvent(EGameEventType::BeenPushedRecently, "Player2", GetActorLocation(), "");
	}
}

void ANetworkPrCharacter::RespawnPlayer()
{
	SetActorLocation(RespawnPos);
}

void ANetworkPrCharacter::ResetPush()
{
	CanPush = true;
}

void ANetworkPrCharacter::ServerRPC_LogEvent_Implementation(EGameEventType GameType, const FString& PlayerNumber, FVector Location, const FString& ExtraData)
{
	float GameTime = GetWorld()->GetTimeSeconds();
	
	UMultiplayerSubsystem* MultiplayerSubsystem = GetGameInstance()->GetSubsystem<UMultiplayerSubsystem>();
	if (MultiplayerSubsystem)
		MultiplayerSubsystem -> LogEvent(GameTime, GameType, PlayerNumber, Location, ExtraData);
}

void ANetworkPrCharacter::Multicast_SetDefaultMaterial_Implementation()
{
	if (DefaultMaterial && GetMesh())
		GetMesh()->SetMaterial(0, DefaultMaterial);
}

void ANetworkPrCharacter::Multicast_SetHitMaterial_Implementation()
{
	if (HitMaterial && GetMesh())
		GetMesh()->SetMaterial(0, HitMaterial);
}


void ANetworkPrCharacter::Multicast_PushVFX_Implementation()
{
	if (AttackMontage) PlayAnimMontage(AttackMontage);

	if (PushEffect)
	{
		PushEffect -> Deactivate();
		PushEffect -> Activate();
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ANetworkPrCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ANetworkPrCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ANetworkPrCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANetworkPrCharacter::Move);

		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ANetworkPrCharacter::ServerRPC_Attack);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ANetworkPrCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X); 
	}
}

void ANetworkPrCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ANetworkPrCharacter::Jump()
{
	Super::Jump();

	ANetworkPrGameState* GS = GetWorld()->GetGameState<ANetworkPrGameState>();
	if (GS && GS -> Player1 == this)
		ServerRPC_LogEvent(EGameEventType::PlayerJump, "Player1", GetActorLocation(), "");
	else if (GS && GS -> Player2 == this)
		ServerRPC_LogEvent(EGameEventType::PlayerJump, "Player2", GetActorLocation(), "");
}
