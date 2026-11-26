#include "LifePlayAIRoleplaySubsystem.h"

#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace LifePlayStandaloneReflection
{
	static FProperty* FindPropIgnoreCase(const UObject* Obj, const TCHAR* PropName)
	{
		if (!Obj)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static FProperty* FindAnyPropIgnoreCase(const UObject* Obj, std::initializer_list<const TCHAR*> Candidates)
	{
		for (const TCHAR* Name : Candidates)
		{
			if (FProperty* Prop = FindPropIgnoreCase(Obj, Name))
			{
				return Prop;
			}
		}
		return nullptr;
	}

	static bool ReadStringProp(const UObject* Obj, std::initializer_list<const TCHAR*> Candidates, FString& OutValue)
	{
		FProperty* Prop = FindAnyPropIgnoreCase(Obj, Candidates);
		if (!Prop)
		{
			return false;
		}
		const void* ValPtr = Prop->ContainerPtrToValuePtr<void>(Obj);
		if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			OutValue = StrProp->GetPropertyValue(ValPtr);
			return true;
		}
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			OutValue = TextProp->GetPropertyValue(ValPtr).ToString();
			return true;
		}
		if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			OutValue = NameProp->GetPropertyValue(ValPtr).ToString();
			return true;
		}
		return false;
	}

	static bool ReadBoolProp(const UObject* Obj, std::initializer_list<const TCHAR*> Candidates, bool& OutValue)
	{
		FProperty* Prop = FindAnyPropIgnoreCase(Obj, Candidates);
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			OutValue = BoolProp->GetPropertyValue_InContainer(Obj);
			return true;
		}
		return false;
	}

	static void WriteBoolProp(UObject* Obj, std::initializer_list<const TCHAR*> Candidates, bool bNewValue)
	{
		FProperty* Prop = FindAnyPropIgnoreCase(Obj, Candidates);
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			BoolProp->SetPropertyValue_InContainer(Obj, bNewValue);
		}
	}

	static bool ReadFloatProp(const UObject* Obj, std::initializer_list<const TCHAR*> Candidates, float& OutValue)
	{
		FProperty* Prop = FindAnyPropIgnoreCase(Obj, Candidates);
		if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
		{
			const void* ValPtr = Prop->ContainerPtrToValuePtr<void>(Obj);
			if (NumProp->IsFloatingPoint())
			{
				OutValue = static_cast<float>(NumProp->GetFloatingPointPropertyValue(ValPtr));
				return true;
			}
			if (NumProp->IsInteger())
			{
				OutValue = static_cast<float>(NumProp->GetSignedIntPropertyValue(ValPtr));
				return true;
			}
		}
		return false;
	}

	static void AddFloatProp(UObject* Obj, std::initializer_list<const TCHAR*> Candidates, float Delta, float MinVal, float MaxVal)
	{
		FProperty* Prop = FindAnyPropIgnoreCase(Obj, Candidates);
		if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
		{
			void* ValPtr = Prop->ContainerPtrToValuePtr<void>(Obj);
			if (NumProp->IsFloatingPoint())
			{
				const double Current = NumProp->GetFloatingPointPropertyValue(ValPtr);
				const double Next = FMath::Clamp(Current + static_cast<double>(Delta), static_cast<double>(MinVal), static_cast<double>(MaxVal));
				NumProp->SetFloatingPointPropertyValue(ValPtr, Next);
			}
			else if (NumProp->IsInteger())
			{
				const int64 Current = NumProp->GetSignedIntPropertyValue(ValPtr);
				const int64 Next = FMath::Clamp<int64>(Current + FMath::RoundToInt64(Delta), static_cast<int64>(MinVal), static_cast<int64>(MaxVal));
				NumProp->SetIntPropertyValue(ValPtr, Next);
			}
		}
	}

	static FString SanitizeId(const FString& Raw)
	{
		FString Out;
		Out.Reserve(Raw.Len());
		for (TCHAR Ch : Raw)
		{
			if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
			{
				Out.AppendChar(Ch);
			}
			else
			{
				Out.AppendChar(TEXT('_'));
			}
		}
		return Out.IsEmpty() ? TEXT("npc") : Out;
	}

	static bool ContainsAny(const FString& LowerText, std::initializer_list<const TCHAR*> Terms)
	{
		for (const TCHAR* Term : Terms)
		{
			if (LowerText.Contains(Term))
			{
				return true;
			}
		}
		return false;
	}
}

void ULifePlayAIRoleplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULifePlayAIRoleplaySubsystem::Deinitialize()
{
	NpcAIStates.Empty();
	CachedSounds.Empty();
	Super::Deinitialize();
}

void ULifePlayAIRoleplaySubsystem::ToggleAudioMute()
{
	bAudioMuted = !bAudioMuted;
}

USoundBase* ULifePlayAIRoleplaySubsystem::GetOrLoadSound(FName SoundKey)
{
	if (SoundKey.IsNone())
	{
		return nullptr;
	}
	if (TObjectPtr<USoundBase>* Found = CachedSounds.Find(SoundKey))
	{
		return Found->Get();
	}
	const FString KeyStr = SoundKey.ToString();
	const FString AssetPath = FString::Printf(TEXT("/Game/Heavy/New2/Sound/%s.%s"), *KeyStr, *KeyStr);
	USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *AssetPath);
	CachedSounds.Add(SoundKey, Loaded);
	return Loaded;
}

