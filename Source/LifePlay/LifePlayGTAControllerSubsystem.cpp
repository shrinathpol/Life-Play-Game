#include "LifePlayGTAControllerSubsystem.h"

#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateImageBrush.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LifePlayAIRoleplaySubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SLifePlayGtaMapWidget.h"
#include "SLifePlayRoleplayChatWidget.h"
#include "Sound/SoundBase.h"
#include "StreetMap.h"
#include "StreetMapComponent.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TextureResource.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

AGtaCameraRigActor::AGtaCameraRigActor() {
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

bool ULifePlayGTAControllerSubsystem::DoesSupportWorldType(
    const EWorldType::Type WorldType) const {
  return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULifePlayGTAControllerSubsystem::GetStatId() const {
  RETURN_QUICK_DECLARE_CYCLE_STAT(ULifePlayGTAControllerSubsystem,
                                  STATGROUP_Tickables);
}

void ULifePlayGTAControllerSubsystem::Initialize(
    FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);
  GAreScreenMessagesEnabled = false;
  MapViewState = MakeShared<FGtaMapViewState>();
  bOnMainMenu = false;
  bMainMenuRebranded = false;
  bLoadingCurtainActive = false;
  bLoadingCurtainDismissed = false;
  bTitleCharClothed = false;
  LoadingElapsed = 0.0f;
  BufferTimeRemaining = 2.0f;
  LoadingProgress = 0.05f;
  LoadingPrepTimer = 0.0f;
  DressRetryTimer = 0.0f;
  DressRetryCount = 0;
  LoadingStatusText = TEXT("Loading game assets & clothing...");
  LoadHudConfig();
}

void ULifePlayGTAControllerSubsystem::LoadHudConfig() {
  TArray<FString> Candidates;
  Candidates.Add(FPaths::Combine(FPaths::ProjectConfigDir(),
                                 TEXT("LifePlayHUD.ini")));
  Candidates.Add(FPaths::Combine(FPaths::LaunchDir(), TEXT("LifePlayHUD.ini")));
  Candidates.Add(
      FPaths::Combine(FPaths::ProjectDir(), TEXT("LifePlayHUD.ini")));
  Candidates.Add(FPaths::Combine(
      FPaths::ConvertRelativePathToFull(
          FPaths::Combine(FPaths::LaunchDir(), TEXT("../../.."))),
      TEXT("LifePlayHUD.ini")));

  FString LoadedPath(TEXT("Defaults"));
  for (const FString &Path : Candidates) {
    if (!FPaths::FileExists(Path)) {
      continue;
    }
    FConfigFile Cfg;
    Cfg.Read(Path);
    const TCHAR *Sec = TEXT("LifePlayHUD");
    Cfg.GetBool(Sec, TEXT("bShowMinimap"), bHudShowMinimap);
    Cfg.GetBool(Sec, TEXT("bShowTopStatusCard"), bHudShowTopStatusCard);
    Cfg.GetBool(Sec, TEXT("bShowBottomControlsBar"), bHudShowBottomControlsBar);
    Cfg.GetString(Sec, TEXT("MinimapAnchor"), HudMinimapAnchor);
    Cfg.GetFloat(Sec, TEXT("MinimapWidth"), HudMinimapWidth);
    Cfg.GetFloat(Sec, TEXT("MinimapHeight"), HudMinimapHeight);
    Cfg.GetFloat(Sec, TEXT("MinimapOffsetX"), HudMinimapOffsetX);
    Cfg.GetFloat(Sec, TEXT("MinimapOffsetY"), HudMinimapOffsetY);
    LoadedPath = Path;
  }

  HudMinimapWidth = FMath::Clamp(HudMinimapWidth, 120.0f, 420.0f);
  HudMinimapHeight = FMath::Clamp(HudMinimapHeight, 80.0f, 300.0f);
  HudMinimapOffsetX = FMath::Clamp(HudMinimapOffsetX, 0.0f, 400.0f);
  HudMinimapOffsetY = FMath::Clamp(HudMinimapOffsetY, 0.0f, 400.0f);

  UE_LOG(LogTemp, Display,
         TEXT("[LifePlayHUD] Config='%s' TopCard=%d Minimap=%d BottomBar=%d "
              "Anchor=%s Size=%.0fx%.0f Offset=%.0f,%.0f"),
         *LoadedPath, bHudShowTopStatusCard ? 1 : 0, bHudShowMinimap ? 1 : 0,
         bHudShowBottomControlsBar ? 1 : 0, *HudMinimapAnchor, HudMinimapWidth,
         HudMinimapHeight, HudMinimapOffsetX, HudMinimapOffsetY);
}

void ULifePlayGTAControllerSubsystem::Deinitialize() {
  CloseRoleplayChat();
  CloseWorldMap();
  StartOrStopStreetAmbience(GetWorld(), false);
  if (bGtaModeEnabled) {
    SetGtaModeEnabled(false);
  }
  if (GEngine && GEngine->GameViewport) {
    if (LoadingCurtainContainer.IsValid()) {
      GEngine->GameViewport->RemoveViewportWidgetContent(
          LoadingCurtainContainer.ToSharedRef());
    }
    if (HudContainer.IsValid()) {
      GEngine->GameViewport->RemoveViewportWidgetContent(
          HudContainer.ToSharedRef());
    }
    if (MinimapContainer.IsValid()) {
      GEngine->GameViewport->RemoveViewportWidgetContent(
          MinimapContainer.ToSharedRef());
    }
  }
  LoadingCurtainContainer.Reset();
  HudContainer.Reset();
  MinimapContainer.Reset();
  BottomHudBorder.Reset();
  TopStatusModeText.Reset();
  TopStatusNpcText.Reset();
  TopStatusAudioText.Reset();
  HudTextBlock.Reset();
  BrandLogoBrush.Reset();
  BrandLogoTexture = nullptr;
  CachedStreetMapData.Reset();
  MapViewState.Reset();
  ProximityReactionTimestamps.Empty();
  Super::Deinitialize();
}

void ULifePlayGTAControllerSubsystem::LoadBrandLogoBrush() {
  if (BrandLogoBrush.IsValid()) {
    return;
  }

  const FString LogoPath = FPaths::Combine(
      FPaths::ProjectContentDir(), TEXT("UI/CustomBrand/LifePlay_Logo.png"));
  TArray<uint8> FileData;
  if (!FFileHelper::LoadFileToArray(FileData, *LogoPath) ||
      FileData.Num() == 0) {
    return;
  }

  IImageWrapperModule &ImageWrapperModule =
      FModuleManager::LoadModuleChecked<IImageWrapperModule>(
          FName("ImageWrapper"));
  TSharedPtr<IImageWrapper> ImageWrapper =
      ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
  if (!ImageWrapper.IsValid() ||
      !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num())) {
    return;
  }

  TArray<uint8> RawBGRA;
  if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA) ||
      RawBGRA.Num() == 0) {
    return;
  }

  const int32 Width = ImageWrapper->GetWidth();
  const int32 Height = ImageWrapper->GetHeight();
  UTexture2D *Tex = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
  if (!Tex || !Tex->GetPlatformData() ||
      Tex->GetPlatformData()->Mips.Num() == 0) {
    return;
  }

  Tex->SRGB = true;
  void *MipData =
      Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
  FMemory::Memcpy(MipData, RawBGRA.GetData(), RawBGRA.Num());
  Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
  Tex->UpdateResource();

  BrandLogoTexture = Tex;
  TSharedPtr<FSlateBrush> NewBrush = MakeShared<FSlateBrush>();
  NewBrush->SetResourceObject(Tex);
  NewBrush->SetImageSize(
      FVector2D(static_cast<float>(Width), static_cast<float>(Height)));
  NewBrush->DrawAs = ESlateBrushDrawType::Image;
  BrandLogoBrush = NewBrush;
}

