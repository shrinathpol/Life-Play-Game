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
	OnAudioMuteToggled = InArgs._OnAudioMuteToggled;

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto MakeQuickBtn = [this](const TCHAR* Label, const TCHAR* ActionId) -> TSharedRef<SWidget>
	{
		const FString IdCopy(ActionId);
		return SNew(SButton)
			.OnClicked_Lambda([this, IdCopy]() { TriggerInteraction(IdCopy); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			];
	};

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(FMargin(0.0f, 24.0f, 24.0f, 76.0f))
	[
		SNew(SBox)
		.WidthOverride(480.0f)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush)
			.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.92f))
			.Padding(0.0f)
			[
				SNew(SVerticalBox)

				// Top Cyan Accent Bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(3.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.18f, 0.82f, 0.98f, 1.0f))
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
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
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("LIFEPLAY v1.0  //  INTERACTION AI")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
							.ColorAndOpacity(FLinearColor(0.22f, 0.88f, 0.98f))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 1.0f, 0.0f, 0.0f)
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
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.92f))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Top)
					[
						SNew(SButton)
						.OnClicked(this, &SLifePlayRoleplayChatWidget::HandleToggleAudioClicked)
						[
							SAssignNew(AudioToggleText, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
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

				// Row 1: Social Role-Play Actions
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ MakeQuickBtn(TEXT("Greet"), TEXT("greet")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Ask Life"), TEXT("ask_life")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Joke"), TEXT("joke")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Flirt"), TEXT("flirt")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Gift £25"), TEXT("gift")) ]
				]

				// Row 2: Unreal Engine Interaction AI Actions
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ MakeQuickBtn(TEXT("Follow Me"), TEXT("follow")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Stay"), TEXT("stay")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Call Over"), TEXT("call_over")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Apologize"), TEXT("apologize")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)[ MakeQuickBtn(TEXT("Provoke"), TEXT("provoke")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.OnClicked(this, &SLifePlayRoleplayChatWidget::HandleClearMemoryClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Reset")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
				]

				// Free-text standalone input bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(InputBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Say anything in-character (Enter to speak)...")))
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
					.Text(FText::FromString(TEXT("UE5 Interaction AI + Audio Soundplay (100% Standalone Offline) — Esc to close.")))
				]
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

	if (AudioToggleText.IsValid())
	{
		const bool bMuted = Sub && Sub->IsAudioMuted();
		AudioToggleText->SetText(FText::FromString(bMuted ? TEXT("Audio: OFF") : TEXT("Audio: ON")));
	}

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
			TEXT("Age %d | %s | Mood: %s | Rapport: %.0f | Attr: %.0f | AI: %s"),
			Ctx.Age,
			*Ctx.Job,
			*Ctx.Mood,
			Ctx.Rapport,
			Ctx.Attraction,
			*Sub->GetNpcAIStateName(Npc)
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
			.Text(FText::FromString(TEXT("Use GTA Roleplay actions (Greet, Joke, Flirt, Gift £25, Follow Me, Call Over, Provoke) or type below. Powered by Unreal Engine Interaction AI & 3D Audio.")))
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
			Meta += FString::Printf(TEXT("%+.0f attr"), Msg.AttractionDelta);
		}
		if (!Msg.Action.IsEmpty() && Msg.Action != TEXT("none"))
		{
			if (!Meta.IsEmpty()) Meta += TEXT(", ");
			Meta += TEXT("AI: ") + Msg.Action;
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

FReply SLifePlayRoleplayChatWidget::HandleToggleAudioClicked()
{
	if (ULifePlayAIRoleplaySubsystem* Sub = Subsystem.Get())
	{
		Sub->ToggleAudioMute();
		OnAudioMuteToggled.ExecuteIfBound();
		RefreshHeader();
	}
	return FReply::Handled();
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