void ULifePlayAIRoleplaySubsystem::PlayRoleplaySound(UWorld* World, FName SoundKey, const FVector* WorldLocation, float Volume, float Pitch)
{
	if (bAudioMuted || !World || SoundKey.IsNone())
	{
		return;
	}
	USoundBase* Sound = GetOrLoadSound(SoundKey);
	if (!Sound)
	{
		return;
	}
	if (WorldLocation)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, *WorldLocation, Volume, Pitch);
	}
	else
	{
		UGameplayStatics::PlaySound2D(World, Sound, Volume, Pitch);
	}
}

ELifePlayNpcAIState ULifePlayAIRoleplaySubsystem::GetNpcAIState(AActor* NpcActor) const
{
	if (!NpcActor)
	{
		return ELifePlayNpcAIState::Idle;
	}
	if (const ELifePlayNpcAIState* Found = NpcAIStates.Find(NpcActor))
	{
		return *Found;
	}
	return ELifePlayNpcAIState::Idle;
}

FString ULifePlayAIRoleplaySubsystem::GetNpcAIStateName(AActor* NpcActor) const
{
	switch (GetNpcAIState(NpcActor))
	{
	case ELifePlayNpcAIState::Engaged: return TEXT("Engaged");
	case ELifePlayNpcAIState::Following: return TEXT("Following");
	case ELifePlayNpcAIState::Fleeing: return TEXT("Fleeing");
	case ELifePlayNpcAIState::Approaching: return TEXT("Approaching");
	case ELifePlayNpcAIState::Roaming: return TEXT("Roaming");
	default: return TEXT("Idle");
	}
}

void ULifePlayAIRoleplaySubsystem::SetNpcAIState(AActor* NpcActor, ELifePlayNpcAIState NewState)
{
	if (!NpcActor || !IsValid(NpcActor))
	{
		return;
	}
	if (NewState == ELifePlayNpcAIState::Idle)
	{
		NpcAIStates.Remove(NpcActor);
	}
	else
	{
		NpcAIStates.Add(NpcActor, NewState);
	}
}

bool ULifePlayAIRoleplaySubsystem::CanNpcSeePlayer(AActor* NpcActor, AActor* PlayerActor) const
{
	if (!NpcActor || !PlayerActor || !IsValid(NpcActor) || !IsValid(PlayerActor))
	{
		return false;
	}
	if (APawn* NpcPawn = Cast<APawn>(NpcActor))
	{
		if (AAIController* AI = Cast<AAIController>(NpcPawn->GetController()))
		{
			return AI->LineOfSightTo(PlayerActor);
		}
	}
	if (UWorld* World = NpcActor->GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LifePlayNpcLOS), false);
		Params.AddIgnoredActor(NpcActor);
		Params.AddIgnoredActor(PlayerActor);
		const FVector Start = NpcActor->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f);
		const FVector End = PlayerActor->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f);
		return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	}
	return true;
}