void ULifePlayGTAControllerSubsystem::CleanAndRebrandGameWidgets(
    UWorld *World) {
  if (!World) {
    return;
  }

  LoadBrandLogoBrush();

  auto CollapseWidget = [](UWidget *W) {
    if (!W)
      return;
    W->VisibilityDelegate.Unbind();
    if (W->GetVisibility() != ESlateVisibility::Collapsed) {
      W->SetVisibility(ESlateVisibility::Collapsed);
    }
  };

  for (TObjectIterator<UUserWidget> It; It; ++It) {
    UUserWidget *UW = *It;
    if (!UW || !IsValid(UW) || UW->GetWorld() != World || !UW->IsInViewport() ||
        !UW->WidgetTree) {
      continue;
    }

    if (UW->GetClass()->GetName().Contains(TEXT("MainMenu"))) {
      bMainMenuRebranded = true;
    }

    UW->WidgetTree->ForEachWidget([&](UWidget *Child) {
      if (!Child || !IsValid(Child)) {
        return;
      }

      const FString Name = Child->GetName();
      if (Name.Equals(TEXT("DonateButton"), ESearchCase::IgnoreCase) ||
          Name.Equals(TEXT("DiscordButton"), ESearchCase::IgnoreCase) ||
          Name.Equals(TEXT("CommissionButton"), ESearchCase::IgnoreCase) ||
          Name.Equals(TEXT("AdButton"), ESearchCase::IgnoreCase) ||
          Name.StartsWith(TEXT("DonorsText"), ESearchCase::IgnoreCase)) {
        CollapseWidget(Child);
        return;
      }

      if (UTextBlock *TB = Cast<UTextBlock>(Child)) {
        const FString TextStr = TB->GetText().ToString();
        const FString Lower = TextStr.ToLower();
        const FString NameLower = Name.ToLower();

        if (Lower.Contains(TEXT("open-source")) ||
            Lower.Contains(TEXT("open source"))) {
          TB->TextDelegate.Unbind();
          TB->SetText(FText::FromString(TEXT("Open World Life & Roleplay")));
        } else if (Lower.Contains(TEXT("v5.9")) ||
                   Lower.Contains(TEXT("lifeplay 5.9")) ||
                   (NameLower.Contains(TEXT("version")) &&
                    !Lower.Equals(TEXT("lifeplay v1.0")))) {
          TB->TextDelegate.Unbind();
          TB->SetText(FText::FromString(TEXT("LifePlay v1.0")));
        } else if (Lower.Contains(TEXT("generous patrons")) ||
                   NameLower.Contains(TEXT("donor"))) {
          CollapseWidget(TB);
        } else if (Lower.Contains(TEXT("patreon")) ||
                   Lower.Contains(TEXT("donate")) ||
                   Lower.Contains(TEXT("discord")) ||
                   Lower.Contains(TEXT("commission"))) {
          UWidget *TargetToCollapse = TB;
          for (UPanelWidget *Parent = TB->GetParent(); Parent;
               Parent = Parent->GetParent()) {
            if (UButton *ParentBtn = Cast<UButton>(Parent)) {
              TargetToCollapse = ParentBtn;
              break;
            }
          }
          CollapseWidget(TargetToCollapse);
        }
      } else if (UImage *Img = Cast<UImage>(Child)) {
        if (BrandLogoTexture) {
          const UObject *Res = Img->GetBrush().GetResourceObject();
          const FString ResName = Res ? Res->GetName() : FString();
          if (ResName.Contains(TEXT("LP-Logo")) ||
              ResName.Contains(TEXT("Logo_256"))) {
            if (Res != BrandLogoTexture) {
              Img->SetBrushFromTexture(BrandLogoTexture, false);
            }
          }
        }
      }
    });
  }
}

bool ULifePlayGTAControllerSubsystem::IsMainMenuVisible(UWorld *World) const {
  if (!World) {
    return false;
  }

  for (TObjectIterator<UUserWidget> It; It; ++It) {
    UUserWidget *UW = *It;
    if (!UW || !IsValid(UW) || UW->GetWorld() != World || !UW->IsInViewport()) {
      continue;
    }
    if (UW->GetClass()->GetName().Contains(TEXT("MainMenu"))) {
      const ESlateVisibility Vis = UW->GetVisibility();
      if (Vis != ESlateVisibility::Collapsed &&
          Vis != ESlateVisibility::Hidden) {
        return true;
      }
    }
  }
  return false;
}

bool ULifePlayGTAControllerSubsystem::IsSetupMenuVisible(UWorld *World) const {
  if (!World) {
    return false;
  }

  for (TObjectIterator<UUserWidget> It; It; ++It) {
    UUserWidget *UW = *It;
    if (!UW || !IsValid(UW) || UW->GetWorld() != World || !UW->IsInViewport()) {
      continue;
    }
    const FString ClassName = UW->GetClass()->GetName();
    static const TCHAR *SetupMenuPrefixes[] = {
        TEXT("CityMenu"), TEXT("CharGen"),   TEXT("LoadMenu"),
        TEXT("NewGame"),  TEXT("Loading"),   TEXT("Builder"),
        TEXT("StatMenu"), TEXT("ContactMenu"), TEXT("BodyShapeMenu"),
        TEXT("ModManager"), TEXT("Inventory"), TEXT("Journal"),
        TEXT("Quest"),    TEXT("Perk"),      TEXT("Tutorial"),
        TEXT("FAQ"),      TEXT("Preset"),    TEXT("SaveSlot"),
        TEXT("BuildingEdit"), TEXT("SexInterface")};
    bool bMatchesSetup = false;
    for (const TCHAR *Prefix : SetupMenuPrefixes) {
      if (ClassName.Contains(Prefix)) {
        bMatchesSetup = true;
        break;
      }
    }
    if (bMatchesSetup) {
      const ESlateVisibility Vis = UW->GetVisibility();
      if (Vis != ESlateVisibility::Collapsed &&
          Vis != ESlateVisibility::Hidden) {
        return true;
      }
    }
  }
  return false;
}

void ULifePlayGTAControllerSubsystem::EnsureTitleCharacterClothed(
    UWorld *World) {
  if (!World) {
    return;
  }

  for (TActorIterator<ACharacter> It(World); It; ++It) {
    ACharacter *Char = *It;
    if (!IsValid(Char) || !ULifePlayAIRoleplaySubsystem::IsPersonActor(Char)) {
      continue;
    }

    TArray<USkeletalMeshComponent *> SkelComps;
    Char->GetComponents<USkeletalMeshComponent>(SkelComps);

    int32 ClothingCount = 0;
    for (USkeletalMeshComponent *Comp : SkelComps) {
      if (!Comp || !IsValid(Comp)) {
        continue;
      }

      const FString CompLower = Comp->GetName().ToLower();
      if (CompLower.Contains(TEXT("penis")) ||
          CompLower.Contains(TEXT("genital"))) {
        if (bOnMainMenu || bLoadingCurtainActive) {
          Comp->SetVisibility(false, true);
          Comp->SetHiddenInGame(true, true);
        }
        continue;
      }

      if (Comp == Char->GetMesh() || !Comp->GetSkeletalMeshAsset() ||
          !Comp->IsVisible() || Comp->bHiddenInGame) {
        continue;
      }

      if (CompLower.Contains(TEXT("hair")) || CompLower.Contains(TEXT("eye")) ||
          CompLower.Contains(TEXT("teeth")) ||
          CompLower.Contains(TEXT("lash")) ||
          CompLower.Contains(TEXT("brow")) ||
          CompLower.Contains(TEXT("head")) ||
          CompLower.Contains(TEXT("face")) ||
          CompLower.Contains(TEXT("pubic"))) {
        continue;
      }

      ++ClothingCount;
    }

    if (ClothingCount > 0) {
      bTitleCharClothed = true;
      continue;
    }

    if (bOnMainMenu && DressRetryCount < 4 && DressRetryTimer <= 0.0f) {
      static const TCHAR *DressFuncNames[] = {TEXT("QuickDress"),
                                              TEXT("GiveDefaultClothes"),
                                              TEXT("Redress"), TEXT("Dress")};
      for (const TCHAR *FuncName : DressFuncNames) {
        if (UFunction *Func = Char->FindFunction(FName(FuncName))) {
          if (Func->NumParms == 0) {
            Char->ProcessEvent(Func, nullptr);
            ++DressRetryCount;
            DressRetryTimer = 1.0f;
            break;
          }
        }
      }
    }
  }
}

