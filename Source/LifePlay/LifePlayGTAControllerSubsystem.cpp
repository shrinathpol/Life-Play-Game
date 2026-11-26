#include "LifePlayGTAControllerSubsystem.h"

#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "LifePlayAIRoleplaySubsystem.h"
#include "SLifePlayRoleplayChatWidget.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

AGtaCameraRigActor::AGtaCameraRigActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PivotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PivotRoot"));
	SetRootComponent(PivotRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(PivotRoot);
	SpringArm->TargetArmLength = 250.0f;
	SpringArm->SocketOffset = FVector(0.0f, 48.0f, 68.0f);
	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeSize = 14.0f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 14.0f;
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 72.0f;
	Camera->bUsePawnControlRotation = false;
}

bool ULifePlayGTAControllerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULifePlayGTAControllerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULifePlayGTAControllerSubsystem, STATGROUP_Tickables);
}

void ULifePlayGTAControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULifePlayGTAControllerSubsystem::Deinitialize()
{
	CloseRoleplayChat();
	if (bGtaModeEnabled)
	{
		SetGtaModeEnabled(false);
	}
	if (GEngine && GEngine->GameViewport && HudContainer.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(HudContainer.ToSharedRef());
	}
	HudContainer.Reset();
	HudTextBlock.Reset();
	Super::Deinitialize();
}

bool ULifePlayGTAControllerSubsystem::IsTextInputFocused() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!Focused.IsValid())
	{
		return false;
	}
	const FName WidgetType = Focused->GetType();
	return WidgetType == FName(TEXT("SEditableText"))
		|| WidgetType == FName(TEXT("SEditableTextBox"))
		|| WidgetType == FName(TEXT("SMultiLineEditableTextBox"));
}

void ULifePlayGTAControllerSubsystem::EnsureHudOverlay()
{
	if (HudContainer.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	HudContainer = SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 18.0f))
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush)
			.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.78f))
			.Padding(FMargin(14.0f, 6.0f))
			[
				SAssignNew(HudTextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f))
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(HudContainer.ToSharedRef(), 20);
	UpdateHudText();
}

void ULifePlayGTAControllerSubsystem::UpdateHudText()
{
	if (!HudTextBlock.IsValid())
	{
		return;
	}

	FString NpcPart;
	if (ACharacter* Npc = CachedNearestNpc.Get())
	{
		NpcPart = FString::Printf(TEXT("  |  [E / T] Roleplay: %s"), *ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(Npc));
	}
	else
	{
		NpcPart = TEXT("  |  [E / T] Roleplay");
	}

	if (bGtaModeEnabled)
	{
		HudTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("GTA MODE ON  |  WASD Move  |  Shift Sprint  |  Ctrl Walk  |  Alt Cursor  |  [G] Free Cam%s"),
			*NpcPart
		)));
	}
	else
	{
		HudTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("[G] GTA Character Control%s"),
			*NpcPart
		)));
	}
}

ACharacter* ULifePlayGTAControllerSubsystem::ResolvePlayerCharacter(UWorld* World, APlayerController* PC)
{
	if (ACharacter* Existing = CachedPlayerChar.Get())
	{
		if (IsValid(Existing))
		{
			return Existing;
		}
	}

	auto ReadPlayerProp = [](UObject* Container) -> ACharacter*
	{
		if (!Container)
		{
			return nullptr;
		}
		for (TFieldIterator<FObjectProperty> It(Container->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(TEXT("PlayerCharacter"), ESearchCase::IgnoreCase))
			{
				if (UObject* Val = It->GetObjectPropertyValue_InContainer(Container))
				{
					return Cast<ACharacter>(Val);
				}
			}
		}
		return nullptr;
	};

	if (PC)
	{
		if (ACharacter* FromPC = ReadPlayerProp(PC))
		{
			CachedPlayerChar = FromPC;
			return FromPC;
		}
	}
	if (World)
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (ACharacter* FromGI = ReadPlayerProp(GI))
			{
				CachedPlayerChar = FromGI;
				return FromGI;
			}
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Char = *It;
			if (ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(Char))
			{
				CachedPlayerChar = Char;
				return Char;
			}
		}
	}
	return nullptr;
}