void ULifePlayAIRoleplaySubsystem::TickInteractionAI(UWorld* World, AActor* PlayerActor, float DeltaTime)
{
	if (!World || !PlayerActor || !IsValid(PlayerActor) || NpcAIStates.Num() == 0)
	{
		return;
	}

	AITickAccumulator += DeltaTime;
	if (AITickAccumulator < 0.35f)
	{
		return;
	}
	AITickAccumulator = 0.0f;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	const FVector PlayerLoc = PlayerActor->GetActorLocation();
	TArray<TWeakObjectPtr<AActor>> KeysToRemove;

	for (auto& Pair : NpcAIStates)
	{
		AActor* NpcActor = Pair.Key.Get();
		if (!NpcActor || !IsValid(NpcActor))
		{
			KeysToRemove.Add(Pair.Key);
			continue;
		}

		ACharacter* NpcChar = Cast<ACharacter>(NpcActor);
		APawn* NpcPawn = Cast<APawn>(NpcActor);
		AAIController* AI = NpcPawn ? Cast<AAIController>(NpcPawn->GetController()) : nullptr;
		UCharacterMovementComponent* MoveComp = NpcChar ? NpcChar->GetCharacterMovement() : nullptr;

		const FVector NpcLoc = NpcActor->GetActorLocation();
		const float Dist = FVector::Dist2D(NpcLoc, PlayerLoc);

		switch (Pair.Value)
		{
		case ELifePlayNpcAIState::Following:
			if (AI)
			{
				if (Dist > 450.0f)
				{
					if (MoveComp) MoveComp->MaxWalkSpeed = 460.0f;
					AI->ClearFocus(EAIFocusPriority::Gameplay);
					AI->MoveToActor(PlayerActor, 150.0f, true, true, true, nullptr, true);
				}
				else if (Dist > 220.0f)
				{
					if (MoveComp) MoveComp->MaxWalkSpeed = 300.0f;
					AI->ClearFocus(EAIFocusPriority::Gameplay);
					if (AI->GetMoveStatus() == EPathFollowingStatus::Idle)
					{
						AI->MoveToActor(PlayerActor, 150.0f, true, true, true, nullptr, true);
					}
				}
				else
				{
					if (MoveComp) MoveComp->MaxWalkSpeed = 240.0f;
					AI->SetFocus(PlayerActor, EAIFocusPriority::Gameplay);
				}
			}
			break;

		case ELifePlayNpcAIState::Fleeing:
			if (Dist > 1100.0f)
			{
				if (AI)
				{
					AI->StopMovement();
					AI->ClearFocus(EAIFocusPriority::Gameplay);
				}
				if (MoveComp) MoveComp->MaxWalkSpeed = 240.0f;
				KeysToRemove.Add(Pair.Key);
			}
			else if (AI)
			{
				if (MoveComp) MoveComp->MaxWalkSpeed = 420.0f;
				AI->ClearFocus(EAIFocusPriority::Gameplay);
				if (AI->GetMoveStatus() == EPathFollowingStatus::Idle)
				{
					FVector AwayDir = (NpcLoc - PlayerLoc).GetSafeNormal2D();
					if (AwayDir.IsNearlyZero())
					{
						AwayDir = FVector::ForwardVector;
					}
					FVector TargetLoc = NpcLoc + AwayDir * 900.0f;
					if (NavSys)
					{
						FNavLocation Projected;
						if (NavSys->ProjectPointToNavigation(TargetLoc, Projected, FVector(300.0f, 300.0f, 200.0f)))
						{
							TargetLoc = Projected.Location;
						}
						else if (NavSys->GetRandomReachablePointInRadius(NpcLoc + AwayDir * 450.0f, 500.0f, Projected))
						{
							TargetLoc = Projected.Location;
						}
					}
					AI->MoveToLocation(TargetLoc, 80.0f, true, true, true, false, nullptr, true);
				}
			}
			break;

		case ELifePlayNpcAIState::Approaching:
			if (Dist <= 220.0f)
			{
				if (AI)
				{
					AI->StopMovement();
					AI->SetFocus(PlayerActor, EAIFocusPriority::Gameplay);
				}
				Pair.Value = ELifePlayNpcAIState::Engaged;
				PlayRoleplaySound(World, FName(TEXT("ui_info")), &NpcLoc, 0.70f, 1.05f);
			}
			else if (AI)
			{
				if (MoveComp) MoveComp->MaxWalkSpeed = 280.0f;
				AI->ClearFocus(EAIFocusPriority::Gameplay);
				if (AI->GetMoveStatus() == EPathFollowingStatus::Idle)
				{
					AI->MoveToActor(PlayerActor, 170.0f, true, true, true, nullptr, true);
				}
			}
			break;

		case ELifePlayNpcAIState::Engaged:
			if (Dist > 650.0f)
			{
				if (AI)
				{
					AI->ClearFocus(EAIFocusPriority::Gameplay);
				}
				KeysToRemove.Add(Pair.Key);
			}
			else if (AI)
			{
				AI->SetFocus(PlayerActor, EAIFocusPriority::Gameplay);
			}
			break;

		case ELifePlayNpcAIState::Roaming:
			if (AI && AI->GetMoveStatus() == EPathFollowingStatus::Idle)
			{
				if (NavSys)
				{
					FNavLocation RoamPoint;
					if (NavSys->GetRandomReachablePointInRadius(NpcLoc, 650.0f, RoamPoint))
					{
						if (MoveComp) MoveComp->MaxWalkSpeed = 150.0f;
						AI->ClearFocus(EAIFocusPriority::Gameplay);
						AI->MoveToLocation(RoamPoint.Location, 60.0f, true, true, true, false, nullptr, true);
					}
					else
					{
						KeysToRemove.Add(Pair.Key);
					}
				}
				else
				{
					KeysToRemove.Add(Pair.Key);
				}
			}
			break;

		default:
			KeysToRemove.Add(Pair.Key);
			break;
		}
	}

	for (const TWeakObjectPtr<AActor>& Key : KeysToRemove)
	{
		NpcAIStates.Remove(Key);
	}
}

FString ULifePlayAIRoleplaySubsystem::GenerateStreetReaction(AActor* NpcActor, AActor* PlayerActor) const
{
	const FLifePlayNpcContext Ctx = ExtractNpcContext(NpcActor);
	const ELifePlayNpcAIState State = GetNpcAIState(NpcActor);
	const FString PlayerName = PlayerActor ? GetNpcDisplayName(PlayerActor) : TEXT("friend");

	if (State == ELifePlayNpcAIState::Fleeing || Ctx.Rapport < -15.0f)
	{
		return FString::Printf(TEXT("%s: \"Hey, give me some space!\""), *Ctx.FullName);
	}
	if (State == ELifePlayNpcAIState::Following)
	{
		return FString::Printf(TEXT("%s: \"Right behind you, %s!\""), *Ctx.FullName, *PlayerName);
	}
	if (Ctx.Rapport >= 20.0f)
	{
		return FString::Printf(TEXT("%s: \"Hey %s! Good seeing you out here.\""), *Ctx.FullName, *PlayerName);
	}
	return FString::Printf(TEXT("%s: \"Hey there — press [E] to talk.\""), *Ctx.FullName);
}