void ULifePlayGTAControllerSubsystem::EnsureLoadingCurtain() {
  if (LoadingCurtainContainer.IsValid() || !GEngine || !GEngine->GameViewport) {
    return;
  }

  LoadBrandLogoBrush();
  const FSlateBrush *WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

  TSharedRef<SVerticalBox> CenterBox = SNew(SVerticalBox);
  if (BrandLogoBrush.IsValid()) {
    CenterBox->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(0.0f, 0.0f, 0.0f,
                 18.0f)[SNew(SBox).WidthOverride(520.0f).HeightOverride(
            292.0f)[SNew(SImage).Image(BrandLogoBrush.Get())]];
  }

  CenterBox->AddSlot().AutoHeight().HAlign(
      HAlign_Center)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("LIFEPLAY  v1.0")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
                         .ColorAndOpacity(FLinearColor(0.22f, 0.88f, 0.98f))];

  CenterBox->AddSlot()
      .AutoHeight()
      .HAlign(HAlign_Center)
      .Padding(0.0f, 6.0f, 0.0f,
               22.0f)[SNew(STextBlock)
                          .Text(FText::FromString(
                              TEXT("Open World Life & Roleplay")))
                          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                          .ColorAndOpacity(FLinearColor(0.76f, 0.84f, 0.94f))];

  CenterBox->AddSlot().AutoHeight().HAlign(HAlign_Center)
      [SNew(SBorder)
           .BorderImage(WhiteBrush)
           .BorderBackgroundColor(FLinearColor(0.08f, 0.11f, 0.16f, 0.95f))
           .Padding(
               FMargin(2.0f))[SNew(SBox).WidthOverride(480.0f).HeightOverride(
               8.0f)[SNew(SProgressBar)
                         .FillColorAndOpacity(
                             FLinearColor(0.18f, 0.82f, 0.98f, 1.0f))
                         .Percent_Lambda([this]() -> TOptional<float> {
                           return LoadingProgress;
                         })]]];

  CenterBox->AddSlot()
      .AutoHeight()
      .HAlign(HAlign_Center)
      .Padding(0.0f, 12.0f, 0.0f,
               0.0f)[SNew(STextBlock)
                         .Text_Lambda([this]() {
                           return FText::FromString(LoadingStatusText);
                         })
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                         .ColorAndOpacity(FLinearColor(0.88f, 0.93f, 0.99f))];

  LoadingCurtainContainer =
      SNew(SOverlay).Visibility(EVisibility::Visible) +
      SOverlay::Slot()[SNew(SBorder)
                           .BorderImage(WhiteBrush)
                           .BorderBackgroundColor(
                               FLinearColor(0.02f, 0.03f, 0.05f, 1.0f))
                           .HAlign(HAlign_Center)
                           .VAlign(VAlign_Center)[CenterBox]];

  GEngine->GameViewport->AddViewportWidgetContent(
      LoadingCurtainContainer.ToSharedRef(), 95);
  bLoadingCurtainActive = true;
}

void ULifePlayGTAControllerSubsystem::DismissLoadingCurtain(
    APlayerController *PC) {
  if (GEngine && GEngine->GameViewport && LoadingCurtainContainer.IsValid()) {
    GEngine->GameViewport->RemoveViewportWidgetContent(
        LoadingCurtainContainer.ToSharedRef());
  }
  LoadingCurtainContainer.Reset();
  bLoadingCurtainActive = false;
  bLoadingCurtainDismissed = true;

  UE_LOG(LogTemp, Display,
         TEXT("[LifePlayHUD] Loading curtain dismissed after %.2fs "
              "(TitleCharClothed=%d, MainMenu=%d)"),
         LoadingElapsed, bTitleCharClothed ? 1 : 0, bOnMainMenu ? 1 : 0);

  if (PC && bOnMainMenu) {
    ApplyInputLockForGta(PC, false);
  }
}

void ULifePlayGTAControllerSubsystem::UpdateLoadingCurtain(
    UWorld *World, APlayerController *PC, float DeltaTime) {
  if (bLoadingCurtainDismissed) {
    return;
  }

  EnsureLoadingCurtain();
  LoadingElapsed += DeltaTime;
  DressRetryTimer = FMath::Max(0.0f, DressRetryTimer - DeltaTime);
  LoadingPrepTimer += DeltaTime;
  if (LoadingPrepTimer >= 0.2f) {
    LoadingPrepTimer = 0.0f;
    CleanAndRebrandGameWidgets(World);
    EnsureTitleCharacterClothed(World);
  }

  const bool bAsyncDone = !IsAsyncLoading();
  const bool bAssetsReady = (bAsyncDone && LoadingElapsed >= 1.2f &&
                             (bTitleCharClothed || LoadingElapsed >= 5.5f)) ||
                            (LoadingElapsed >= 9.0f);

  if (!bAssetsReady) {
    LoadingProgress = FMath::Min(0.68f, (LoadingElapsed / 6.0f) * 0.68f);
    LoadingStatusText =
        !bAsyncDone ? TEXT("Loading game assets & textures...")
                    : (bTitleCharClothed
                           ? TEXT("Preparing scene...")
                           : TEXT("Loading outfits & dressing character..."));
  } else {
    BufferTimeRemaining -= DeltaTime;
    const float BufFrac =
        1.0f - FMath::Clamp(BufferTimeRemaining / 2.0f, 0.0f, 1.0f);
    LoadingProgress = 0.68f + 0.32f * BufFrac;
    LoadingStatusText = TEXT("Buffering interface...");
    if (BufferTimeRemaining <= 0.0f || LoadingElapsed >= 12.0f) {
      DismissLoadingCurtain(PC);
    }
  }
}

void ULifePlayGTAControllerSubsystem::RefreshStreetMapCache(UWorld *World) {
  if (!World) {
    return;
  }

  const FString CurrentMap = World->GetMapName();
  if (CachedStreetMapData.IsValid() && CachedStreetMapWorldName == CurrentMap &&
      CachedStreetMapData->Polylines.Num() > 0) {
    return;
  }

  TSharedPtr<FGtaMapData> NewData = MakeShared<FGtaMapData>();
  NewData->MapLabel = CurrentMap;
  bool bInitializedBounds = false;

  auto AddPoly = [&](const TArray<FVector2D> &LocalPts, const FTransform &Xform,
                     bool bClosed, const FLinearColor &Color, float Thickness) {
    if (LocalPts.Num() < 2) {
      return;
    }
    FGtaMapPolyline Poly;
    Poly.Color = Color;
    Poly.Thickness = Thickness;
    Poly.Points.Reserve(LocalPts.Num() + (bClosed ? 1 : 0));

    bool bFirst = true;
    for (const FVector2D &Pt : LocalPts) {
      const FVector W = Xform.TransformPosition(FVector(Pt.X, Pt.Y, 0.0f));
      const FVector2D W2(W.X, W.Y);
      Poly.Points.Add(W2);
      if (bFirst) {
        Poly.BoundsMin = W2;
        Poly.BoundsMax = W2;
        bFirst = false;
      } else {
        Poly.BoundsMin.X = FMath::Min(Poly.BoundsMin.X, W2.X);
        Poly.BoundsMin.Y = FMath::Min(Poly.BoundsMin.Y, W2.Y);
        Poly.BoundsMax.X = FMath::Max(Poly.BoundsMax.X, W2.X);
        Poly.BoundsMax.Y = FMath::Max(Poly.BoundsMax.Y, W2.Y);
      }
    }

    if (bClosed && Poly.Points.Num() >= 3) {
      Poly.Points.Add(Poly.Points[0]);
    }

    if (!bInitializedBounds) {
      NewData->WorldMin = Poly.BoundsMin;
      NewData->WorldMax = Poly.BoundsMax;
      bInitializedBounds = true;
    } else {
      NewData->WorldMin.X = FMath::Min(NewData->WorldMin.X, Poly.BoundsMin.X);
      NewData->WorldMin.Y = FMath::Min(NewData->WorldMin.Y, Poly.BoundsMin.Y);
      NewData->WorldMax.X = FMath::Max(NewData->WorldMax.X, Poly.BoundsMax.X);
      NewData->WorldMax.Y = FMath::Max(NewData->WorldMax.Y, Poly.BoundsMax.Y);
    }
    NewData->Polylines.Add(MoveTemp(Poly));
  };

  for (TObjectIterator<UStreetMapComponent> It; It; ++It) {
    UStreetMapComponent *Comp = *It;
    if (!Comp || !IsValid(Comp) || Comp->GetWorld() != World) {
      continue;
    }
    const UStreetMap *MapAsset = Comp->GetStreetMap();
    if (!MapAsset) {
      continue;
    }

    const FTransform Xform = Comp->GetComponentTransform();

    for (const FStreetMapPark &Park : MapAsset->GetParks()) {
      AddPoly(Park.ParkPoints, Xform, true,
              FLinearColor(0.08f, 0.28f, 0.14f, 0.65f), 1.4f);
    }
    for (const FStreetMapWater &Water : MapAsset->GetWaters()) {
      AddPoly(Water.WaterPoints, Xform, true,
              FLinearColor(0.08f, 0.26f, 0.52f, 0.78f), 1.6f);
    }
    for (const FStreetMapBuilding &Bldg : MapAsset->GetBuildings()) {
      AddPoly(Bldg.BuildingPoints, Xform, true,
              FLinearColor(0.14f, 0.20f, 0.28f, 0.65f), 1.0f);
    }
    for (const FStreetMapRoad &Road : MapAsset->GetRoads()) {
      FLinearColor Color(0.32f, 0.38f, 0.46f, 0.85f);
      float Thickness = 1.6f;
      switch (Road.RoadType) {
      case EStreetMapRoadType::Highway:
        Color = FLinearColor(0.86f, 0.66f, 0.28f, 0.92f);
        Thickness = 3.2f;
        break;
      case EStreetMapRoadType::MajorRoad:
        Color = FLinearColor(0.58f, 0.66f, 0.74f, 0.90f);
        Thickness = 2.4f;
        break;
      default:
        break;
      }
      AddPoly(Road.RoadPoints, Xform, false, Color, Thickness);
    }
  }

  CachedStreetMapData = NewData;
  CachedStreetMapWorldName = CurrentMap;
  if (MapViewState.IsValid()) {
    MapViewState->MapData = NewData;
    MapViewState->LocationLabel = CurrentMap;
  }
}

