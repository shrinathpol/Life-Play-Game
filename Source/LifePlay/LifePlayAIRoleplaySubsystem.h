#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LifePlayAIRoleplaySubsystem.generated.h"

class AActor;

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
 * 100% standalone offline NPC role-play & social action subsystem.
 * Reads live LifePlay character attributes, persists per-NPC conversation memory locally,
 * updates rapport/attraction, and executes physical world actions (follow/stay/face player).
 */
UCLASS()
class LIFEPLAY_API ULifePlayAIRoleplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "LifePlay|Roleplay")
	FOnLifePlayRoleplayReply OnRoleplayReplyReceived;

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
	TMap<FString, TArray<FLifePlayChatMessage>> MemoryCache;

	FString GetMemoryFilePath(const FString& NpcId) const;
	TArray<FLifePlayChatMessage>& LoadOrGetMemory(const FString& NpcId);
	void SaveMemoryToDisk(const FString& NpcId, const TArray<FLifePlayChatMessage>& Messages) const;
	void ApplyStatDelta(AActor* NpcActor, float RapportDelta, float AttractionDelta) const;
	FLifePlayChatMessage GenerateOfflineReply(
		const FLifePlayNpcContext& Ctx,
		const FString& PlayerName,
		const FString& InputText,
		int32 TurnIndex
	) const;
};
