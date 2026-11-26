#pragma once

#include "CoreMinimal.h"
#include "LifePlayAIRoleplaySubsystem.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SEditableTextBox;
class SScrollBox;
class STextBlock;

/**
 * Right-docked 100% standalone offline Slate role-play panel with GTA quick actions.
 */
class SLifePlayRoleplayChatWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLifePlayRoleplayChatWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULifePlayAIRoleplaySubsystem>, Subsystem)
		SLATE_ARGUMENT(TWeakObjectPtr<AActor>, NpcActor)
		SLATE_ARGUMENT(TWeakObjectPtr<AActor>, PlayerActor)
		SLATE_EVENT(FSimpleDelegate, OnRequestClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void FocusInput();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<ULifePlayAIRoleplaySubsystem> Subsystem;
	TWeakObjectPtr<AActor> NpcActor;
	TWeakObjectPtr<AActor> PlayerActor;
	FSimpleDelegate OnRequestClose;

	FString NpcId;

	TSharedPtr<STextBlock> HeaderTitleText;
	TSharedPtr<STextBlock> HeaderStatsText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SScrollBox> MessageScrollBox;
	TSharedPtr<SEditableTextBox> InputBox;

	void RefreshHeader();
	void RebuildMessageList();
	void AddMessageRow(const FLifePlayChatMessage& Msg);
	void SubmitCurrentMessage();
	void TriggerInteraction(const FString& InteractionId);

	FReply HandleSendClicked();
	void HandleInputCommitted(const FText& NewText, ETextCommit::Type CommitType);
	FReply HandleCloseClicked();
	FReply HandleClearMemoryClicked();
};