void ULifePlayGTAControllerSubsystem::UpdateMapViewState(
    UWorld *World, APlayerController *PC, ACharacter *PlayerChar,
    ULifePlayAIRoleplaySubsystem *AISub) {
  if (!MapViewState.IsValid() || !PlayerChar || !IsValid(PlayerChar)) {
    return;
  }

  const FVector PlayerLoc = PlayerChar->GetActorLocation();
  MapViewState->PlayerPos2D = FVector2D(PlayerLoc.X, PlayerLoc.Y);
  MapViewState->PlayerYawDeg = PlayerChar->GetActorRotation().Yaw;
  MapViewState->CameraYawDeg =
      bGtaModeEnabled ? CameraYaw
                      : ((PC && PC->PlayerCameraManager)
                             ? PC->PlayerCameraManager->GetCameraRotation().Yaw
                             : MapViewState->PlayerYawDeg);

  MapViewState->NpcBlips.Reset();
  if (World) {
    for (TActorIterator<ACharacter> It(World); It; ++It) {
      ACharacter *Npc = *It;
      if (!IsValid(Npc) || Npc == PlayerChar ||
          !ULifePlayAIRoleplaySubsystem::IsPersonActor(Npc) ||
          ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(Npc)) {
        continue;
      }
      if (!bFullMapOpen &&
          FVector::DistSquared2D(PlayerLoc, Npc->GetActorLocation()) >
              (4000.0f * 4000.0f)) {
        continue;
      }

      FGtaMapBlip Blip;
      const FVector NpcLoc = Npc->GetActorLocation();
      Blip.WorldPos = FVector2D(NpcLoc.X, NpcLoc.Y);
      Blip.Label = ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(Npc);
      Blip.Color = FLinearColor(0.92f, 0.94f, 0.98f, 0.95f);

      if (AISub) {
        const ELifePlayNpcAIState State = AISub->GetNpcAIState(Npc);
        const FLifePlayNpcContext Ctx = AISub->ExtractNpcContext(Npc);
        if (State == ELifePlayNpcAIState::Following ||
            State == ELifePlayNpcAIState::Approaching) {
          Blip.Color = FLinearColor(0.18f, 0.85f, 1.0f, 1.0f);
        } else if (State == ELifePlayNpcAIState::Fleeing ||
                   Ctx.Rapport < -15.0f) {
          Blip.Color = FLinearColor(0.98f, 0.28f, 0.28f, 1.0f);
        } else if (Ctx.Rapport >= 15.0f) {
          Blip.Color = FLinearColor(1.0f, 0.78f, 0.25f, 1.0f);
        }
      }
      MapViewState->NpcBlips.Add(MoveTemp(Blip));
    }
  }
}

void ULifePlayGTAControllerSubsystem::SetGpsWaypoint(bool bHasWaypoint,
                                                     FVector2D WorldPos2D) {
  if (!MapViewState.IsValid()) {
    return;
  }
  MapViewState->bHasWaypoint = bHasWaypoint;
  MapViewState->WaypointWorld2D = WorldPos2D;

  if (UWorld *World = GetWorld()) {
    if (ULifePlayAIRoleplaySubsystem *AISub =
            World->GetGameInstance()
                ? World->GetGameInstance()
                      ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
                : nullptr) {
      AISub->PlayRoleplaySound(
          World, FName(bHasWaypoint ? TEXT("ui_positive") : TEXT("ui_click")),
          nullptr, 0.75f, 1.0f);
    }
  }
  if (bHasWaypoint && CachedPlayerChar.IsValid()) {
    const float Meters =
        FVector2D::Distance(MapViewState->PlayerPos2D, WorldPos2D) / 100.0f;
    ShowHudToast(FString::Printf(TEXT("GPS Waypoint set (%.0fm away) — follow "
                                      "the magenta radar line!"),
                                 Meters),
                 3.5f);
  }
}

void ULifePlayGTAControllerSubsystem::TeleportPlayerToWaypoint() {
  UWorld *World = GetWorld();
  ACharacter *PlayerChar = CachedPlayerChar.Get();
  if (!World || !PlayerChar || !IsValid(PlayerChar) ||
      !MapViewState.IsValid() || !MapViewState->bHasWaypoint) {
    ShowHudToast(TEXT("Right-click anywhere on the World Map first to place a "
                      "GPS Waypoint!"),
                 3.5f);
    return;
  }

  const FVector2D Target2D = MapViewState->WaypointWorld2D;
  float TargetZ = PlayerChar->GetActorLocation().Z + 50.0f;

  FHitResult Hit;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(LifePlayGpsTeleport), false);
  Params.AddIgnoredActor(PlayerChar);
  if (World->LineTraceSingleByChannel(
          Hit, FVector(Target2D.X, Target2D.Y, 15000.0f),
          FVector(Target2D.X, Target2D.Y, -5000.0f), ECC_WorldStatic, Params)) {
    TargetZ = Hit.ImpactPoint.Z + 96.0f;
  }

  PlayerChar->SetActorLocation(FVector(Target2D.X, Target2D.Y, TargetZ), false,
                               nullptr, ETeleportType::TeleportPhysics);
  if (AGtaCameraRigActor *Rig = CameraRig.Get()) {
    Rig->SetActorLocation(PlayerChar->GetActorLocation());
  }

  if (ULifePlayAIRoleplaySubsystem *AISub =
          World->GetGameInstance()
              ? World->GetGameInstance()
                    ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
              : nullptr) {
    AISub->PlayRoleplaySound(World, FName(TEXT("ui_quest")), nullptr, 0.85f,
                             1.0f);
  }
  SetGpsWaypoint(false, FVector2D::ZeroVector);
  CloseWorldMap();
  ShowHudToast(TEXT("Fast traveled to GPS Waypoint!"), 3.0f);
}

void ULifePlayGTAControllerSubsystem::ToggleWorldMap() {
  if (bFullMapOpen) {
    CloseWorldMap();
    return;
  }

  UWorld *World = GetWorld();
  if (!World || !GEngine || !GEngine->GameViewport || !MapViewState.IsValid()) {
    return;
  }

  APlayerController *PC = World->GetFirstPlayerController();
  ACharacter *PlayerChar = ResolvePlayerCharacter(World, PC);
  if (!PlayerChar) {
    ShowHudToast(
        TEXT("Start or load a game first before opening the GTA World Map."),
        3.5f);
    return;
  }

  RefreshStreetMapCache(World);
  UpdateMapViewState(World, PC, PlayerChar,
                     World->GetGameInstance()
                         ? World->GetGameInstance()
                               ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
                         : nullptr);

  if (bChatOpen) {
    CloseRoleplayChat();
  }

  FullMapWidget =
      SNew(SLifePlayGtaMapWidget)
          .ViewState(MapViewState)
          .bFullScreen(true)
          .OnWaypointChanged(FOnGtaMapWaypointChanged::CreateUObject(
              this, &ULifePlayGTAControllerSubsystem::SetGpsWaypoint))
          .OnRequestTeleport(FSimpleDelegate::CreateUObject(
              this, &ULifePlayGTAControllerSubsystem::TeleportPlayerToWaypoint))
          .OnRequestClose(FSimpleDelegate::CreateUObject(
              this, &ULifePlayGTAControllerSubsystem::CloseWorldMap));

  FullMapContainer =
      SNew(SOverlay) + SOverlay::Slot()[FullMapWidget.ToSharedRef()];

  GEngine->GameViewport->AddViewportWidgetContent(
      FullMapContainer.ToSharedRef(), 45);
  bFullMapOpen = true;

  if (ULifePlayAIRoleplaySubsystem *AISub =
          World->GetGameInstance()
              ? World->GetGameInstance()
                    ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
              : nullptr) {
    AISub->PlayRoleplaySound(World, FName(TEXT("ui_click")), nullptr, 0.75f,
                             1.05f);
  }
  if (PC) {
    ApplyInputLockForGta(PC, false);
  }
  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().SetKeyboardFocus(FullMapWidget,
                                              EFocusCause::SetDirectly);
  }
}

