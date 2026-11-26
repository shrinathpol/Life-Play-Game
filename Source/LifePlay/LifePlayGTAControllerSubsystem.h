#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LifePlayGTAControllerSubsystem.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class SLifePlayRoleplayChatWidget;
class SBorder;
class STextBlock;
class SWidget;
class UAudioComponent;
class UCameraComponent;
class USpringArmComponent;
class UTexture2D;
struct FSlateDynamicImageBrush;

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
 * Unreal Engine Interaction AI (E / T), NavMesh NPC call-over (F), street ambience (M),
 * sleek v1.0 GTA HUD branding, and automatic cleanup of legacy MainMenu social/donate widgets.
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

	void CloseRoleplayChat();
	void SyncAmbientAudioState();

private:
	bool bGtaModeEnabled = false;
	bool bAltCursorUnlock = false;
	bool bChatOpen = false;
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

	TSharedPtr<FSlateDynamicImageBrush> BrandLogoBrush;
	TSharedPtr<SWidget> HudContainer;
	TSharedPtr<SBorder> BottomHudBorder;
	TSharedPtr<STextBlock> TopStatusModeText;
	TSharedPtr<STextBlock> TopStatusNpcText;
	TSharedPtr<STextBlock> TopStatusAudioText;
	TSharedPtr<STextBlock> HudTextBlock;
	TSharedPtr<SWidget> ChatContainer;
	TSharedPtr<SLifePlayRoleplayChatWidget> ChatWidget;

	float NpcScanTimer = 0.0f;
	float MenuCleanTimer = 0.0f;
	float AmbientRoamTimer = 0.0f;
	float ToastTimer = 0.0f;
	FString ActiveStreetToast;
	TMap<TWeakObjectPtr<AActor>, double> ProximityReactionTimestamps;

	void LoadBrandLogoBrush();
	void CleanAndRebrandGameWidgets(UWorld* World);
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
