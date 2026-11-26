#include "LifePlayAIRoleplaySubsystem.h"

#include "AIController.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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

void ULifePlayAIRoleplaySubsystem::ApplyStatDelta(AActor* NpcActor, float RapportDelta, float AttractionDelta) const
{
	if (!NpcActor)
	{
		return;
	}
	if (!FMath::IsNearlyZero(RapportDelta))
	{
		LifePlayStandaloneReflection::AddFloatProp(NpcActor, { TEXT("rapportwithplayer"), TEXT("Rapport") }, RapportDelta, -100.0f, 100.0f);
	}
	if (!FMath::IsNearlyZero(AttractionDelta))
	{
		LifePlayStandaloneReflection::AddFloatProp(NpcActor, { TEXT("attractiontoplayer"), TEXT("Attraction") }, AttractionDelta, -100.0f, 100.0f);
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

	if (PlayerActor && IsValid(PlayerActor) && (Normalized == TEXT("face_player") || Normalized == TEXT("wave")))
	{
		FVector Dir = PlayerActor->GetActorLocation() - NpcActor->GetActorLocation();
		Dir.Z = 0.0f;
		if (!Dir.IsNearlyZero())
		{
			const FRotator CurrentRot = NpcActor->GetActorRotation();
			NpcActor->SetActorRotation(FRotator(CurrentRot.Pitch, Dir.Rotation().Yaw, CurrentRot.Roll));
		}
	}

	if (APawn* NpcPawn = Cast<APawn>(NpcActor))
	{
		if (AAIController* AI = Cast<AAIController>(NpcPawn->GetController()))
		{
			if ((Normalized == TEXT("follow") || Normalized == TEXT("follow_player")) && PlayerActor && IsValid(PlayerActor))
			{
				LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, true);
				AI->MoveToActor(PlayerActor, 150.0f, true, true, true, nullptr, true);
			}
			else if (Normalized == TEXT("stop_follow") || Normalized == TEXT("stop_following") || Normalized == TEXT("stay"))
			{
				LifePlayStandaloneReflection::WriteBoolProp(NpcActor, { TEXT("FollowPlayer") }, false);
				AI->StopMovement();
			}
		}
	}
}