void ULifePlayGTAControllerSubsystem::CloseWorldMap() {
  if (!bFullMapOpen) {
    return;
  }
  if (GEngine && GEngine->GameViewport && FullMapContainer.IsValid()) {
    GEngine->GameViewport->RemoveViewportWidgetContent(
        FullMapContainer.ToSharedRef());
  }
  FullMapContainer.Reset();
  FullMapWidget.Reset();
  bFullMapOpen = false;

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      ApplyInputLockForGta(PC, bGtaModeEnabled && !bAltCursorUnlock);
    }
  }
}

void ULifePlayGTAControllerSubsystem::StartOrStopStreetAmbience(
    UWorld *World, bool bShouldPlay) {
  if (!bShouldPlay) {
    if (UAudioComponent *Comp = AmbientAudioComp.Get()) {
      if (IsValid(Comp) && Comp->IsPlaying()) {
        Comp->FadeOut(0.4f, 0.0f);
      }
    }
    return;
  }

  if (!World)
    return;

  ULifePlayAIRoleplaySubsystem *AISub =
      World->GetGameInstance()
          ? World->GetGameInstance()
                ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
          : nullptr;
  if (!AISub || AISub->IsAudioMuted()) {
    StartOrStopStreetAmbience(World, false);
    return;
  }

  if (UAudioComponent *Existing = AmbientAudioComp.Get()) {
    if (IsValid(Existing)) {
      if (!Existing->IsPlaying())
        Existing->FadeIn(0.4f, 0.22f);
      return;
    }
  }

  if (USoundBase *StreetSound = AISub->GetOrLoadSound(FName(TEXT("street")))) {
    UAudioComponent *Spawned = UGameplayStatics::SpawnSound2D(
        World, StreetSound, 0.22f, 1.0f, 0.0f, nullptr, false, false);
    if (Spawned)
      AmbientAudioComp = Spawned;
  }
}

void ULifePlayGTAControllerSubsystem::SyncAmbientAudioState() {
  StartOrStopStreetAmbience(GetWorld(), bGtaModeEnabled);
  UpdateHudText();
}

bool ULifePlayGTAControllerSubsystem::IsTextInputFocused() const {
  if (!FSlateApplication::IsInitialized())
    return false;
  TSharedPtr<SWidget> Focused =
      FSlateApplication::Get().GetKeyboardFocusedWidget();
  if (!Focused.IsValid())
    return false;
  const FName WidgetType = Focused->GetType();
  return WidgetType == FName(TEXT("SEditableText")) ||
         WidgetType == FName(TEXT("SEditableTextBox")) ||
         WidgetType == FName(TEXT("SMultiLineEditableTextBox"));
}

void ULifePlayGTAControllerSubsystem::EnsureHudOverlay() {
  if (!GEngine || !GEngine->GameViewport) {
    return;
  }

  if (bHudShowMinimap && !MinimapContainer.IsValid() && MapViewState.IsValid()) {
    EHorizontalAlignment HAlign = HAlign_Right;
    EVerticalAlignment VAlign = VAlign_Bottom;
    if (HudMinimapAnchor.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase)) {
      HAlign = HAlign_Left;
      VAlign = VAlign_Bottom;
    } else if (HudMinimapAnchor.Equals(TEXT("TopRight"),
                                       ESearchCase::IgnoreCase)) {
      HAlign = HAlign_Right;
      VAlign = VAlign_Top;
    } else if (HudMinimapAnchor.Equals(TEXT("TopLeft"),
                                       ESearchCase::IgnoreCase)) {
      HAlign = HAlign_Left;
      VAlign = VAlign_Top;
    }

    const FMargin MinimapMargin(
        (HAlign == HAlign_Left) ? HudMinimapOffsetX : 0.0f,
        (VAlign == VAlign_Top) ? HudMinimapOffsetY : 0.0f,
        (HAlign == HAlign_Right) ? HudMinimapOffsetX : 0.0f,
        (VAlign == VAlign_Bottom) ? HudMinimapOffsetY : 0.0f);

    MinimapContainer =
        SNew(SOverlay).Visibility(EVisibility::HitTestInvisible) +
        SOverlay::Slot()[SNew(SLifePlayGtaMapWidget)
                             .ViewState(MapViewState)
                             .bFullScreen(false)
                             .MinimapHAlign(HAlign)
                             .MinimapVAlign(VAlign)
                             .MinimapSize(FVector2D(HudMinimapWidth,
                                                    HudMinimapHeight))
                             .MinimapPadding(MinimapMargin)];
    GEngine->GameViewport->AddViewportWidgetContent(
        MinimapContainer.ToSharedRef(), 22);
  }

  if (HudContainer.IsValid() ||
      (!bHudShowTopStatusCard && !bHudShowBottomControlsBar)) {
    return;
  }

  const FSlateBrush *WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
  TSharedRef<SOverlay> Overlay =
      SNew(SOverlay).Visibility(EVisibility::HitTestInvisible);

  if (bHudShowTopStatusCard) {
    LoadBrandLogoBrush();
    TSharedRef<SHorizontalBox> BrandHeaderRow = SNew(SHorizontalBox);
    if (BrandLogoBrush.IsValid()) {
      BrandHeaderRow->AddSlot()
          .AutoWidth()
          .VAlign(VAlign_Center)
          .Padding(0.0f, 0.0f, 10.0f,
                   0.0f)[SNew(SBox).WidthOverride(96.0f).HeightOverride(
              54.0f)[SNew(SImage).Image(BrandLogoBrush.Get())]];
    }

    BrandHeaderRow->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight()
             [SNew(STextBlock)
                  .Text(FText::FromString(TEXT("LIFEPLAY  v1.0")))
                  .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                  .ColorAndOpacity(FLinearColor(0.22f, 0.88f, 0.98f))] +
         SVerticalBox::Slot().AutoHeight().Padding(
             0.0f, 2.0f, 0.0f,
             0.0f)[SAssignNew(TopStatusModeText, STextBlock)
                       .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                       .ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f))] +
         SVerticalBox::Slot().AutoHeight().Padding(
             0.0f, 1.0f, 0.0f,
             0.0f)[SAssignNew(TopStatusNpcText, STextBlock)
                       .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                       .ColorAndOpacity(FLinearColor(0.98f, 0.80f, 0.35f))] +
         SVerticalBox::Slot().AutoHeight().Padding(
             0.0f, 1.0f, 0.0f,
             0.0f)[SAssignNew(TopStatusAudioText, STextBlock)
                       .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                       .ColorAndOpacity(FLinearColor(0.60f, 0.72f, 0.85f))]];

    Overlay->AddSlot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(20.0f, 18.0f, 0.0f, 0.0f))
            [SNew(SBorder)
                 .BorderImage(WhiteBrush)
                 .BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.84f))
                 .Padding(0.0f)
                     [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot()
                          .AutoWidth()[SNew(SBox).WidthOverride(
                              4.0f)[SNew(SBorder)
                                        .BorderImage(WhiteBrush)
                                        .BorderBackgroundColor(FLinearColor(
                                            0.18f, 0.82f, 0.98f, 1.0f))]] +
                      SHorizontalBox::Slot().AutoWidth().Padding(
                          FMargin(10.0f, 8.0f, 14.0f, 8.0f))[BrandHeaderRow]]];
  }

  if (bHudShowBottomControlsBar) {
    Overlay->AddSlot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 14.0f))
            [SAssignNew(BottomHudBorder, SBorder)
                 .BorderImage(WhiteBrush)
                 .BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.78f))
                 .Padding(FMargin(12.0f, 5.0f))
                     [SAssignNew(HudTextBlock, STextBlock)
                          .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                          .ColorAndOpacity(FLinearColor(0.94f, 0.96f, 1.0f))]];
  }

  HudContainer = Overlay;
  GEngine->GameViewport->AddViewportWidgetContent(HudContainer.ToSharedRef(),
                                                  20);
  UpdateHudText();
}

void ULifePlayGTAControllerSubsystem::ShowHudToast(const FString &Message,
                                                   float DurationSeconds) {
  ActiveStreetToast = Message;
  ToastTimer = DurationSeconds;
  UpdateHudText();
}

