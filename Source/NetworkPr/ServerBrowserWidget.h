// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MultiplayerSubsystem.h"
#include "ServerBrowserWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;

/**
 * One of these per row in the list.
 *
 * UButton's OnClicked doesn't tell you which button was pressed, so every row gets its own
 * little object that remembers which game it belongs to.
 */
UCLASS()
class NETWORKPR_API UServerRowHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString GameSessionId;
	UPROPERTY()
	TWeakObjectPtr<UMultiplayerSubsystem> Subsystem;

	UFUNCTION()
	void HandleClicked();
};

/**
 * The join screen. Fetches the list of games and makes a button for each one.
 *
 * In the widget Blueprint you only need a VerticalBox or ScrollBox named ServerListBox.
 * The buttons themselves are built in C++, so there's no row widget to make.
 */
UCLASS()
class NETWORKPR_API UServerBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Required - name it exactly this in the widget Blueprint
	UPROPERTY(meta = (BindWidget))
	UPanelWidget* ServerListBox;

	// Optional extras
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RefreshButton;
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	// Ask for the list again. Also runs automatically when the widget opens
	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	void Refresh();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleServerList(bool bWasSuccessful, const TArray<FGameSessionInfo>& Sessions);
	UFUNCTION()
	void HandleJoinResult(bool bWasSuccessful);

private:
	UMultiplayerSubsystem* GetMultiplayer() const;
	void SetStatus(const FString& Text);

	// Holds the row objects so they don't get garbage collected while their buttons exist
	UPROPERTY()
	TArray<TObjectPtr<UServerRowHandler>> RowHandlers;
};