FLifePlayChatMessage ULifePlayAIRoleplaySubsystem::GenerateOfflineReply(
	const FLifePlayNpcContext& Ctx,
	const FString& PlayerName,
	const FString& InputText,
	int32 TurnIndex) const
{
	FLifePlayChatMessage Reply;
	Reply.Role = TEXT("assistant");
	Reply.Action = TEXT("face_player");

	const FString Lower = InputText.ToLower();
	const int32 Variant = FMath::Abs(TurnIndex) % 3;
	const bool bHostile = Ctx.Rapport < -15.0f;
	const bool bFriendly = Ctx.Rapport >= 15.0f;
	const bool bHighAttraction = Ctx.Attraction >= 20.0f;
	const FString JobLabel = Ctx.Job.IsEmpty() ? TEXT("resident around here") : Ctx.Job;

	// 1. Follow / hang out command
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("follow"), TEXT("come with"), TEXT("walk with"), TEXT("hang out"), TEXT("roll with") }))
	{
		if (bHostile)
		{
			Reply.Text = TEXT("Forget it. After the way things went between us, I am walking on my own.");
			Reply.Emotion = TEXT("angry");
			Reply.Action = TEXT("face_player");
			Reply.RelationshipDelta = -1.0f;
		}
		else
		{
			Reply.Action = TEXT("follow");
			Reply.Emotion = TEXT("happy");
			Reply.RelationshipDelta = 2.0f;
			static const TCHAR* FollowLines[3] = {
				TEXT("Sure thing, lead the way. Let's see what's happening down the street."),
				TEXT("Alright, I've got a few minutes free. Where are we heading?"),
				TEXT("Count me in — walking together beats standing around alone.")
			};
			Reply.Text = FollowLines[Variant];
		}
		return Reply;
	}

	// 2. Stop following / stay command
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("stay"), TEXT("stop follow"), TEXT("wait here"), TEXT("bye"), TEXT("later"), TEXT("part ways") }))
	{
		Reply.Action = TEXT("stop_follow");
		Reply.Emotion = TEXT("neutral");
		Reply.RelationshipDelta = 1.0f;
		static const TCHAR* StayLines[3] = {
			TEXT("Got it, I'll hold up right here. Catch you around the block."),
			TEXT("Alright, I'll take off for a bit. Talk to you later."),
			TEXT("Sounds good — see you around the city.")
		};
		Reply.Text = StayLines[Variant];
		return Reply;
	}

	// 3. Provoke / confrontation / GTA street banter
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("provoke"), TEXT("idiot"), TEXT("fight"), TEXT("back off"), TEXT("stupid"), TEXT("hate"), TEXT("ugly"), TEXT("shut up"), TEXT("rob") }))
	{
		Reply.Emotion = TEXT("angry");
		Reply.Action = TEXT("stop_follow");
		Reply.RelationshipDelta = -6.0f;
		Reply.AttractionDelta = -4.0f;
		static const TCHAR* AngryLines[3] = {
			TEXT("Watch your mouth! You're asking for trouble talking to me like that."),
			TEXT("Whoa, easy! Keep stepping if you don't want a scene right here on the sidewalk."),
			TEXT("Seriously? Do yourself a favor and cool off before things get ugly.")
		};
		Reply.Text = AngryLines[Variant];
		return Reply;
	}

	// 4. Flirt / romance / compliment
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("flirt"), TEXT("cute"), TEXT("hot"), TEXT("date"), TEXT("pretty"), TEXT("handsome"), TEXT("love"), TEXT("kiss"), TEXT("drink") }))
	{
		if (bHostile)
		{
			Reply.Emotion = TEXT("angry");
			Reply.RelationshipDelta = -2.0f;
			Reply.Text = TEXT("Save the smooth talk. You're going to have to earn back some trust first.");
		}
		else if (bFriendly || bHighAttraction)
		{
			Reply.Emotion = TEXT("flirty");
			Reply.RelationshipDelta = 3.0f;
			Reply.AttractionDelta = 5.0f;
			if (Variant == 0)
			{
				Reply.Text = FString::Printf(TEXT("Careful, %s — keep talking like that and I might actually clear my schedule tonight."), *PlayerName);
			}
			else if (Variant == 1)
			{
				Reply.Text = TEXT("Haha, you've got charm, I'll give you that. Walking with you makes this whole neighborhood better.");
			}
			else
			{
				Reply.Text = TEXT("Now that's the kind of energy I like. Let's grab a coffee or walk down the avenue together.");
			}
		}
		else
		{
			Reply.Emotion = TEXT("flirty");
			Reply.RelationshipDelta = 2.0f;
			Reply.AttractionDelta = 3.0f;
			Reply.Text = TEXT("Oh? Bold move right in the middle of the street. Keep being sweet and maybe you'll win me over.");
		}
		return Reply;
	}

	// 5. Ask about job / city / mood / life
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("job"), TEXT("work"), TEXT("life"), TEXT("mood"), TEXT("how are"), TEXT("who are"), TEXT("city"), TEXT("london") }))
	{
		Reply.Emotion = bHostile ? TEXT("angry") : TEXT("happy");
		Reply.RelationshipDelta = bHostile ? 0.0f : 2.0f;
		if (Variant == 0)
		{
			Reply.Text = FString::Printf(TEXT("I'm %s, %d, working as a %s. Feeling pretty %s right now — the streets never really slow down."), *Ctx.FullName, Ctx.Age, *JobLabel, *Ctx.Mood.ToLower());
		}
		else if (Variant == 1)
		{
			Reply.Text = FString::Printf(TEXT("Life as a %s keeps me busy, honestly. Today's mood is %s, though chatting with you helps pass the time."), *JobLabel, *Ctx.Mood.ToLower());
		}
		else
		{
			Reply.Text = FString::Printf(TEXT("Just trying to hustle through the week like everyone else. Being %d in this city means every block has a story."), Ctx.Age);
		}
		return Reply;
	}

	// 6. Greeting / first hello
	if (LifePlayStandaloneReflection::ContainsAny(Lower, { TEXT("hello"), TEXT("hi"), TEXT("hey"), TEXT("greet"), TEXT("morning"), TEXT("evening"), TEXT("sup") }))
	{
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
			Reply.Text = FString::Printf(TEXT("Hey %s! Good seeing a friendly face around here. What are you up to?"), *PlayerName);
		}
		else
		{
			Reply.Emotion = TEXT("neutral");
			Reply.RelationshipDelta = 2.0f;
			Reply.Text = FString::Printf(TEXT("Hey there. Name's %s — nice weather for a walk around the city."), *Ctx.FullName);
		}
		return Reply;
	}

	// 7. General free-text roleplay response tailored to current rapport & mood
	if (bHostile)
	{
		Reply.Emotion = TEXT("angry");
		Reply.RelationshipDelta = 0.0f;
		Reply.Text = TEXT("Look, I hear you, just keep it respectful if you want to talk.");
	}
	else if (bFriendly)
	{
		Reply.Emotion = TEXT("happy");
		Reply.RelationshipDelta = 1.0f;
		static const TCHAR* FriendlyLines[3] = {
			TEXT("Haha, fair point. Honestly, hanging out around town with you beats the usual routine."),
			TEXT("I feel that. Tell you what — if you want me to tag along for a bit, just say the word."),
			TEXT("True! Funny how a random street chat turns out to be the highlight of the day.")
		};
		Reply.Text = FriendlyLines[Variant];
	}
	else
	{
		Reply.Emotion = TEXT("neutral");
		Reply.RelationshipDelta = 1.0f;
		if (Variant == 0)
		{
			Reply.Text = FString::Printf(TEXT("Interesting take. Coming from someone walking these streets like you own them, I kind of respect it."), *PlayerName);
		}
		else if (Variant == 1)
		{
			Reply.Text = FString::Printf(TEXT("Hmm, I hadn't looked at it like that. Between %s shifts, it's nice having someone real to talk to."), *JobLabel);
		}
		else
		{
			Reply.Text = TEXT("Gotcha. Let me know if you want to keep walking together or catch up later.");
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

	FLifePlayChatMessage ReplyMsg = GenerateOfflineReply(Ctx, PlayerName, Trimmed, History.Num());
	History.Add(ReplyMsg);

	if (History.Num() > MaxHistoryTurns)
	{
		History.RemoveAt(0, History.Num() - MaxHistoryTurns);
	}
	SaveMemoryToDisk(Ctx.NpcId, History);

	ApplyStatDelta(NpcActor, ReplyMsg.RelationshipDelta, ReplyMsg.AttractionDelta);
	ExecuteNpcAction(NpcActor, PlayerActor, ReplyMsg.Action);
	OnRoleplayReplyReceived.Broadcast(Ctx.NpcId, true, ReplyMsg);
	return ReplyMsg;
}

FLifePlayChatMessage ULifePlayAIRoleplaySubsystem::TriggerPresetInteraction(
	AActor* NpcActor,
	AActor* PlayerActor,
	const FString& InteractionId)
{
	const FString Id = InteractionId.TrimStartAndEnd().ToLower();
	if (Id == TEXT("greet"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Hey! How's your day going?"));
	}
	if (Id == TEXT("ask_life"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Tell me about your life, work, and how you're feeling today."));
	}
	if (Id == TEXT("flirt"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("You look amazing today — want to hang out later?"));
	}
	if (Id == TEXT("follow"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Come follow me and walk around the city together."));
	}
	if (Id == TEXT("stay"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Wait here for a bit, see you later."));
	}
	if (Id == TEXT("provoke"))
	{
		return SendRoleplayMessage(NpcActor, PlayerActor, TEXT("Watch where you're going — back off!"));
	}
	return SendRoleplayMessage(NpcActor, PlayerActor, InteractionId);
}