void ULifePlayGTAControllerSubsystem::UpdateHudText() {
  UWorld *World = GetWorld();
  ULifePlayAIRoleplaySubsystem *AISub =
      (World && World->GetGameInstance())
          ? World->GetGameInstance()
                ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
          : nullptr;
  const bool bMuted = AISub && AISub->IsAudioMuted();

  if (TopStatusModeText.IsValid()) {
    TopStatusModeText->SetText(FText::FromString(
        bGtaModeEnabled ? FString::Printf(TEXT("MODE: GTA THIRD-PERSON  •  %s"),
                                          *CurrentPaceLabel)
                        : TEXT("MODE: FREE CAMERA  (Press [G] for GTA Play)")));
  }

  if (TopStatusNpcText.IsValid()) {
    if (ACharacter *Npc = CachedNearestNpc.Get()) {
      ACharacter *PlayerChar = CachedPlayerChar.Get();
      const float Meters = PlayerChar
                               ? FVector::Dist(PlayerChar->GetActorLocation(),
                                               Npc->GetActorLocation()) /
                                     100.0f
                               : 0.0f;
      const FString AIState =
          AISub ? AISub->GetNpcAIStateName(Npc) : TEXT("Idle");
      TopStatusNpcText->SetText(FText::FromString(
          FString::Printf(TEXT("NEARBY: %s (%.1fm)  •  AI: %s"),
                          *ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(Npc),
                          Meters, *AIState)));
    } else {
      TopStatusNpcText->SetText(
          FText::FromString(TEXT("NEARBY: Walk closer to an NPC to roleplay")));
    }
  }

  if (TopStatusAudioText.IsValid()) {
    TopStatusAudioText->SetText(FText::FromString(
        bMuted ? TEXT("AUDIO: MUTED [M]")
               : TEXT("AUDIO: 3D SOUND & STREET AMBIENCE [M]")));
  }

  if (!HudTextBlock.IsValid())
    return;

  if (ToastTimer > 0.0f && !ActiveStreetToast.IsEmpty()) {
    if (BottomHudBorder.IsValid()) {
      BottomHudBorder->SetBorderBackgroundColor(
          FLinearColor(0.22f, 0.14f, 0.03f, 0.92f));
    }
    HudTextBlock->SetText(FText::FromString(ActiveStreetToast));
    return;
  }

  if (BottomHudBorder.IsValid()) {
    BottomHudBorder->SetBorderBackgroundColor(
        FLinearColor(0.03f, 0.04f, 0.07f, 0.84f));
  }

  if (bGtaModeEnabled) {
    HudTextBlock->SetText(FText::FromString(TEXT(
        "[WASD] Move  •  [Shift] Sprint  •  [Alt] Cursor  •  [E/T] Roleplay  • "
        " [N] GTA Map  •  [F] Call NPC  •  [M] Mute  •  [G] Exit")));
  } else {
    HudTextBlock->SetText(FText::FromString(
        TEXT("[G] GTA Play  •  [E/T] NPC Roleplay  •  [N] GTA World Map  •  "
             "[F] Call NPC  •  [M] Mute Audio")));
  }
}

ACharacter *
ULifePlayGTAControllerSubsystem::ResolvePlayerCharacter(UWorld *World,
                                                        APlayerController *PC) {
  if (ACharacter *Existing = CachedPlayerChar.Get()) {
    if (IsValid(Existing))
      return Existing;
  }

  auto ReadPlayerProp = [](UObject *Container) -> ACharacter * {
    if (!Container)
      return nullptr;
    for (TFieldIterator<FObjectProperty> It(Container->GetClass()); It; ++It) {
      if (It->GetName().Equals(TEXT("PlayerCharacter"),
                               ESearchCase::IgnoreCase)) {
        if (UObject *Val = It->GetObjectPropertyValue_InContainer(Container)) {
          return Cast<ACharacter>(Val);
        }
      }
    }
    return nullptr;
  };

  if (PC) {
    if (ACharacter *FromPC = ReadPlayerProp(PC)) {
      CachedPlayerChar = FromPC;
      return FromPC;
    }
  }
  if (World) {
    if (UGameInstance *GI = World->GetGameInstance()) {
      if (ACharacter *FromGI = ReadPlayerProp(GI)) {
        CachedPlayerChar = FromGI;
        return FromGI;
      }
    }
    for (TActorIterator<ACharacter> It(World); It; ++It) {
      ACharacter *Char = *It;
      if (ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(Char)) {
        CachedPlayerChar = Char;
        return Char;
      }
    }

    const FVector CamLoc = (PC && PC->PlayerCameraManager)
                               ? PC->PlayerCameraManager->GetCameraLocation()
                               : FVector::ZeroVector;
    ACharacter *NearestPerson = nullptr;
    float BestDistSq = MAX_flt;
    for (TActorIterator<ACharacter> It(World); It; ++It) {
      ACharacter *Char = *It;
      if (ULifePlayAIRoleplaySubsystem::IsPersonActor(Char)) {
        const float DistSq =
            FVector::DistSquared(CamLoc, Char->GetActorLocation());
        if (DistSq < BestDistSq) {
          BestDistSq = DistSq;
          NearestPerson = Char;
        }
      }
    }
    if (NearestPerson) {
      CachedPlayerChar = NearestPerson;
      return NearestPerson;
    }
  }
  return nullptr;
}

ACharacter *ULifePlayGTAControllerSubsystem::FindNearestNpc(
    UWorld *World, const FVector &Origin, float MaxDistance) const {
  if (!World)
    return nullptr;

  ACharacter *BestNpc = nullptr;
  float BestDistSq = MaxDistance * MaxDistance;
  ACharacter *PlayerChar = CachedPlayerChar.Get();

  for (TActorIterator<ACharacter> It(World); It; ++It) {
    ACharacter *Candidate = *It;
    if (!IsValid(Candidate) || Candidate == PlayerChar)
      continue;
    if (!ULifePlayAIRoleplaySubsystem::IsPersonActor(Candidate) ||
        ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(Candidate))
      continue;
    const float DistSq =
        FVector::DistSquared(Origin, Candidate->GetActorLocation());
    if (DistSq < BestDistSq) {
      BestDistSq = DistSq;
      BestNpc = Candidate;
    }
  }
  return BestNpc;
}

void ULifePlayGTAControllerSubsystem::NudgeNearbyIdleNpcsToRoam(
    UWorld *World, const FVector &Origin, ULifePlayAIRoleplaySubsystem *AISub) {
  if (!World || !AISub)
    return;

  ACharacter *PlayerChar = CachedPlayerChar.Get();
  int32 ActiveRoamers = 0;
  for (TActorIterator<ACharacter> It(World); It; ++It) {
    ACharacter *Candidate = *It;
    if (!IsValid(Candidate) || Candidate == PlayerChar ||
        !ULifePlayAIRoleplaySubsystem::IsPersonActor(Candidate))
      continue;

    if (AISub->GetNpcAIState(Candidate) == ELifePlayNpcAIState::Roaming) {
      ++ActiveRoamers;
      continue;
    }
    if (ActiveRoamers < 3 &&
        AISub->GetNpcAIState(Candidate) == ELifePlayNpcAIState::Idle &&
        FVector::DistSquared(Origin, Candidate->GetActorLocation()) <=
            (1800.0f * 1800.0f)) {
      AISub->SetNpcAIState(Candidate, ELifePlayNpcAIState::Roaming);
      ++ActiveRoamers;
    }
  }
}

AGtaCameraRigActor *
ULifePlayGTAControllerSubsystem::EnsureCameraRig(UWorld *World) {
  if (AGtaCameraRigActor *Existing = CameraRig.Get()) {
    if (IsValid(Existing))
      return Existing;
  }
  if (!World)
    return nullptr;

  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AGtaCameraRigActor *Spawned = World->SpawnActor<AGtaCameraRigActor>(
      AGtaCameraRigActor::StaticClass(), FTransform::Identity, Params);
  CameraRig = Spawned;
  return Spawned;
}

void ULifePlayGTAControllerSubsystem::ApplyInputLockForGta(
    APlayerController *PC, bool bLockMouse) {
  if (!PC)
    return;

  if (bLockMouse) {
    FInputModeGameOnly GameOnly;
    GameOnly.SetConsumeCaptureMouseDown(true);
    PC->SetInputMode(GameOnly);
    PC->bShowMouseCursor = false;
  } else {
    FInputModeGameAndUI GameAndUI;
    GameAndUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    GameAndUI.SetHideCursorDuringCapture(false);
    PC->SetInputMode(GameAndUI);
    PC->bShowMouseCursor = true;
  }
}

void ULifePlayGTAControllerSubsystem::ToggleGtaMode() {
  SetGtaModeEnabled(!bGtaModeEnabled);
}

