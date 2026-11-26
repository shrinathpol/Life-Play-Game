#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LifePlayAIRoleplaySubsystem.generated.h"

class AActor;
class USoundBase;
class UWorld;

UENUM(BlueprintType)
enum class ELifePlayNpcAIState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Engaged UMETA(DisplayName = "Engaged"),
	Following UMETA(DisplayName = "Following"),
	Fleeing UMETA(DisplayName = "Fleeing"),
	Approaching UMETA(DisplayName = "Approaching"),
	Roaming UMETA(DisplayName = "Roaming")
};

USTRUCT(BlueprintType)
struct LIFEPLAY_API FLifePlayNpcContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString NpcId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString FullName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	int32 Age = 25;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Job = TEXT("Resident");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Mood = TEXT("Neutral");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	float Rapport = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	float Attraction = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString LocationName;
};

USTRUCT(BlueprintType)
struct LIFEPLAY_API FLifePlayChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Role;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Text;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Emotion = TEXT("neutral");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	FString Action = TEXT("none");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	float RelationshipDelta = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LifePlay|Roleplay")
	float AttractionDelta = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnLifePlayRoleplayReply,
	const FString&, NpcId,
	bool, bSuccess,
	const FLifePlayChatMessage&, Reply
);

/**
 * Native Unreal Engine 5.8 standalone NPC Interaction AI, Role-Play, and Audio Soundplay subsystem.
 * Uses UE AAIController (Focus / MoveToActor / LineOfSightTo), UNavigationSystemV1,
 * UCharacterMovementComponent pace-matching, and UGameplayStatics spatial/UI sound cues.
 */
UCLASS()
class LIFEPLAY_API ULifePlayAIRoleplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "LifePlay|Roleplay")
	FOnLifePlayRoleplayReply OnRoleplayReplyReceived;

	// --- Native UE Audio Soundplay ---
	UFUNCTION(BlueprintCallable, Category = "LifePlay|Audio")
	void ToggleAudioMute();

	UFUNCTION(BlueprintPure, Category = "LifePlay|Audio")
	bool IsAudioMuted() const { return bAudioMuted; }

	USoundBase* GetOrLoadSound(FName SoundKey);
	void PlayRoleplaySound(UWorld* World, FName SoundKey, const FVector* WorldLocation = nullptr, float Volume = 0.85f, float Pitch = 1.0f);

	// --- Native UE Interaction AI ---
	UFUNCTION(BlueprintPure, Category = "LifePlay|InteractionAI")
	ELifePlayNpcAIState GetNpcAIState(AActor* NpcActor) const;

	UFUNCTION(BlueprintPure, Category = "LifePlay|InteractionAI")
	FString GetNpcAIStateName(AActor* NpcActor) const;

	UFUNCTION(BlueprintCallable, Category = "LifePlay|InteractionAI")
	void SetNpcAIState(AActor* NpcActor, ELifePlayNpcAIState NewState);

	UFUNCTION(BlueprintPure, Category = "LifePlay|InteractionAI")
	bool CanNpcSeePlayer(AActor* NpcActor, AActor* PlayerActor) const;

	void TickInteractionAI(UWorld* World, AActor* PlayerActor, float DeltaTime);
	FString GenerateStreetReaction(AActor* NpcActor, AActor* PlayerActor) const;

	// --- LifePlay Context & Standalone Role-Play ---
	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	FLifePlayNpcContext ExtractNpcContext(AActor* NpcActor) const;

	UFUNCTION(BlueprintPure, Category = "LifePlay|Roleplay")
	static FString GetNpcDisplayName(AActor* NpcActor);

	UFUNCTION(BlueprintPure, Category = "LifePlay|Roleplay")
	static bool IsPersonActor(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "LifePlay|Roleplay")
	static bool IsPlayerPersonActor(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	TArray<FLifePlayChatMessage> GetHistoryForNpc(const FString& NpcId);

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	void ClearNpcMemory(const FString& NpcId);

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	void ExecuteNpcAction(AActor* NpcActor, AActor* PlayerActor, const FString& Action);

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	FLifePlayChatMessage SendRoleplayMessage(AActor* NpcActor, AActor* PlayerActor, const FString& PlayerMessage);

	UFUNCTION(BlueprintCallable, Category = "LifePlay|Roleplay")
	FLifePlayChatMessage TriggerPresetInteraction(AActor* NpcActor, AActor* PlayerActor, const FString& InteractionId);

private:
	static constexpr int32 MaxHistoryTurns = 30;
	bool bAudioMuted = false;
	float AITickAccumulator = 0.0f;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> CachedSounds;

	TMap<TWeakObjectPtr<AActor>, ELifePlayNpcAIState> NpcAIStates;
	TMap<FString, TArray<FLifePlayChatMessage>> MemoryCache;

	FString GetMemoryFilePath(const FString& NpcId) const;
	TArray<FLifePlayChatMessage>& LoadOrGetMemory(const FString& NpcId);
	void SaveMemoryToDisk(const FString& NpcId, const TArray<FLifePlayChatMessage>& Messages) const;
	void ApplyStatDelta(AActor* NpcActor, AActor* PlayerActor, float RapportDelta, float AttractionDelta, float MoneyTransfer) const;
	FLifePlayChatMessage GenerateOfflineReply(
		const FLifePlayNpcContext& Ctx,
		const FString& PlayerName,
		const FString& InputText,
		int32 TurnIndex,
		FName& OutSoundKey,
		float& OutMoneyTransfer
	) const;
};