FString ULifePlayAIRoleplaySubsystem::GetMemoryFilePath(const FString& NpcId) const
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("LifePlayRoleplay"),
		FString::Printf(TEXT("memory_%s.json"), *LifePlayStandaloneReflection::SanitizeId(NpcId))
	);
}

bool ULifePlayAIRoleplaySubsystem::IsPersonActor(const AActor* Actor)
{
	if (!Actor || !IsValid(Actor))
	{
		return false;
	}
	const FString ClassName = Actor->GetClass()->GetName();
	if (ClassName.StartsWith(TEXT("Person")) || ClassName.Contains(TEXT("Person_C")))
	{
		return true;
	}
	return LifePlayStandaloneReflection::FindPropIgnoreCase(Actor, TEXT("isPlayer")) != nullptr
		&& LifePlayStandaloneReflection::FindPropIgnoreCase(Actor, TEXT("Name_First")) != nullptr;
}

bool ULifePlayAIRoleplaySubsystem::IsPlayerPersonActor(const AActor* Actor)
{
	if (!IsPersonActor(Actor))
	{
		return false;
	}
	bool bIsPlayer = false;
	if (LifePlayStandaloneReflection::ReadBoolProp(Actor, { TEXT("isPlayer") }, bIsPlayer))
	{
		return bIsPlayer;
	}
	return false;
}

FString ULifePlayAIRoleplaySubsystem::GetNpcDisplayName(AActor* NpcActor)
{
	if (!NpcActor)
	{
		return TEXT("Resident");
	}
	FString First, Last;
	LifePlayStandaloneReflection::ReadStringProp(NpcActor, { TEXT("Name_First"), TEXT("FirstName") }, First);
	LifePlayStandaloneReflection::ReadStringProp(NpcActor, { TEXT("Name_Last"), TEXT("LastName") }, Last);
	const FString Full = (First.TrimStartAndEnd() + TEXT(" ") + Last.TrimStartAndEnd()).TrimStartAndEnd();
	return Full.IsEmpty() ? NpcActor->GetName() : Full;
}

FLifePlayNpcContext ULifePlayAIRoleplaySubsystem::ExtractNpcContext(AActor* NpcActor) const
{
	FLifePlayNpcContext Ctx;
	if (!NpcActor)
	{
		Ctx.NpcId = TEXT("unknown");
		Ctx.FullName = TEXT("Resident");
		return Ctx;
	}

	Ctx.FullName = GetNpcDisplayName(NpcActor);
	Ctx.NpcId = LifePlayStandaloneReflection::SanitizeId(Ctx.FullName + TEXT("_") + NpcActor->GetName());

	float AgeFloat = 25.0f;
	if (LifePlayStandaloneReflection::ReadFloatProp(NpcActor, { TEXT("age"), TEXT("AgeCharacter") }, AgeFloat))
	{
		Ctx.Age = FMath::Max(18, FMath::RoundToInt(AgeFloat));
	}

	FString JobStr;
	if (LifePlayStandaloneReflection::ReadStringProp(NpcActor, { TEXT("job"), TEXT("Job") }, JobStr) && !JobStr.IsEmpty())
	{
		Ctx.Job = JobStr;
	}

	FString MoodStr;
	if (LifePlayStandaloneReflection::ReadStringProp(NpcActor, { TEXT("CurrentMood"), TEXT("mood"), TEXT("Mood") }, MoodStr) && !MoodStr.IsEmpty())
	{
		Ctx.Mood = MoodStr;
	}

	LifePlayStandaloneReflection::ReadFloatProp(NpcActor, { TEXT("rapportwithplayer"), TEXT("Rapport") }, Ctx.Rapport);
	LifePlayStandaloneReflection::ReadFloatProp(NpcActor, { TEXT("attractiontoplayer"), TEXT("Attraction") }, Ctx.Attraction);

	FString BuildingName;
	if (LifePlayStandaloneReflection::ReadStringProp(NpcActor, { TEXT("CurrentPlayerBuilding"), TEXT("BuildingName") }, BuildingName) && !BuildingName.IsEmpty())
	{
		Ctx.LocationName = BuildingName;
	}
	else if (UWorld* World = NpcActor->GetWorld())
	{
		Ctx.LocationName = World->GetMapName();
	}

	return Ctx;
}