void ULifePlayGTAControllerSubsystem::SetGtaModeEnabled(bool bEnable) {
  UWorld *World = GetWorld();
  if (!World)
    return;
  APlayerController *PC = World->GetFirstPlayerController();
  if (!PC)
    return;

  ACharacter *PlayerChar = ResolvePlayerCharacter(World, PC);
  if (bEnable && !PlayerChar) {
    ShowHudToast(TEXT("Load or start a game first (New Game / Load Last Save), "
                      "then press [G] for GTA Play."),
                 4.0f);
    return;
  }

  if (bEnable == bGtaModeEnabled)
    return;

  bGtaModeEnabled = bEnable;
  bAltCursorUnlock = false;
  CurrentPaceLabel = TEXT("IDLE");

  if (ULifePlayAIRoleplaySubsystem *AISub =
          World->GetGameInstance()
              ? World->GetGameInstance()
                    ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
              : nullptr) {
    AISub->PlayRoleplaySound(World, FName(TEXT("ui_click")), nullptr, 0.75f,
                             bGtaModeEnabled ? 1.05f : 0.92f);
  }

  if (bGtaModeEnabled) {
    PreviousViewTarget = PC->GetViewTarget();
    PreviousControlledPawn = PC->GetPawn();
    if (APawn *PrevPawn = PreviousControlledPawn.Get()) {
      if (PrevPawn != PlayerChar)
        PrevPawn->DisableInput(PC);
    }

    CameraYaw = PlayerChar->GetActorRotation().Yaw;
    CameraPitch = -12.0f;

    if (UCharacterMovementComponent *MoveComp =
            PlayerChar->GetCharacterMovement()) {
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

    if (AGtaCameraRigActor *Rig = EnsureCameraRig(World)) {
      Rig->SetActorLocationAndRotation(PlayerChar->GetActorLocation(),
                                       FRotator(CameraPitch, CameraYaw, 0.0f));
      Rig->SpringArm->TargetArmLength = TargetArmLength;
      PC->SetViewTargetWithBlend(Rig, 0.25f);
    }

    StartOrStopStreetAmbience(World, true);

    if (!bChatOpen && !bFullMapOpen)
      ApplyInputLockForGta(PC, true);
  } else {
    StartOrStopStreetAmbience(World, false);

    if (PlayerChar && IsValid(PlayerChar)) {
      if (UCharacterMovementComponent *MoveComp =
              PlayerChar->GetCharacterMovement()) {
        MoveComp->bOrientRotationToMovement = bSavedOrientToMovement;
        MoveComp->bRunPhysicsWithNoController = bSavedRunPhysicsNoController;
        MoveComp->bUseControllerDesiredRotation = bSavedUseControllerDesiredRot;
        MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed;
      }
      PlayerChar->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
    }

    if (APawn *PrevPawn = PreviousControlledPawn.Get()) {
      if (IsValid(PrevPawn)) {
        if (PlayerChar && IsValid(PlayerChar) && PrevPawn != PlayerChar) {
          const FVector CamBack =
              PlayerChar->GetActorLocation() -
              FRotator(0.0f, CameraYaw, 0.0f).Vector() * 240.0f +
              FVector(0.0f, 0.0f, 110.0f);
          PrevPawn->SetActorLocation(CamBack);
        }
        PrevPawn->EnableInput(PC);
      }
    }

    if (AActor *PrevTarget = PreviousViewTarget.Get()) {
      if (IsValid(PrevTarget))
        PC->SetViewTargetWithBlend(PrevTarget, 0.25f);
    }
    ApplyInputLockForGta(PC, false);
  }

  UpdateHudText();
}

void ULifePlayGTAControllerSubsystem::ToggleRoleplayChat() {
  if (bChatOpen) {
    CloseRoleplayChat();
    return;
  }

  if (bFullMapOpen) {
    CloseWorldMap();
  }

  UWorld *World = GetWorld();
  if (!World || !GEngine || !GEngine->GameViewport)
    return;

  APlayerController *PC = World->GetFirstPlayerController();
  ACharacter *PlayerChar = ResolvePlayerCharacter(World, PC);
  const FVector SearchOrigin =
      PlayerChar ? PlayerChar->GetActorLocation()
                 : (PC && PC->PlayerCameraManager
                        ? PC->PlayerCameraManager->GetCameraLocation()
                        : FVector::ZeroVector);

  ACharacter *TargetNpc = CachedNearestNpc.Get();
  if (!TargetNpc || !IsValid(TargetNpc)) {
    TargetNpc = FindNearestNpc(World, SearchOrigin, 2500.0f);
  }

  if (!TargetNpc) {
    ShowHudToast(TEXT("No NPCs nearby! Use 'Head to an outdoor location' or "
                      "'Contacts -> Invite over' first."),
                 4.5f);
    return;
  }

  ULifePlayAIRoleplaySubsystem *AISub =
      World->GetGameInstance()
          ? World->GetGameInstance()
                ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
          : nullptr;
  if (!AISub)
    return;

  AISub->PlayRoleplaySound(World, FName(TEXT("ui_click")), nullptr, 0.75f,
                           1.0f);
  AISub->ExecuteNpcAction(TargetNpc, PlayerChar, TEXT("face_player"));

  ChatWidget =
      SNew(SLifePlayRoleplayChatWidget)
          .Subsystem(AISub)
          .NpcActor(TargetNpc)
          .PlayerActor(PlayerChar)
          .OnRequestClose(FSimpleDelegate::CreateUObject(
              this, &ULifePlayGTAControllerSubsystem::CloseRoleplayChat))
          .OnAudioMuteToggled(FSimpleDelegate::CreateUObject(
              this, &ULifePlayGTAControllerSubsystem::SyncAmbientAudioState));

  ChatContainer = SNew(SOverlay) + SOverlay::Slot()[ChatWidget.ToSharedRef()];

  GEngine->GameViewport->AddViewportWidgetContent(ChatContainer.ToSharedRef(),
                                                  50);
  bChatOpen = true;

  if (PC)
    ApplyInputLockForGta(PC, false);
  ChatWidget->FocusInput();
}

void ULifePlayGTAControllerSubsystem::CloseRoleplayChat() {
  if (!bChatOpen)
    return;

  if (GEngine && GEngine->GameViewport && ChatContainer.IsValid()) {
    GEngine->GameViewport->RemoveViewportWidgetContent(
        ChatContainer.ToSharedRef());
  }
  ChatContainer.Reset();
  ChatWidget.Reset();
  bChatOpen = false;

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      ApplyInputLockForGta(PC, bGtaModeEnabled && !bAltCursorUnlock &&
                                   !bFullMapOpen);
    }
  }
}

