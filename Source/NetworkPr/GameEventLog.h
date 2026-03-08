// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameEventLog.generated.h"

UENUM()
enum class EGameEventType : uint8
{
    MatchStart,
    PlayerJump,
    PlayerPushAttempt,
    PlayerPushSuccess,
    BeenPushedRecently,
    DamageTaken,
    MeteorSpawn,
    MeteorHit,
    MatchEnd
};

USTRUCT()
struct FGameEventLog
{
    GENERATED_BODY()

    UPROPERTY() float Timestamp; 
    UPROPERTY() EGameEventType EventType; 
    UPROPERTY() FString Instigator; 
    UPROPERTY() FVector Location; 
    UPROPERTY() FString AdditionalData; 

    FGameEventLog() : Timestamp(0.0f), EventType(EGameEventType::MatchStart), Location(FVector::ZeroVector) {}
};