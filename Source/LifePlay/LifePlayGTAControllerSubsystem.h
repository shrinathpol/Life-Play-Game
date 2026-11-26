#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LifePlayGTAControllerSubsystem.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class SLifePlayRoleplayChatWidget;
class STextBlock;
class SWidget;
class UCameraComponent;
class USpringArmComponent;

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
 * Enables GTA-style third-person character control (toggle: G) and opens the
 * live AI Role-Play Chat overlay for the nearest NPC (toggle: T).
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

	UFUNCTION(BlueprintCallable, Category = "LifePlay|AI Roleplay")
	void ToggleRoleplayChat();

	void CloseRoleplayChat();

private:
	bool bGtaModeEnabled = false;
	bool bAltCursorUnlock = false;
	bool bChatOpen = false;

	float CameraYaw = 0.0f;
	float CameraPitch = -12.0f;
	float TargetArmLength = 250.0f;

	// Saved character movement flags for clean restoration when exiting GTA Mode
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

	TSharedPtr<SWidget> HudContainer;
	TSharedPtr<STextBlock> HudTextBlock;
	TSharedPtr<SWidget> ChatContainer;
	TSharedPtr<SLifePlayRoleplayChatWidget> ChatWidget;

	float NpcScanTimer = 0.0f;

	ACharacter* ResolvePlayerCharacter(UWorld* World, APlayerController* PC);
	ACharacter* FindNearestNpc(UWorld* World, const FVector& Origin, float MaxDistance) const;
	AGtaCameraRigActor* EnsureCameraRig(UWorld* World);
	void EnsureHudOverlay();
	void UpdateHudText();
	void ApplyInputLockForGta(APlayerController* PC, bool bLockMouse);
	bool IsTextInputFocused() const;
};
