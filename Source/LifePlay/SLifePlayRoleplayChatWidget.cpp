#include "SLifePlayRoleplayChatWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SLifePlayRoleplayChatWidget::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	NpcActor = InArgs._NpcActor;
	PlayerActor = InArgs._PlayerActor;
	OnRequestClose = InArgs._OnRequestClose;

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(FMargin(0.0f, 28.0f, 24.0f, 76.0f))
	[
		SNew(SBox)
		.WidthOverride(440.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush)
			.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.90f))
			.Padding(14.0f)
			[
				SNew(SVerticalBox)

				// Header row
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(HeaderTitleText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity(FLinearColor(0.95f, 0.97f, 1.0f))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SAssignNew(HeaderStatsText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.92f))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Top)
					[
						SNew(SButton)
						.OnClicked(this, &SLifePlayRoleplayChatWidget::HandleCloseClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Esc")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
					]
				]

				// Chat history log
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 10.0f, 0.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.72f))
					.Padding(8.0f)
					[
						SAssignNew(MessageScrollBox, SScrollBox)
					]
				]

				// Row 1: Dialogue Roleplay Presets
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("greet")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Greet")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("ask_life")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Ask About Life")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("flirt")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Flirt")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
				]

				// Row 2: World Actions + Reset
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("follow")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Follow Me")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("stay")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Stay Here")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { TriggerInteraction(TEXT("provoke")); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Provoke")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked(this, &SLifePlayRoleplayChatWidget::HandleClearMemoryClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Reset Log")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
				]

				// Custom roleplay input bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(InputBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Type roleplay dialogue or command (Enter to say)...")))
						.OnTextCommitted(this, &SLifePlayRoleplayChatWidget::HandleInputCommitted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked(this, &SLifePlayRoleplayChatWidget::HandleSendClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Say")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]
					]
				]

				// Status footer
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(StatusText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.65f, 0.72f, 0.80f))
					.Text(FText::FromString(TEXT("Standalone Offline Roleplay — Click an action or type anything (Esc to close).")))
				]
			]
		]
	];

	RefreshHeader();
	RebuildMessageList();
}

void SLifePlayRoleplayChatWidget::FocusInput()
{
	if (InputBox.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
	}
}

FReply SLifePlayRoleplayChatWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnRequestClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SLifePlayRoleplayChatWidget::RefreshHeader()
{
	ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get();
	AActor* Npc = NpcActor.Get();
	if (!Sub || !Npc)
	{
		if (HeaderTitleText.IsValid())
		{
			HeaderTitleText->SetText(FText::FromString(TEXT("GTA Roleplay")));
		}
		if (HeaderStatsText.IsValid())
		{
			HeaderStatsText->SetText(FText::FromString(TEXT("No nearby NPC found")));
		}
		return;
	}

	const FLifePlayNpcContext Ctx = Sub->ExtractNpcContext(Npc);
	NpcId = Ctx.NpcId;

	if (HeaderTitleText.IsValid())
	{
		HeaderTitleText->SetText(FText::FromString(Ctx.FullName));
	}
	if (HeaderStatsText.IsValid())
	{
		HeaderStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("Age %d  |  %s  |  Mood: %s  |  Rapport: %.0f  |  Attraction: %.0f"),
			Ctx.Age,
			*Ctx.Job,
			*Ctx.Mood,
			Ctx.Rapport,
			Ctx.Attraction
		)));
	}
}

void SLifePlayRoleplayChatWidget::RebuildMessageList()
{
	if (!MessageScrollBox.IsValid())
	{
		return;
	}
	MessageScrollBox->ClearChildren();

	ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get();
	if (!Sub || NpcId.IsEmpty())
	{
		return;
	}

	const TArray<FLifePlayChatMessage> Messages = Sub->GetHistoryForNpc(NpcId);
	if (Messages.Num() == 0)
	{
		MessageScrollBox->AddSlot()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT("Click Greet, Ask About Life, Flirt, Follow Me, or type your own line below. All stats & memories save offline automatically.")))
			.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.72f))
		];
		return;
	}

	for (const FLifePlayChatMessage& Msg : Messages)
	{
		AddMessageRow(Msg);
	}
	MessageScrollBox->ScrollToEnd();
}