TArray<FLifePlayChatMessage>& ULifePlayAIRoleplaySubsystem::LoadOrGetMemory(const FString& NpcId)
{
	if (TArray<FLifePlayChatMessage>* Existing = MemoryCache.Find(NpcId))
	{
		return *Existing;
	}

	TArray<FLifePlayChatMessage>& Messages = MemoryCache.Add(NpcId);
	FString RawJson;
	if (!FFileHelper::LoadFileToString(RawJson, *GetMemoryFilePath(NpcId)))
	{
		return Messages;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		for (const TSharedPtr<FJsonValue>& Val : JsonArray)
		{
			const TSharedPtr<FJsonObject> Obj = Val->AsObject();
			if (!Obj.IsValid())
			{
				continue;
			}
			FLifePlayChatMessage Msg;
			Obj->TryGetStringField(TEXT("role"), Msg.Role);
			Obj->TryGetStringField(TEXT("text"), Msg.Text);
			Obj->TryGetStringField(TEXT("emotion"), Msg.Emotion);
			Obj->TryGetStringField(TEXT("action"), Msg.Action);
			double Delta = 0.0;
			if (Obj->TryGetNumberField(TEXT("relationship_delta"), Delta))
			{
				Msg.RelationshipDelta = static_cast<float>(Delta);
			}
			double AttrDelta = 0.0;
			if (Obj->TryGetNumberField(TEXT("attraction_delta"), AttrDelta))
			{
				Msg.AttractionDelta = static_cast<float>(AttrDelta);
			}
			if (!Msg.Text.IsEmpty())
			{
				Messages.Add(MoveTemp(Msg));
			}
		}
	}
	return Messages;
}

void ULifePlayAIRoleplaySubsystem::SaveMemoryToDisk(const FString& NpcId, const TArray<FLifePlayChatMessage>& Messages) const
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (const FLifePlayChatMessage& Msg : Messages)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("role"), Msg.Role);
		Obj->SetStringField(TEXT("text"), Msg.Text);
		Obj->SetStringField(TEXT("emotion"), Msg.Emotion);
		Obj->SetStringField(TEXT("action"), Msg.Action);
		Obj->SetNumberField(TEXT("relationship_delta"), Msg.RelationshipDelta);
		Obj->SetNumberField(TEXT("attraction_delta"), Msg.AttractionDelta);
		JsonArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonArray, Writer);
	FFileHelper::SaveStringToFile(OutputString, *GetMemoryFilePath(NpcId));
}

TArray<FLifePlayChatMessage> ULifePlayAIRoleplaySubsystem::GetHistoryForNpc(const FString& NpcId)
{
	return LoadOrGetMemory(NpcId);
}

void ULifePlayAIRoleplaySubsystem::ClearNpcMemory(const FString& NpcId)
{
	MemoryCache.Remove(NpcId);
	IFileManager::Get().Delete(*GetMemoryFilePath(NpcId));
}

void ULifePlayAIRoleplaySubsystem::ApplyStatDelta(AActor* NpcActor, AActor* PlayerActor, float RapportDelta, float AttractionDelta, float MoneyTransfer) const
{
	if (NpcActor)
	{
		if (!FMath::IsNearlyZero(RapportDelta))
		{
			LifePlayStandaloneReflection::AddFloatProp(NpcActor, { TEXT("rapportwithplayer"), TEXT("Rapport") }, RapportDelta, -100.0f, 100.0f);
		}
		if (!FMath::IsNearlyZero(AttractionDelta))
		{
			LifePlayStandaloneReflection::AddFloatProp(NpcActor, { TEXT("attractiontoplayer"), TEXT("Attraction") }, AttractionDelta, -100.0f, 100.0f);
		}
		if (MoneyTransfer > 0.0f)
		{
			LifePlayStandaloneReflection::AddFloatProp(NpcActor, { TEXT("money"), TEXT("Money") }, MoneyTransfer, 0.0f, 10000000.0f);
		}
	}
	if (PlayerActor && MoneyTransfer > 0.0f)
	{
		LifePlayStandaloneReflection::AddFloatProp(PlayerActor, { TEXT("money"), TEXT("Money") }, -MoneyTransfer, 0.0f, 10000000.0f);
	}
}

void ULifePlayAIRoleplaySubsystem::ExecuteNpcAction(AActor* NpcActor, AActor* PlayerActor, const FString& Action)
{
	if (!NpcActor || !IsValid(NpcActor))
	{
		return;
	}

	const FString Normalized = Action.TrimStartAndEnd().ToLower();
	if (Normalized.IsEmpty() || Normalized == TEXT("none"))
	{
		return;
	}

	APawn* NpcPawn = Cast<APawn>(NpcActor);
	AAIController* AI = NpcPawn ? Cast<AAIController>(NpcPawn->GetController()) : nullptr;

	if (PlayerActor && IsValid(PlayerActor) && (Normalized == TEXT("face_player") || Normalized == TEXT("wave") || Normalized == TEXT("engage")))
	{
		FVector Dir = PlayerActor->GetActorLocation() - NpcActor->GetActorLocation();
		Dir.Z = 0.0f;
		if (!Dir.IsNearlyZero())
		{
			const FRotator CurrentRot = NpcActor->GetActorRotation();
			NpcActor->SetActorRotation(FRotator(CurrentRot.Pitch, Dir.Rotation().Yaw, CurrentRot.Roll));
		}
		if (AI)
		{
			AI->SetFocus(PlayerActor, EAIFocusPriority::Gameplay);
		}
		if (GetNpcAIState(NpcActor) != ELifePlayNpcAIState::Following)
		{
			SetNpcAIState(NpcActor, ELifePlayNpcAIState::Engaged);
		}
		return;
	}

	if ((Normalized == TEXT("follow") || Normalized == TEXT("follow_player")) && PlayerActor && IsValid(PlayerActor))
	{
		LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, true);
		SetNpcAIState(NpcActor, ELifePlayNpcAIState::Following);
		if (AI)
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->MoveToActor(PlayerActor, 150.0f, true, true, true, nullptr, true);
		}
		return;
	}

	if (Normalized == TEXT("call_over") || Normalized == TEXT("approach"))
	{
		SetNpcAIState(NpcActor, ELifePlayNpcAIState::Approaching);
		if (AI && PlayerActor && IsValid(PlayerActor))
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->MoveToActor(PlayerActor, 170.0f, true, true, true, nullptr, true);
		}
		return;
	}

	if (Normalized == TEXT("flee") || Normalized == TEXT("run_away"))
	{
		LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, false);
		SetNpcAIState(NpcActor, ELifePlayNpcAIState::Fleeing);
		if (AI)
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->StopMovement();
		}
		return;
	}

	if (Normalized == TEXT("roam"))
	{
		LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, false);
		SetNpcAIState(NpcActor, ELifePlayNpcAIState::Roaming);
		if (AI)
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->StopMovement();
		}
		return;
	}

	if (Normalized == TEXT("stop_follow") || Normalized == TEXT("stop_following") || Normalized == TEXT("stay"))
	{
		LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, false);
		SetNpcAIState(NpcActor, ELifePlayNpcAIState::Idle);
		if (AI)
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->StopMovement();
		}
	}
}