void ULifePlayGTAControllerSubsystem::Tick(float DeltaTime) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  GAreScreenMessagesEnabled = false;
  bOnMainMenu = IsMainMenuVisible(World);

  APlayerController *PC = World->GetFirstPlayerController();

  if (!bLoadingCurtainDismissed) {
    UpdateLoadingCurtain(World, PC, DeltaTime);
  }

  MenuCleanTimer += DeltaTime;
  if (MenuCleanTimer >= 0.5f) {
    MenuCleanTimer = 0.0f;
    CleanAndRebrandGameWidgets(World);
    if (bOnMainMenu) {
      EnsureTitleCharacterClothed(World);
    }
  }

  if (bOnMainMenu) {
    if (HudContainer.IsValid())
      HudContainer->SetVisibility(EVisibility::Collapsed);
    if (MinimapContainer.IsValid())
      MinimapContainer->SetVisibility(EVisibility::Collapsed);
    CachedPlayerChar.Reset();
    if (bChatOpen)
      CloseRoleplayChat();
    if (bFullMapOpen)
      CloseWorldMap();
    if (bGtaModeEnabled)
      SetGtaModeEnabled(false);
    return;
  }

  const bool bOnSetupMenu = IsSetupMenuVisible(World);
  if (bOnSetupMenu) {
    if (HudContainer.IsValid())
      HudContainer->SetVisibility(EVisibility::Collapsed);
    if (MinimapContainer.IsValid())
      MinimapContainer->SetVisibility(EVisibility::Collapsed);
    return;
  }

  if (!bLoadingCurtainActive) {
    if (HudContainer.IsValid())
      HudContainer->SetVisibility(EVisibility::HitTestInvisible);
    if (MinimapContainer.IsValid())
      MinimapContainer->SetVisibility((bChatOpen || bFullMapOpen)
                                          ? EVisibility::Collapsed
                                          : EVisibility::HitTestInvisible);
  }

  if (!PC)
    return;

  ACharacter *PlayerChar = ResolvePlayerCharacter(World, PC);
  if (PlayerChar) {
    EnsureHudOverlay();
  }

  ULifePlayAIRoleplaySubsystem *AISub =
      World->GetGameInstance()
          ? World->GetGameInstance()
                ->GetSubsystem<ULifePlayAIRoleplaySubsystem>()
          : nullptr;

  if (AISub && PlayerChar) {
    AISub->TickInteractionAI(World, PlayerChar, DeltaTime);
  }

  if (ToastTimer > 0.0f) {
    ToastTimer -= DeltaTime;
    if (ToastTimer <= 0.0f) {
      ActiveStreetToast.Reset();
      UpdateHudText();
    }
  }

  NpcScanTimer += DeltaTime;
  if (NpcScanTimer >= 0.25f) {
    NpcScanTimer = 0.0f;
    RefreshStreetMapCache(World);

    const FVector Origin =
        PlayerChar ? PlayerChar->GetActorLocation()
                   : (PC->PlayerCameraManager
                          ? PC->PlayerCameraManager->GetCameraLocation()
                          : FVector::ZeroVector);
    ACharacter *NewNearest = FindNearestNpc(World, Origin, 450.0f);
    CachedNearestNpc = NewNearest;
    UpdateHudText();

    if (bGtaModeEnabled && !bChatOpen && AISub && PlayerChar && NewNearest &&
        IsValid(NewNearest)) {
      const float DistToNpc = FVector::Dist2D(PlayerChar->GetActorLocation(),
                                              NewNearest->GetActorLocation());
      if (DistToNpc <= 250.0f &&
          AISub->CanNpcSeePlayer(NewNearest, PlayerChar)) {
        const double Now = World->GetTimeSeconds();
        double *LastTime = ProximityReactionTimestamps.Find(NewNearest);
        if (!LastTime || (Now - *LastTime) >= 12.0) {
          ProximityReactionTimestamps.Add(NewNearest, Now);
          AISub->ExecuteNpcAction(NewNearest, PlayerChar, TEXT("face_player"));
          const FVector NpcLoc = NewNearest->GetActorLocation();
          AISub->PlayRoleplaySound(World, FName(TEXT("ui_info")), &NpcLoc,
                                   0.55f, FMath::FRandRange(0.95f, 1.08f));
          ShowHudToast(AISub->GenerateStreetReaction(NewNearest, PlayerChar),
                       4.0f);
        }
      }
    }
  }

  if (PlayerChar) {
    UpdateMapViewState(World, PC, PlayerChar, AISub);
  }

  if (bGtaModeEnabled && AISub && PlayerChar) {
    AmbientRoamTimer += DeltaTime;
    if (AmbientRoamTimer >= 4.0f) {
      AmbientRoamTimer = 0.0f;
      NudgeNearbyIdleNpcsToRoam(World, PlayerChar->GetActorLocation(), AISub);
    }
  }

  if (bChatOpen) {
    if (PC->WasInputKeyJustPressed(EKeys::Escape))
      CloseRoleplayChat();
    return;
  }

  if (bFullMapOpen) {
    if (PC->WasInputKeyJustPressed(EKeys::Escape) ||
        PC->WasInputKeyJustPressed(EKeys::N)) {
      CloseWorldMap();
    }
    return;
  }

  const bool bCtrlDown = PC->IsInputKeyDown(EKeys::LeftControl) ||
                         PC->IsInputKeyDown(EKeys::RightControl);
  const bool bAltDown =
      PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
  const bool bShiftDown = PC->IsInputKeyDown(EKeys::LeftShift) ||
                          PC->IsInputKeyDown(EKeys::RightShift);

  if (!IsTextInputFocused() && !bCtrlDown && !bAltDown && !bShiftDown) {
    if (PC->WasInputKeyJustPressed(EKeys::G)) {
      ToggleGtaMode();
    } else if (PC->WasInputKeyJustPressed(EKeys::E) ||
               PC->WasInputKeyJustPressed(EKeys::T)) {
      ToggleRoleplayChat();
      return;
    } else if (PC->WasInputKeyJustPressed(EKeys::N)) {
      ToggleWorldMap();
      return;
    } else if (PC->WasInputKeyJustPressed(EKeys::F) && AISub && PlayerChar) {
      ACharacter *FarNpc =
          FindNearestNpc(World, PlayerChar->GetActorLocation(), 2500.0f);
      if (FarNpc) {
        const FVector NpcLoc = FarNpc->GetActorLocation();
        AISub->ExecuteNpcAction(FarNpc, PlayerChar, TEXT("call_over"));
        AISub->PlayRoleplaySound(World, FName(TEXT("ui_info")), &NpcLoc, 0.80f,
                                 1.08f);
        ShowHudToast(
            FString::Printf(
                TEXT("Called %s over to you..."),
                *ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(FarNpc)),
            3.0f);
      } else {
        ShowHudToast(TEXT("No NPCs within range — try 'Head to an outdoor "
                          "location' first."),
                     3.5f);
      }
    } else if (PC->WasInputKeyJustPressed(EKeys::M) && AISub) {
      AISub->ToggleAudioMute();
      SyncAmbientAudioState();
    }
  }

  if (!bGtaModeEnabled || !PlayerChar || !IsValid(PlayerChar)) {
    return;
  }

  const bool bWantAltUnlock = PC->IsInputKeyDown(EKeys::LeftAlt);
  if (bWantAltUnlock != bAltCursorUnlock) {
    bAltCursorUnlock = bWantAltUnlock;
    ApplyInputLockForGta(PC, !bAltCursorUnlock);
  }

  if (!bAltCursorUnlock) {
    float MouseDX = 0.0f;
    float MouseDY = 0.0f;
    PC->GetInputMouseDelta(MouseDX, MouseDY);
    CameraYaw += MouseDX * 2.2f;
    CameraPitch = FMath::Clamp(CameraPitch - MouseDY * 2.0f, -40.0f, 55.0f);

    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) {
      TargetArmLength = FMath::Clamp(TargetArmLength - 20.0f, 140.0f, 450.0f);
    } else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) {
      TargetArmLength = FMath::Clamp(TargetArmLength + 20.0f, 140.0f, 450.0f);
    }
  }

  if (AGtaCameraRigActor *Rig = EnsureCameraRig(World)) {
    Rig->SetActorLocationAndRotation(PlayerChar->GetActorLocation(),
                                     FRotator(CameraPitch, CameraYaw, 0.0f));
    Rig->SpringArm->TargetArmLength = TargetArmLength;
  }

  if (IsTextInputFocused())
    return;

  float ForwardInput = 0.0f;
  float RightInput = 0.0f;
  if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
    ForwardInput += 1.0f;
  if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
    ForwardInput -= 1.0f;
  if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
    RightInput += 1.0f;
  if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
    RightInput -= 1.0f;

  FString NextPace = TEXT("IDLE");
  if (!FMath::IsNearlyZero(ForwardInput) || !FMath::IsNearlyZero(RightInput)) {
    if (AAIController *AI = Cast<AAIController>(PlayerChar->GetController())) {
      AI->StopMovement();
    }

    const bool bSprint = PC->IsInputKeyDown(EKeys::LeftShift);
    const bool bWalk = PC->IsInputKeyDown(EKeys::LeftControl);
    NextPace = bSprint ? TEXT("SPRINT") : (bWalk ? TEXT("WALK") : TEXT("JOG"));

    if (UCharacterMovementComponent *MoveComp =
            PlayerChar->GetCharacterMovement()) {
      MoveComp->bRunPhysicsWithNoController = true;
      MoveComp->bOrientRotationToMovement = true;
      if (MoveComp->MovementMode == MOVE_None)
        MoveComp->SetMovementMode(MOVE_Walking);
      MoveComp->MaxWalkSpeed = bSprint ? 520.0f : (bWalk ? 150.0f : 300.0f);
    }

    const FRotator YawRot(0.0f, CameraYaw, 0.0f);
    const FVector MoveDir =
        (FRotationMatrix(YawRot).GetUnitAxis(EAxis::X) * ForwardInput +
         FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y) * RightInput)
            .GetSafeNormal();

    PlayerChar->AddMovementInput(MoveDir, 1.0f, true);
    PlayerChar->SetActorRotation(FMath::RInterpTo(
        PlayerChar->GetActorRotation(),
        FRotator(0.0f, MoveDir.Rotation().Yaw, 0.0f), DeltaTime, 10.0f));
  }

  if (!CurrentPaceLabel.Equals(NextPace)) {
    CurrentPaceLabel = NextPace;
    UpdateHudText();
  }
}
