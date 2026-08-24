#include "MultiplayerSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

void UMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Subsystems don't load their config properties automatically
    LoadConfig();

    // No player accounts yet, so a fresh id each run is good enough
    PlayerId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    if (CreateGameUrl.IsEmpty() || ListGamesUrl.IsEmpty() || JoinGameUrl.IsEmpty())
    {
        PrintString("Backend URLs are not set - fill them in DefaultGame.ini");
    }
}

void UMultiplayerSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UMultiplayerSubsystem::PrintString(const FString& String)
{
    if (GEngine)
       GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, String);
}

void UMultiplayerSubsystem::FailWith(const FString& Message, FServerCreateDelegate* CreateDel, FServerJoinDelegate* JoinDel)
{
    LastError = Message;
    PrintString(Message);

    if (CreateDel) CreateDel->Broadcast(false);
    if (JoinDel)   JoinDel->Broadcast(false);
}

bool UMultiplayerSubsystem::ParseConnectionInfo(const FString& Json, FString& OutIp, int32& OutPort, FString& OutPlayerSessionId)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

    return Root->TryGetStringField(TEXT("ipAddress"), OutIp)
        && Root->TryGetNumberField(TEXT("port"), OutPort)
        && Root->TryGetStringField(TEXT("playerSessionId"), OutPlayerSessionId);
}

void UMultiplayerSubsystem::TravelToServer(const FString& Ip, int32 Port, const FString& PlayerSessionId)
{
    APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!PC)
    {
        PrintString("No local player controller to travel with");
        return;
    }

    // Send the player session id with us so the server can check we're allowed in
    const FString Url = FString::Printf(TEXT("%s:%d?playerSessionId=%s"), *Ip, Port, *PlayerSessionId);
    PrintString("Travelling to " + Url);
    PC->ClientTravel(Url, ETravelType::TRAVEL_Absolute);
}

void UMultiplayerSubsystem::CreateServer(FString ServerName)
{
    if (ServerName.IsEmpty())
    {
        FailWith(TEXT("Server name cannot be empty"), &ServerCreateDel, nullptr);
        return;
    }
    if (CreateGameUrl.IsEmpty())
    {
        FailWith(TEXT("CreateGameUrl is not configured"), &ServerCreateDel, nullptr);
        return;
    }

    const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("name"), ServerName);
    Body->SetStringField(TEXT("playerId"), PlayerId);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(CreateGameUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
        {
            if (!bConnectedOk || !Response.IsValid())
            {
                FailWith(TEXT("Could not reach the backend"), &ServerCreateDel, nullptr);
                return;
            }

            const FString Content = Response->GetContentAsString();
            if (Response->GetResponseCode() != 200)
            {
                FailWith(FString::Printf(TEXT("Create failed (%d): %s"), Response->GetResponseCode(), *Content),
                    &ServerCreateDel, nullptr);
                return;
            }

            FString Ip, PlayerSessionId;
            int32 Port = 0;
            if (!ParseConnectionInfo(Content, Ip, Port, PlayerSessionId))
            {
                FailWith(TEXT("Create response was not in the expected shape"), &ServerCreateDel, nullptr);
                return;
            }

            ServerCreateDel.Broadcast(true);
            TravelToServer(Ip, Port, PlayerSessionId);
        });

    Request->ProcessRequest();
}

void UMultiplayerSubsystem::RefreshServerList()
{
    if (ListGamesUrl.IsEmpty())
    {
        LastError = TEXT("ListGamesUrl is not configured");
        PrintString(LastError);
        ServerListDel.Broadcast(false, TArray<FGameSessionInfo>());
        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ListGamesUrl);
    Request->SetVerb(TEXT("GET"));

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
        {
            TArray<FGameSessionInfo> Sessions;

            if (!bConnectedOk || !Response.IsValid() || Response->GetResponseCode() != 200)
            {
                LastError = TEXT("Could not fetch the server list");
                PrintString(LastError);
                ServerListDel.Broadcast(false, Sessions);
                return;
            }

            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
            {
                LastError = TEXT("Server list was not valid JSON");
                PrintString(LastError);
                ServerListDel.Broadcast(false, Sessions);
                return;
            }

            const TArray<TSharedPtr<FJsonValue>>* Games = nullptr;
            if (Root->TryGetArrayField(TEXT("games"), Games) && Games)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Games)
                {
                    const TSharedPtr<FJsonObject>* Entry = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry) continue;

                    FGameSessionInfo Info;
                    (*Entry)->TryGetStringField(TEXT("gameSessionId"), Info.GameSessionId);
                    (*Entry)->TryGetStringField(TEXT("name"), Info.Name);
                    (*Entry)->TryGetNumberField(TEXT("players"), Info.Players);
                    (*Entry)->TryGetNumberField(TEXT("maxPlayers"), Info.MaxPlayers);
                    Sessions.Add(Info);
                }
            }

            ServerListDel.Broadcast(true, Sessions);
        });

    Request->ProcessRequest();
}

void UMultiplayerSubsystem::JoinServer(const FString& GameSessionId)
{
    if (GameSessionId.IsEmpty())
    {
        FailWith(TEXT("No game session selected"), nullptr, &ServerJoinDel);
        return;
    }
    if (JoinGameUrl.IsEmpty())
    {
        FailWith(TEXT("JoinGameUrl is not configured"), nullptr, &ServerJoinDel);
        return;
    }

    const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("gameSessionId"), GameSessionId);
    Body->SetStringField(TEXT("playerId"), PlayerId);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(JoinGameUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
        {
            if (!bConnectedOk || !Response.IsValid())
            {
                FailWith(TEXT("Could not reach the backend"), nullptr, &ServerJoinDel);
                return;
            }

            const FString Content = Response->GetContentAsString();
            if (Response->GetResponseCode() != 200)
            {
                // A 409 usually means the game filled up or ended while we were looking at the list
                FailWith(FString::Printf(TEXT("Join failed (%d): %s"), Response->GetResponseCode(), *Content),
                    nullptr, &ServerJoinDel);
                return;
            }

            FString Ip, PlayerSessionId;
            int32 Port = 0;
            if (!ParseConnectionInfo(Content, Ip, Port, PlayerSessionId))
            {
                FailWith(TEXT("Join response was not in the expected shape"), nullptr, &ServerJoinDel);
                return;
            }

            ServerJoinDel.Broadcast(true);
            TravelToServer(Ip, Port, PlayerSessionId);
        });

    Request->ProcessRequest();
}
