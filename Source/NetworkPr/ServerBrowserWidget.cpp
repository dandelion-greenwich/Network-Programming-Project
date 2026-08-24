// Fill out your copyright notice in the Description page of Project Settings.

#include "ServerBrowserWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UServerRowHandler::HandleClicked()
{
	if (Subsystem.IsValid() && !GameSessionId.IsEmpty())
	{
		Subsystem->JoinServer(GameSessionId);
	}
}

UMultiplayerSubsystem* UServerBrowserWidget::GetMultiplayer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMultiplayerSubsystem>() : nullptr;
}

void UServerBrowserWidget::SetStatus(const FString& Text)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Text));
	}
}

void UServerBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMultiplayerSubsystem* MP = GetMultiplayer())
	{
		MP->ServerListDel.AddDynamic(this, &UServerBrowserWidget::HandleServerList);
		MP->ServerJoinDel.AddDynamic(this, &UServerBrowserWidget::HandleJoinResult);
	}

	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &UServerBrowserWidget::Refresh);
	}

	// Load the list as soon as the panel opens so there's nothing to press first
	Refresh();
}

void UServerBrowserWidget::NativeDestruct()
{
	if (UMultiplayerSubsystem* MP = GetMultiplayer())
	{
		MP->ServerListDel.RemoveDynamic(this, &UServerBrowserWidget::HandleServerList);
		MP->ServerJoinDel.RemoveDynamic(this, &UServerBrowserWidget::HandleJoinResult);
	}

	Super::NativeDestruct();
}

void UServerBrowserWidget::Refresh()
{
	UMultiplayerSubsystem* MP = GetMultiplayer();
	if (!MP)
	{
		SetStatus(TEXT("Multiplayer subsystem is missing"));
		return;
	}

	SetStatus(TEXT("Looking for games..."));
	MP->RefreshServerList();
}

void UServerBrowserWidget::HandleServerList(bool bWasSuccessful, const TArray<FGameSessionInfo>& Sessions)
{
	if (!ServerListBox) return;

	// Rebuild from scratch every time - the list is small and this keeps it simple
	ServerListBox->ClearChildren();
	RowHandlers.Reset();

	UMultiplayerSubsystem* MP = GetMultiplayer();

	if (!bWasSuccessful)
	{
		SetStatus(MP && !MP->LastError.IsEmpty() ? MP->LastError : TEXT("Could not load the list"));
		return;
	}

	if (Sessions.Num() == 0)
	{
		SetStatus(TEXT("No games running - create one"));
		return;
	}

	SetStatus(FString::Printf(TEXT("%d game(s) found"), Sessions.Num()));

	for (const FGameSessionInfo& Session : Sessions)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (!Button || !Label) continue;

		const FString Title = Session.Name.IsEmpty() ? TEXT("Unnamed Game") : Session.Name;
		Label->SetText(FText::FromString(
			FString::Printf(TEXT("%s    %d/%d"), *Title, Session.Players, Session.MaxPlayers)));
		Label->SetJustification(ETextJustify::Center);

		Button->AddChild(Label);

		// Give the button something that knows which game it points at
		UServerRowHandler* Handler = NewObject<UServerRowHandler>(this);
		Handler->GameSessionId = Session.GameSessionId;
		Handler->Subsystem = MP;
		Button->OnClicked.AddDynamic(Handler, &UServerRowHandler::HandleClicked);
		RowHandlers.Add(Handler);

		ServerListBox->AddChild(Button);
	}
}

void UServerBrowserWidget::HandleJoinResult(bool bWasSuccessful)
{
	// On success we're already travelling to the server, so there's nothing to show
	if (bWasSuccessful) return;

	UMultiplayerSubsystem* MP = GetMultiplayer();
	SetStatus(MP && !MP->LastError.IsEmpty() ? MP->LastError : TEXT("Could not join that game"));

	// The game probably filled up or ended, so show what's actually there now
	Refresh();
}
