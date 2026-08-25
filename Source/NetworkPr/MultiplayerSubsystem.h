// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MultiplayerSubsystem.generated.h"

// One row in the server list
USTRUCT(BlueprintType)
struct FGameSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString GameSessionId;
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 Players = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;
};

// Everything this class does ends up here. On-screen messages disappear; the log doesn't
DECLARE_LOG_CATEGORY_EXTERN(LogMultiplayer, Log, All);

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerCreateDelegate, bool, WasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerJoinDelegate, bool, WasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FServerListDelegate, bool, WasSuccessful, const TArray<FGameSessionInfo>&, Sessions);

/**
 * Finds and creates games through the GameLift backend.
 *
 * We can't put AWS keys in the game, so three Lambda functions do the AWS calls instead.
 * This class just sends them JSON, gets an IP and port back, and travels there.
 */
UCLASS(Config = Game)
class NETWORKPR_API UMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Faster debugging
	void PrintString(const FString& String);

	// Makes a new game, takes a slot on it, and joins
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void CreateServer(FString ServerName);

	// Fetches the list of games. The result comes back on ServerListDel
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void RefreshServerList();

	// Takes a slot on a game someone else made, and joins
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void JoinServer(const FString& GameSessionId);

	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer")
	FServerCreateDelegate ServerCreateDel;
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer")
	FServerJoinDelegate ServerJoinDel;
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer")
	FServerListDelegate ServerListDel;

	// What went wrong last time, so the menu can show it
	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer")
	FString LastError;

	// These live in DefaultGame.ini so you can change them without rebuilding
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer")
	FString CreateGameUrl;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer")
	FString ListGamesUrl;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer")
	FString JoinGameUrl;

private:
	// Pulls the URLs from LocalEndpoints.ini and the command line, in that order
	void LoadEndpointOverrides();
	// Reads ipAddress, port and playerSessionId out of a create or join response
	bool ParseConnectionInfo(const FString& Json, FString& OutIp, int32& OutPort, FString& OutPlayerSessionId);
	// Connects to the server, passing the player session id along for it to check
	void TravelToServer(const FString& Ip, int32 Port, const FString& PlayerSessionId);
	void FailWith(const FString& Message, FServerCreateDelegate* CreateDel, FServerJoinDelegate* JoinDel);

	// GameLift wants a different player id for every player session
	FString PlayerId;
};