FLifePlayChatMessage ULifePlayAIRoleplaySubsystem::GenerateOfflineReply(
	const FLifePlayNpcContext& Ctx,
	const FString& PlayerName,
	const FString& InputText,
	int32 TurnIndex,
	FName& OutSoundKey,
	float& OutMoneyTransfer) const
{
	FLifePlayChatMessage Reply;
	Reply.Role = TEXT("assistant");
	Reply.Action = TEXT("face_player");
	OutSoundKey = FName(TEXT("ui_info"));
	OutMoneyTransfer = 0.0f;

	const FString Lower = InputText.ToLower();
	const int32 Variant = FMath::Abs(TurnIndex) % 3;
	const bool bHostile = Ctx.Rapport < -15.0f;
	const bool bFriendly = Ctx.Rapport >= 15.0f;
	const bool bHighAttraction = Ctx.Attraction >= 20.0f;
	const FString JobLabel = Ctx.Job.IsEmpty() ? TEXT("resident around here") : Ctx.Job;

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("gift"), TEXT("25"), TEXT("money"), TEXT("coffee on me"), TEXT("cash") }))
	{
		OutMoneyTransfer = 25.0f;
		OutSoundKey = FName(TEXT("ui_money"));
		Reply.Emotion = TEXT("happy");
		Reply.Action = TEXT("face_player");
		Reply.RelationshipDelta = 6.0f;
		Reply.AttractionDelta = 2.0f;
		Reply.Text = FString::Printf(TEXT("Whoa, £25? Thanks a lot, %s! That really made my day."), *PlayerName);
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("sorry"), TEXT("apologize"), TEXT("my bad"), TEXT("calm down"), TEXT("peace") }))
	{
		OutSoundKey = FName(TEXT("ui_positive"));
		Reply.Emotion = TEXT("neutral");
		Reply.Action = TEXT("face_player");
		Reply.RelationshipDelta = 4.0f;
		Reply.Text = TEXT("Alright, alright — I appreciate you owning up to it. Let's start fresh.");
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("call over"), TEXT("come here"), TEXT("over here"), TEXT("walk over") }))
	{
		OutSoundKey = FName(TEXT("ui_info"));
		Reply.Action = TEXT("call_over");
		Reply.Emotion = bHostile ? TEXT("neutral") : TEXT("happy");
		Reply.RelationshipDelta = 1.0f;
		Reply.Text = TEXT("Hold on, I'm walking over to you now.");
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("follow"), TEXT("come with"), TEXT("walk with"), TEXT("hang out"), TEXT("roll with") }))
	{
		if (bHostile)
		{
			OutSoundKey = FName(TEXT("ui_negative"));
			Reply.Text = TEXT("Forget it. Apologize first if you expect me to walk with you.");
			Reply.Emotion = TEXT("angry");
			Reply.Action = TEXT("face_player");
			Reply.RelationshipDelta = -1.0f;
		}
		else
		{
			OutSoundKey = FName(TEXT("ui_quest"));
			Reply.Action = TEXT("follow");
			Reply.Emotion = TEXT("happy");
			Reply.RelationshipDelta = 2.0f;
			static const TCHAR* FollowLines[3] = {
				TEXT("Sure thing, lead the way! I'll keep up with your pace."),
				TEXT("Count me in — let's cruise through the neighborhood together."),
				TEXT("Sounds fun! Walk wherever you want, I'm right behind you.")
			};
			Reply.Text = FollowLines[Variant];
		}
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("stay"), TEXT("stop follow"), TEXT("wait here"), TEXT("bye"), TEXT("later") }))
	{
		OutSoundKey = FName(TEXT("ui_click"));
		Reply.Action = TEXT("stop_follow");
		Reply.Emotion = TEXT("neutral");
		Reply.RelationshipDelta = 1.0f;
		static const TCHAR* StayLines[3] = {
			TEXT("Got it, I'll hold up here. Catch you around the block."),
			TEXT("Alright, I'll head off on my own. Talk later!"),
			TEXT("Sounds good — see you around London.")
		};
		Reply.Text = StayLines[Variant];
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("provoke"), TEXT("intimidate"), TEXT("idiot"), TEXT("fight"), TEXT("back off"), TEXT("stupid"), TEXT("hate"), TEXT("shut up"), TEXT("rob") }))
	{
		OutSoundKey = FName(TEXT("ui_negative"));
		Reply.Emotion = TEXT("angry");
		Reply.Action = TEXT("flee");
		Reply.RelationshipDelta = -8.0f;
		Reply.AttractionDelta = -4.0f;
		static const TCHAR* AngryLines[3] = {
			TEXT("Whoa, back off! I'm getting out of here before you start something!"),
			TEXT("Are you crazy?! Forget this, I'm walking away right now!"),
			TEXT("Leave me alone! I don't need this street drama!")
		};
		Reply.Text = AngryLines[Variant];
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("joke"), TEXT("funny"), TEXT("laugh") }))
	{
		OutSoundKey = FName(TEXT("ui_positive"));
		Reply.Emotion = TEXT("happy");
		Reply.RelationshipDelta = 3.0f;
		static const TCHAR* JokeLines[3] = {
			TEXT("Haha, okay that actually caught me off guard — not bad at all!"),
			TEXT("You're ridiculous, haha! Needed a good laugh today."),
			TEXT("Alright, you win — that's the funniest thing I've heard all week.")
		};
		Reply.Text = JokeLines[Variant];
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("flirt"), TEXT("cute"), TEXT("hot"), TEXT("date"), TEXT("pretty"), TEXT("handsome"), TEXT("love"), TEXT("kiss"), TEXT("drink") }))
	{
		if (bHostile)
		{
			OutSoundKey = FName(TEXT("ui_negative"));
			Reply.Emotion = TEXT("angry");
			Reply.RelationshipDelta = -2.0f;
			Reply.Text = TEXT("Save the smooth talk. Try apologizing before flirting with me.");
		}
		else if (bFriendly || bHighAttraction)
		{
			OutSoundKey = (Ctx.Rapport >= 35.0f) ? FName(TEXT("ui_achievement")) : FName(TEXT("ui_positive"));
			Reply.Emotion = TEXT("flirty");
			Reply.RelationshipDelta = 3.0f;
			Reply.AttractionDelta = 5.0f;
			if (Variant == 0)
			{
				Reply.Text = FString::Printf(TEXT("Careful, %s — keep talking like that and I might clear my whole evening for you."), *PlayerName);
			}
			else if (Variant == 1)
			{
				Reply.Text = TEXT("Haha, you've definitely got charm. Walking around the city with you is my favorite part of the day.");
			}
			else
			{
				Reply.Text = TEXT("Now that's the energy I love. Ask me to follow and let's go explore!");
			}
		}
		else
		{
			OutSoundKey = FName(TEXT("ui_positive"));
			Reply.Emotion = TEXT("flirty");
			Reply.RelationshipDelta = 2.0f;
			Reply.AttractionDelta = 3.0f;
			Reply.Text = TEXT("Oh? Bold move right on the street! Keep being sweet and you might win me over.");
		}
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("job"), TEXT("work"), TEXT("life"), TEXT("mood"), TEXT("how are"), TEXT("who are"), TEXT("city"), TEXT("london") }))
	{
		OutSoundKey = FName(TEXT("ui_info"));
		Reply.Emotion = bHostile ? TEXT("angry") : TEXT("happy");
		Reply.RelationshipDelta = bHostile ? 0.0f : 2.0f;
		if (Variant == 0)
		{
			Reply.Text = FString::Printf(TEXT("I'm %s, %d, working as a %s. Feeling %s today — always something going on in this city."), *Ctx.FullName, Ctx.Age, *JobLabel, *Ctx.Mood.ToLower());
		}
		else if (Variant == 1)
		{
			Reply.Text = FString::Printf(TEXT("Life as a %s keeps me moving. Mood's %s right now, and talking with you helps."), *JobLabel, *Ctx.Mood.ToLower());
		}
		else
		{
			Reply.Text = FString::Printf(TEXT("Just hustling through the week at %d. Every block in London has its own vibe."), Ctx.Age);
		}
		return Reply;
	}

	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("hello"), TEXT("hi"), TEXT("hey"), TEXT("greet"), TEXT("morning"), TEXT("evening"), TEXT("sup") }))
	{
		OutSoundKey = bHostile ? FName(TEXT("ui_negative")) : FName(TEXT("ui_positive"));
		if (bHostile)
		{
			Reply.Emotion = TEXT("angry");
			Reply.RelationshipDelta = 0.0f;
			Reply.Text = FString::Printf(TEXT("Yeah, what do you want, %s? Make it quick."), *PlayerName);
		}
		else if (bFriendly)
		{
			Reply.Emotion = TEXT("happy");
			Reply.RelationshipDelta = 2.0f;
			Reply.Text = FString::Printf(TEXT("Hey %s! Great running into you again. Want to walk around?"), *PlayerName);
		}
		else
		{
			Reply.Emotion = TEXT("happy");
			Reply.RelationshipDelta = 2.0f;
			Reply.Text = FString::Printf(TEXT("Hey there! I'm %s — nice seeing a friendly face on the street."), *Ctx.FullName);
		}
		return Reply;
	}

	OutSoundKey = FName(TEXT("ui_info"));
	if (bHostile)
	{
		Reply.Emotion = TEXT("angry");
		Reply.RelationshipDelta = 0.0f;
		Reply.Text = TEXT("Look, I hear you — just keep it respectful if you want to keep talking.");
	}
	else if (bFriendly)
	{
		Reply.Emotion = TEXT("happy");
		Reply.RelationshipDelta = 1.0f;
		static const TCHAR* FriendlyLines[3] = {
			TEXT("Haha, fair point! Hanging out around town with you beats the usual routine."),
			TEXT("I feel that. Just click Follow Me anytime you want company on a walk."),
			TEXT("True! Funny how a quick street chat turns into the best part of the day.")
		};
		Reply.Text = FriendlyLines[Variant];
	}
	else
	{
		Reply.Emotion = TEXT("neutral");
		Reply.RelationshipDelta = 1.0f;
		if (Variant == 0)
		{
			Reply.Text = TEXT("Interesting take. Walking these streets like you own the place — I respect the confidence.");
		}
		else if (Variant == 1)
		{
			Reply.Text = FString::Printf(TEXT("Good point. Between %s shifts, it's nice talking to someone real."), *JobLabel);
		}
		else
		{
			Reply.Text = TEXT("Gotcha. Let me know if you want to hang out or catch up later.");
		}
	}
	return Reply;
}

