// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "MultiplayerSubsystem.generated.h"

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerCreateDelegate, bool, WasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerJoinDelegate, bool, WasSuccessful);

/**
 * 
 */
UCLASS()
class NETWORKPR_API UMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSubsystem();

	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

	// Faster debugging
	void PrintString(const FString& String);

	// Networking functions
	UFUNCTION(BlueprintCallable)
	void CreateServer(FString ServerName);
	UFUNCTION(BlueprintCallable)
	void FindServer(FString ServerName);
	void OnCreateSessionComplete(FName SessionName, bool WasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool WasSuccessful);
	void OnFindSessionsComplete(bool WasSuccessful);
	void OnJoinSessionsComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	// Supportive variables
	IOnlineSessionPtr SessionInterface;
	IOnlineSubsystem* OnlineSubSystem;
	bool CreateServerAfterDestroy;
	FString DestroyServerName;
	FString ServerNameTofind;
	FName MySessionName;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	UPROPERTY(BlueprintReadWrite)
	FString GameMapPath;

	// Delegates
	UPROPERTY(BlueprintAssignable)
	FServerCreateDelegate ServerCreateDel;
	UPROPERTY(BlueprintAssignable)
	FServerJoinDelegate ServerJoinDel;
};