ACharacter* ULifePlayGTAControllerSubsystem::FindNearestNpc(UWorld* World, const FVector& Origin, float MaxDistance) const
{
	if (!World)
	{
		return nullptr;
	}
	ACharacter* BestNpc = nullptr;
	float BestDistSq = MaxDistance * MaxDistance;
	ACharacter* PlayerChar = CachedPlayerChar.Get();

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == PlayerChar)
		{
			continue;
		}
		if (!ULifePlayAIRoleplaySubsystem::IsPersonActor(Candidate) || ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(Candidate))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestNpc = Candidate;
		}
	}
	return BestNpc;
}

AGtaCameraRigActor* ULifePlayGTAControllerSubsystem::EnsureCameraRig(UWorld* World)
{
	if (AGtaCameraRigActor* Existing = CameraRig.Get())
	{
		if (IsValid(Existing))
		{
			return Existing;
		}
	}
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGtaCameraRigActor* Spawned = World->SpawnActor<AGtaCameraRigActor>(AGtaCameraRigActor::StaticClass(), FTransform::Identity, Params);
	CameraRig = Spawned;
	return Spawned;
}

void ULifePlayGTAControllerSubsystem::ApplyInputLockForGta(APlayerController* PC, bool bLockMouse)
{
	if (!PC)
	{
		return;
	}
	if (bLockMouse)
	{
		FInputModeGameOnly GameOnly;
		GameOnly.SetConsumeCaptureMouseDown(true);
		PC->SetInputMode(GameOnly);
		PC->bShowMouseCursor = false;
	}
	else
	{
		FInputModeGameAndUI GameAndUI;
		GameAndUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		GameAndUI.SetHideCursorDuringCapture(false);
		PC->SetInputMode(GameAndUI);
		PC->bShowMouseCursor = true;
	}
}

void ULifePlayGTAControllerSubsystem::ToggleGtaMode()
{
	SetGtaModeEnabled(!bGtaModeEnabled);
}

void ULifePlayGTAControllerSubsystem::SetGtaModeEnabled(bool bEnable)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	ACharacter* PlayerChar = ResolvePlayerCharacter(World, PC);
	if (bEnable && !PlayerChar)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Start or load a LifePlay game first before activating GTA Control."));
		}
		return;
	}

	if (bEnable == bGtaModeEnabled)
	{
		return;
	}

	bGtaModeEnabled = bEnable;
	bAltCursorUnlock = false;

	if (bGtaModeEnabled)
	{
		PreviousViewTarget = PC->GetViewTarget();
		PreviousControlledPawn = PC->GetPawn();
		if (APawn* PrevPawn = PreviousControlledPawn.Get())
		{
			if (PrevPawn != PlayerChar)
			{
				PrevPawn->DisableInput(PC);
			}
		}

		CameraYaw = PlayerChar->GetActorRotation().Yaw;
		CameraPitch = -12.0f;

		if (UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement())
		{
			bSavedOrientToMovement = MoveComp->bOrientRotationToMovement;
			bSavedRunPhysicsNoController = MoveComp->bRunPhysicsWithNoController;
			bSavedUseControllerDesiredRot = MoveComp->bUseControllerDesiredRotation;
			SavedMaxWalkSpeed = MoveComp->MaxWalkSpeed;

			MoveComp->bOrientRotationToMovement = true;
			MoveComp->bRunPhysicsWithNoController = true;
			MoveComp->bUseControllerDesiredRotation = false;
			MoveComp->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		}
		bSavedUseControllerRotationYaw = PlayerChar->bUseControllerRotationYaw;
		PlayerChar->bUseControllerRotationYaw = false;

		if (AGtaCameraRigActor* Rig = EnsureCameraRig(World))
		{
			Rig->SetActorLocationAndRotation(PlayerChar->GetActorLocation(), FRotator(CameraPitch, CameraYaw, 0.0f));
			Rig->SpringArm->TargetArmLength = TargetArmLength;
			PC->SetViewTargetWithBlend(Rig, 0.25f);
		}

		if (!bChatOpen)
		{
			ApplyInputLockForGta(PC, true);
		}
	}
	else
	{
		if (PlayerChar && IsValid(PlayerChar))
		{
			if (UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement())
			{
				MoveComp->bOrientRotationToMovement = bSavedOrientToMovement;
				MoveComp->bRunPhysicsWithNoController = bSavedRunPhysicsNoController;
				MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRot;
				MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed;
			}
			PlayerChar->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
		}

		if (APawn* PrevPawn = PreviousControlledPawn.Get())
		{
			if (IsValid(PrevPawn))
			{
				if (PlayerChar && IsValid(PlayerChar) && PrevPawn != PlayerChar)
				{
					const FVector CamBack = PlayerChar->GetActorLocation() - FRotator(0.0f, CameraYaw, 0.0f).Vector() * 240.0f + FVector(0.0f, 0.0f, 110.0f);
					PrevPawn->SetActorLocation(CamBack);
				}
				PrevPawn->EnableInput(PC);
			}
		}

		if (AActor* PrevTarget = PreviousViewTarget.Get())
		{
			if (IsValid(PrevTarget))
			{
				PC->SetViewTargetWithBlend(PrevTarget, 0.25f);
			}
		}
		ApplyInputLockForGta(PC, false);
	}

	UpdateHudText();
}