void SLifePlayRoleplayChatWidget::AddMessageRow(const FLifePlayChatMessage& Msg)
{
	if (!MessageScrollBox.IsValid())
	{
		return;
	}

	const bool bIsUser = Msg.Role.Equals(TEXT("user"), ESearchCase::IgnoreCase);
	const FLinearColor SpeakerColor = bIsUser
		? FLinearColor(0.45f, 0.85f, 1.0f)
		: FLinearColor(0.98f, 0.82f, 0.45f);

	FString SpeakerLabel = bIsUser ? TEXT("You") : ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(NpcActor.Get());
	if (!bIsUser)
	{
		FString Meta;
		if (!Msg.Emotion.IsEmpty() && Msg.Emotion != TEXT("neutral"))
		{
			Meta += Msg.Emotion;
		}
		if (!FMath::IsNearlyZero(Msg.RelationshipDelta))
		{
			if (!Meta.IsEmpty()) Meta += TEXT(", ");
			Meta += FString::Printf(TEXT("%+.0f rapport"), Msg.RelationshipDelta);
		}
		if (!FMath::IsNearlyZero(Msg.AttractionDelta))
		{
			if (!Meta.IsEmpty()) Meta += TEXT(", ");
			Meta += FString::Printf(TEXT("%+.0f attraction"), Msg.AttractionDelta);
		}
		if (!Msg.Action.IsEmpty() && Msg.Action != TEXT("none"))
		{
			if (!Meta.IsEmpty()) Meta += TEXT(", ");
			Meta += TEXT("action: ") + Msg.Action;
		}
		if (!Meta.IsEmpty())
		{
			SpeakerLabel += FString::Printf(TEXT(" (%s)"), *Meta);
		}
	}

	MessageScrollBox->AddSlot()
	.Padding(FMargin(2.0f, 4.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(SpeakerLabel))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(SpeakerColor)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 1.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(FText::FromString(Msg.Text))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(FLinearColor(0.92f, 0.94f, 0.98f))
		]
	];
}

void SLifePlayRoleplayChatWidget::SubmitCurrentMessage()
{
	if (!InputBox.IsValid())
	{
		return;
	}

	ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get();
	AActor* Npc = NpcActor.Get();
	if (!Sub || !Npc)
	{
		return;
	}

	const FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	InputBox->SetText(FText::GetEmpty());
	Sub->SendRoleplayMessage(Npc, PlayerActor.Get(), Text);
	RefreshHeader();
	RebuildMessageList();
	FocusInput();
}

void SLifePlayRoleplayChatWidget::TriggerInteraction(const FString& InteractionId)
{
	ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get();
	AActor* Npc = NpcActor.Get();
	if (!Sub || !Npc)
	{
		return;
	}
	Sub->TriggerPresetInteraction(Npc, PlayerActor.Get(), InteractionId);
	RefreshHeader();
	RebuildMessageList();
}

FReply SLifePlayRoleplayChatWidget::HandleSendClicked()
{
	SubmitCurrentMessage();
	return FReply::Handled();
}

void SLifePlayRoleplayChatWidget::HandleInputCommitted(const FText& /*NewText*/, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		SubmitCurrentMessage();
	}
}

FReply SLifePlayRoleplayChatWidget::HandleCloseClicked()
{
	OnRequestClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLifePlayRoleplayChatWidget::HandleClearMemoryClicked()
{
	if (ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get())
	{
		Sub->ClearNpcMemory(NpcId);
		RebuildMessageList();
	}
	return FReply::Handled();
}