FLifePlayChatMessage ULifePlayAIRoleplaySubsystem::SendRoleplayMessage(
	AActor* NpcActor,
	AActor* PlayerActor,
	const FString& PlayerMessage)
{
	const FString Trimmed = PlayerMessage.TrimStartAndEnd();
	if (!NpcActor || Trimmed.IsEmpty())
	{
		return FLifePlayChatMessage();
	}

	const FLifePlayNpcContext Ctx = ExtractNpcContext(NpcActor);
	const FString PlayerName = PlayerActor ? GetNpcDisplayName(PlayerActor) : TEXT("You");
	TArray<FLifePlayChatMessage>& History = LoadOrGetMemory(Ctx.NpcId);

	FLifePlayChatMessage UserMsg;
	UserMsg.Role = TEXT("user");
	UserMsg.Text = Trimmed;
	History.Add(UserMsg);

	FName SoundKey = FName(TEXT("ui_info"));
	float MoneyTransfer = 0.0f;
	FLifePlayChatMessage ReplyMsg = GenerateOfflineReply(Ctx, PlayerName, Trimmed, History.Num(), SoundKey, MoneyTransfer);
	History.Add(ReplyMsg);

	if (History.Num() > MaxHistoryTurns)
	{
		History.RemoveAt(0, History.Num() - MaxHistoryTurns);
	}
	SaveMemoryToDisk(Ctx.NpcId, History);

	ApplyStatDelta(NpcActor, PlayerActor, ReplyMsg.RelationshipDelta, ReplyMsg.AttractionDelta, MoneyTransfer);
	ExecuteNpcAction(NpcActor, PlayerActor, ReplyMsg.Action);

	const FVector NpcLoc = NpcActor->GetActorLocation();
	const float Pitch = FMath::FRandRange(0.95f, 1.05f);
	PlayRoleplaySound(NpcActor->GetWorld(), SoundKey, &NpcLoc, 0.85f, Pitch);

	OnRoleplayReplyReceived.Broadcast(Ctx.NpcId, true, ReplyMsg);
	return ReplyMsg;
}

FLifePlayChatMessage ULifePlayAIRoleplaySubsystem::TriggerPresetInteraction(
	AActor* NpcActor,
	AActor* PlayerActor,
	const FString& InteractionId)
{
	const FString Id = InteractionId.TrimStartAndEnd().ToLower();
	if (Id == TEXT("greet")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Hey! How's your day going?"));
	if (Id == TEXT("ask_life")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Tell me about your life, job, and how you're feeling today."));
	if (Id == TEXT("joke")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Want to hear a quick joke to make you laugh?"));
	if (Id == TEXT("flirt")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("You look amazing today — want to hang out later?"));
	if (Id == TEXT("gift")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Here's a £25 gift — grab a coffee on me!"));
	if (Id == TEXT("follow")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Come follow me and walk around the city together."));
	if (Id == TEXT("stay")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Wait here for a bit, see you later."));
	if (Id == TEXT("call_over")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Hey, come here and walk over for a second!"));
	if (Id == TEXT("apologize")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Hey, I'm sorry about earlier — peace?"));
	if (Id == TEXT("provoke")) return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Watch where you're going — back off!"));
	return SendRoleplayMessage(NpcActor, PlayerActor, InteractionId);
}