void ULifePlayGTAControllerSubsystem::ToggleRoleplayChat()
{
	if (bChatOpen)
	{
		CloseRoleplayChat();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	ACharacter* PlayerChar = ResolvePlayerCharacter(World, PC);
	const FVector SearchOrigin = PlayerChar
		? PlayerChar->GetActorLocation()
		: (PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector);

	ACharacter* TargetNpc = CachedNearestNpc.Get();
	if (!TargetNpc || !IsValid(TargetNpc))
	{
		TargetNpc = FindNearestNpc(World, SearchOrigin, 2500.0f);
	}

	if (!TargetNpc)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Walk closer to an NPC in a city scene before starting AI Roleplay."));
		}
		return;
	}

	ULifePlayAIRoleplaySubsystem* AISub = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
		: nullptr;
	if (!AISub)
	{
		return;
	}

	AISub->ExecuteNpcAction(TargetNpc, PlayerChar, TEXT("face_player"));

	ChatWidget = SNew(SLifePlayRoleplayChatWidget)
		.Subsystem(AISub)
		.NpcActor(TargetNpc)
		.PlayerActor(PlayerChar)
		.OnRequestClose(FSimpleDelegate::CreateUObject(this, &ULifePlayGTAControllerSubsystem::CloseRoleplayChat));

	ChatContainer = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			ChatWidget.ToSharedRef()
		];

	GEngine->GameViewport->AddViewportWidgetContent(ChatContainer.ToSharedRef(), 50);
	bChatOpen = true;

	if (PC)
	{
		ApplyInputLockForGta(PC, false);
	}
	ChatWidget->FocusInput();
}

void ULifePlayGTAControllerSubsystem::CloseRoleplayChat()
{
	if (!bChatOpen)
	{
		return;
	}
	if (GEngine && GEngine->GameViewport && ChatContainer.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ChatContainer.ToSharedRef());
	}
	ChatContainer.Reset();
	ChatWidget.Reset();
	bChatOpen = false;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			ApplyInputLockForGta(PC, bGtaModeEnabled && !bAltCursorUnlock);
		}
	}
}

void ULifePlayGTAControllerSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	ACharacter* PlayerChar = ResolvePlayerCharacter(World, PC);
	if (PlayerChar)
	{
		EnsureHudOverlay();
	}

	NpcScanTimer += DeltaTime;
	if (NpcScanTimer >= 0.25f)
	{
		NpcScanTimer = 0.0f;
		const FVector Origin = PlayerChar
			? PlayerChar->GetActorLocation()
			: (PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector);
		ACharacter* NewNearest = FindNearestNpc(World, Origin, 450.0f);
		if (NewNearest != CachedNearestNpc.Get())
		{
			CachedNearestNpc = NewNearest;
			UpdateHudText();
		}
	}

	if (bChatOpen)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape))
		{
			CloseRoleplayChat();
		}
		return;
	}

	const bool bCtrlDown = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
	const bool bAltDown = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	const bool bShiftDown = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);

	if (!IsTextInputFocused() && !bCtrlDown && !bAltDown && !bShiftDown)
	{
		if (PC->WasInputKeyJustPressed(EKeys::G))
		{
			ToggleGtaMode();
		}
		else if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::T))
		{
			ToggleRoleplayChat();
			return;
		}
	}

	if (!bGtaModeEnabled || !PlayerChar || !IsValid(PlayerChar))
	{
		return;
	}

	// Temporary LeftAlt cursor unlock for clicking LifePlay HUD buttons while staying in GTA Mode
	const bool bWantAltUnlock = PC->IsInputKeyDown(EKeys::LeftAlt);
	if (bWantAltUnlock != bAltCursorUnlock)
	{
		bAltCursorUnlock = bWantAltUnlock;
		ApplyInputLockForGta(PC, !bAltCursorUnlock);
	}

	if (!bAltCursorUnlock)
	{
		float MouseDX = 0.0f;
		float MouseDY = 0.0f;
		PC->GetInputMouseDelta(MouseDX, MouseDY);
		CameraYaw += MouseDX * 2.2f;
		CameraPitch = FMath::Clamp(CameraPitch - MouseDY * 2.0f, -40.0f, 55.0f);

		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			TargetArmLength = FMath::Clamp(TargetArmLength - 20.0f, 140.0f, 450.0f);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			TargetArmLength = FMath::Clamp(TargetArmLength + 20.0f, 140.0f, 450.0f);
		}
	}

	if (AGtaCameraRigActor* Rig = EnsureCameraRig(World))
	{
		Rig->SetActorLocationAndRotation(PlayerChar->GetActorLocation(), FRotator(CameraPitch, CameraYaw, 0.0f));
		Rig->SpringArm->TargetArmLength = TargetArmLength;
	}

	if (IsTextInputFocused())
	{
		return;
	}

	float ForwardInput = 0.0f;
	float RightInput = 0.0f;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up)) ForwardInput += 1.0f;
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) ForwardInput -= 1.0f;
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) RightInput += 1.0f;
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) RightInput -= 1.0f;

	if (!FMath::IsNearlyZero(ForwardInput) || !FMath::IsNearlyZero(RightInput))
	{
		if (AAIController* AI = Cast<AAIController>(PlayerChar->GetController()))
		{
			AI->StopMovement();
		}

		if (UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement())
		{
			MoveComp->bRunPhysicsWithNoController = true;
			MoveComp->bOrientRotationToMovement = true;
			if (MoveComp->MovementMode == MOVE_None)
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
			const float TargetSpeed = PC->IsInputKeyDown(EKeys::LeftShift)
				? 520.0f
				: (PC->IsInputKeyDown(EKeys::LeftControl) ? 150.0f : 300.0f);
			MoveComp->MaxWalkSpeed = TargetSpeed;
		}

		const FRotator YawRot(0.0f, CameraYaw, 0.0f);
		const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		const FVector MoveDir = (ForwardDir * ForwardInput + RightDir * RightInput).GetSafeNormal();

		PlayerChar->AddMovementInput(MoveDir, 1.0f, true);

		const FRotator DesiredYawRot(0.0f, MoveDir.Rotation().Yaw, 0.0f);
		PlayerChar->SetActorRotation(FMath::RInterpTo(PlayerChar->GetActorRotation(), DesiredYawRot, DeltaTime, 10.0f));
	}
}
