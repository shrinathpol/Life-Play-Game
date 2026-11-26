#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LifePlayGTAControllerSubsystem.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class SLifePlayGtaMapWidget;
class SLifePlayRoleplayChatWidget;
class SBorder;
class STextBlock;
class SWidget;
class UAudioComponent;
class UCameraComponent;
class USpringArmComponent;
class UTexture2D;
struct FGtaMapData;
struct FGtaMapViewState;
struct FSlateBrush;

/**
 * Lightweight over-the-shoulder GTA camera rig spawned at runtime.
 */
UCLASS(Transient)
class LIFEPLAY_API AGtaCameraRigActor : public AActor
{
	GENERATED_BODY()

public:
	AGtaCameraRigActor();

	UPROPERTY()
	TObjectPtr<USceneComponent> PivotRoot;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
};

/**
 * Enables GTA-style third-person character control (G), standalone Role-Play &
 * UE Interaction AI (E / T), GTA Radar Minimap & World GPS Map (N), NavMesh NPC call-over (F),
 * street ambience & audio mute (M), and clean v1.0 MainMenu branding.
 */
UCLASS()
class LIFEPLAY_API ULifePlayGTAControllerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "LifePlay|GTA")
	void ToggleGtaMode();

	UFUNCTION(BlueprintCallable, Category = "LifePlay|GTA")
	void SetGtaModeEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "LifePlay|GTA")
	bool IsGtaModeEnabled() const { return bGtaModeEnabled; }

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	void ToggleRoleplayChat();

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Map")
	void ToggleWorldMap();

	void CloseRoleplayChat();
	void CloseWorldMap();
	void SyncAmbientAudioState();
	void SetGpsWaypoint(bool bHasWaypoint, FVector2D WorldPos2D);
	void TeleportPlayerToWaypoint();

private:
	bool bGtaModeEnabled = false;
	bool bAltCursorUnlock = false;
	bool bChatOpen = false;
	bool bFullMapOpen = false;
	FString CurrentPaceLabel = TEXT("IDLE");

	float CameraYaw = 0.0f;
	float CameraPitch = -12.0f;
	float TargetArmLength = 250.0f;

	bool bSavedOrientToMovement = false;
	bool bSavedRunPhysicsNoController = false;
	bool bSavedUseControllerDesiredRot = false;
	bool bSavedUseControllerRotationYaw = false;
	float SavedMaxWalkSpeed = 300.0f;

	TWeakObjectPtr<ACharacter> CachedPlayerChar;
	TWeakObjectPtr<ACharacter> CachedNearestNpc;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	TWeakObjectPtr<APawn> PreviousControlledPawn;
	TWeakObjectPtr<AGtaCameraRigActor> CameraRig;
	TWeakObjectPtr<UAudioComponent> AmbientAudioComp;

	UPROPERTY()
	TObjectPtr<UTexture2D> BrandLogoTexture;

	TSharedPtr<FSlateBrush> BrandLogoBrush;
	TSharedPtr<SWidget> HudContainer;
	TSharedPtr<SBorder> BottomHudBorder;
	TSharedPtr<STextBlock> TopStatusModeText;
	TSharedPtr<STextBlock> TopStatusNpcText;
	TSharedPtr<STextBlock> TopStatusAudioText;
	TSharedPtr<STextBlock> HudTextBlock;
	TSharedPtr<SWidget> ChatContainer;
	TSharedPtr<SLifePlayRoleplayChatWidget> ChatWidget;

	TSharedPtr<FGtaMapData> CachedStreetMapData;
	TSharedPtr<FGtaMapViewState> MapViewState;
	FString CachedStreetMapWorldName;
	TSharedPtr<SWidget> MinimapContainer;
	TSharedPtr<SWidget> FullMapContainer;
	TSharedPtr<SLifePlayGtaMapWidget> FullMapWidget;
	TSharedPtr<SWidget> LoadingCurtainContainer;

	bool bOnMainMenu = false;
	bool bMainMenuRebranded = false;
	bool bLoadingCurtainActive = false;
	bool bLoadingCurtainDismissed = false;
	bool bTitleCharClothed = false;
	float LoadingElapsed = 0.0f;
	float BufferTimeRemaining = 2.0f;
	float LoadingProgress = 0.05f;
	float LoadingPrepTimer = 0.0f;
	float DressRetryTimer = 0.0f;
	int32 DressRetryCount = 0;
	FString LoadingStatusText = TEXT("Loading game assets & clothing...");

	float NpcScanTimer = 0.0f;
	float MenuCleanTimer = 0.0f;
	float AmbientRoamTimer = 0.0f;
	float ToastTimer = 0.0f;
	FString ActiveStreetToast;
	TMap<TWeakObjectPtr<AActor>, double> ProximityReactionTimestamps;

	void LoadBrandLogoBrush();
	void CleanAndRebrandGameWidgets(UWorld* World);
	bool IsMainMenuVisible(UWorld* World) const;
	void EnsureTitleCharacterClothed(UWorld* World);
	void EnsureLoadingCurtain();
	void UpdateLoadingCurtain(UWorld* World, APlayerController* PC, float DeltaTime);
	void DismissLoadingCurtain(APlayerController* PC);
	void RefreshStreetMapCache(UWorld* World);
	void UpdateMapViewState(UWorld* World, APlayerController* PC, ACharacter* PlayerChar, class ULifePlayAIRoleplaySubsystem* AISub);
	ACharacter* ResolvePlayerCharacter(UWorld* World, APlayerController* PC);
	ACharacter* FindNearestNpc(UWorld* World, const FVector& Origin, float MaxDistance) const;
	AGtaCameraRigActor* EnsureCameraRig(UWorld* World);
	void EnsureHudOverlay();
	void UpdateHudText();
	void ShowHudToast(const FString& Message, float DurationSeconds);
	void ApplyInputLockForGta(APlayerController* PC, bool bLockMouse);
	bool IsTextInputFocused() const;
	void StartOrStopStreetAmbience(UWorld* World, bool bShouldPlay);
	void NudgeNearbyIdleNpcsToRoam(UWorld* World, const FVector& Origin, class ULifePlayAIRoleplaySubsystem* AISub);
};
