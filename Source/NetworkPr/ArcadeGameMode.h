// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArcadeGameMode.generated.h"

// Forward declared so the GameLift SDK headers stay out of this header entirely. They are only
// included in the .cpp, and only for Server builds.
struct FProcessParameters;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMatch);

// A dedicated server has no output log window, so the log file is the only way to see what the
// GameLift handshake did. Its own category makes it filterable and lets you raise the verbosity
// at runtime with "Log LogGameLift Verbose" instead of recompiling.
DECLARE_LOG_CATEGORY_EXTERN(LogGameLift, Log, All);

UCLASS()
class NETWORKPR_API AArcadeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	// Checks the player session token before we let anyone in
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	// First point where we have both the controller and its connection options
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	UFUNCTION(BlueprintCallable)
	int32 GetPlayerSlot(AController* Player) const;

	void AddSecondLocalPlayer();
	void TryToStartMatch();
	void WaitForTheSecondPlayer();
	void ContinueGame();
	void GameOver(AActor* DeadPlayer);

	FTimerHandle TimerHandle;
	FOnStartMatch OnStartMatch;

private:
	// Performs the GameLift handshake: InitSDK, register the lifecycle callbacks, ProcessReady.
	// Only defined and called for Server builds - see WITH_GAMELIFT in NetworkPr.Build.cs.
	void InitGameLift();

	// Must outlive InitGameLift: GameLift keeps a reference to the callbacks it holds.
	TSharedPtr<FProcessParameters> ProcessParameters;

	// Which GameLift player session each player arrived on, so we can hand the slot back when
	// they leave. Without this, reserved slots pile up and games report themselves full.
	TMap<TWeakObjectPtr<AController>, FString> PlayerSessionIds;
};
