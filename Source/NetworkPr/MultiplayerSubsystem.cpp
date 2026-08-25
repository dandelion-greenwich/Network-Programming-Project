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
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY(LogMultiplayer);

void UMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Subsystems don't load their config properties automatically
    LoadConfig();
    LoadEndpointOverrides();

    // No player accounts yet, so a fresh id each run is good enough
    PlayerId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

    UE_LOG(LogMultiplayer, Log, TEXT("Create: %s"), CreateGameUrl.IsEmpty() ? TEXT("(not set)") : *CreateGameUrl);
    UE_LOG(LogMultiplayer, Log, TEXT("List:   %s"), ListGamesUrl.IsEmpty()  ? TEXT("(not set)") : *ListGamesUrl);
    UE_LOG(LogMultiplayer, Log, TEXT("Join:   %s"), JoinGameUrl.IsEmpty()   ? TEXT("(not set)") : *JoinGameUrl);

    if (CreateGameUrl.IsEmpty() || ListGamesUrl.IsEmpty() || JoinGameUrl.IsEmpty())
    {
        const FString Message = TEXT("Backend URLs are not set - see Config/LocalEndpoints.ini, "
                                     "or pass -CreateGameUrl= -ListGamesUrl= -JoinGameUrl=");
        UE_LOG(LogMultiplayer, Error, TEXT("%s"), *Message);
        PrintString(Message);
    }
}

void UMultiplayerSubsystem::LoadEndpointOverrides()
{
    // The repo is public, so the real URLs live in Config/LocalEndpoints.ini, which is gitignored.
    // In the editor this sits in the project. A packaged build has no Config folder unless you
    // make one next to the exe, which is why the command line below also works.
    const FString LocalIniPath = FPaths::ProjectConfigDir() / TEXT("LocalEndpoints.ini");
    if (FPaths::FileExists(LocalIniPath))
    {
        UE_LOG(LogMultiplayer, Log, TEXT("Reading endpoints from %s"), *LocalIniPath);

        FConfigFile LocalIni;
        LocalIni.Read(LocalIniPath);

        // URLs must be quoted in the ini - Unreal treats // as the start of a comment, so an
        // unquoted https://host gets cut down to just "https:". Strip the quotes back off here.
        auto ApplyOverride = [&LocalIni](const TCHAR* Key, FString& Target)
        {
            FString Value;
            if (LocalIni.GetString(TEXT("Endpoints"), Key, Value))
            {
                Value.TrimQuotesInline();
                Value.TrimStartAndEndInline();
                if (!Value.IsEmpty())
                {
                    Target = Value;
                }
            }
        };

        ApplyOverride(TEXT("CreateGameUrl"), CreateGameUrl);
        ApplyOverride(TEXT("ListGamesUrl"), ListGamesUrl);
        ApplyOverride(TEXT("JoinGameUrl"), JoinGameUrl);
    }
    else
    {
        UE_LOG(LogMultiplayer, Log, TEXT("No LocalEndpoints.ini at %s"), *LocalIniPath);
    }

    // Command line beats everything. This is the easy way to point a packaged build at the
    // backend without copying config files around after every package.
    FParse::Value(FCommandLine::Get(), TEXT("CreateGameUrl="), CreateGameUrl);
    FParse::Value(FCommandLine::Get(), TEXT("ListGamesUrl="), ListGamesUrl);
    FParse::Value(FCommandLine::Get(), TEXT("JoinGameUrl="), JoinGameUrl);
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
    UE_LOG(LogMultiplayer, Error, TEXT("%s"), *Message);
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
    UE_LOG(LogMultiplayer, Log, TEXT("Travelling to %s"), *Url);
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
        UE_LOG(LogMultiplayer, Error, TEXT("%s"), *LastError);
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
                UE_LOG(LogMultiplayer, Error, TEXT("%s (connected=%d, code=%d)"), *LastError,
                    bConnectedOk ? 1 : 0, Response.IsValid() ? Response->GetResponseCode() : -1);
                PrintString(LastError);
                ServerListDel.Broadcast(false, Sessions);
                return;
            }

            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
            {
                LastError = TEXT("Server list was not valid JSON");
                UE_LOG(LogMultiplayer, Error, TEXT("%s: %s"), *LastError, *Response->GetContentAsString());
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
