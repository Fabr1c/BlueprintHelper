#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeContextRevisionManifest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Misc/Crc.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionService.h"

class FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils
{
public:
	static FString ReadString(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
	{
		FString Value;
		return Json.IsValid() && Json->TryGetStringField(Field, Value) ? Value : FString();
	}

	static FString ReadNestedString(const TSharedPtr<FJsonObject>& Json, const TCHAR* ObjectField, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Json.IsValid() || !Json->TryGetObjectField(ObjectField, Object) || !Object || !Object->IsValid())
		{
			return FString();
		}
		return ReadString(*Object, Field);
	}

	static int32 ReadInt(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
	{
		double Value = 0.0;
		return Json.IsValid() && Json->TryGetNumberField(Field, Value) ? static_cast<int32>(Value) : 0;
	}

	static bool ReadBool(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
	{
		bool Value = false;
		return Json.IsValid() && Json->TryGetBoolField(Field, Value) ? Value : false;
	}
};

FString FBlueprintHelperTaskRuntimeContextRevisionEntry::StableKey() const
{
	return FString::Printf(TEXT("%s|%s"), *AssetPath, *GraphName);
}

FString FBlueprintHelperTaskRuntimeContextRevisionEntry::ToStableString() const
{
	return FString::Printf(
		TEXT("%s|graph_exists=%d|bp=%d|graph=%d"),
		*StableKey(),
		bGraphExists ? 1 : 0,
		BlueprintRevision,
		GraphRevision);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeContextRevisionEntry::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("asset_path"), AssetPath);
	Json->SetStringField(TEXT("graph_name"), GraphName);
	Json->SetBoolField(TEXT("graph_exists"), bGraphExists);
	Json->SetNumberField(TEXT("blueprint_revision"), BlueprintRevision);
	Json->SetNumberField(TEXT("graph_revision"), GraphRevision);
	return Json;
}

void FBlueprintHelperTaskRuntimeContextRevisionManifest::RecomputeHash()
{
	Entries.Sort([](
		const FBlueprintHelperTaskRuntimeContextRevisionEntry& Left,
		const FBlueprintHelperTaskRuntimeContextRevisionEntry& Right)
	{
		return Left.StableKey() < Right.StableKey();
	});

	TArray<FString> Parts;
	for (const FBlueprintHelperTaskRuntimeContextRevisionEntry& Entry : Entries)
	{
		Parts.Add(Entry.ToStableString());
	}
	ManifestHash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*FString::Join(Parts, TEXT("\n"))));
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeContextRevisionManifest::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("manifest_hash"), ManifestHash);

	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FBlueprintHelperTaskRuntimeContextRevisionEntry& Entry : Entries)
	{
		Values.Add(MakeShared<FJsonValueObject>(Entry.ToJson()));
	}
	Json->SetArrayField(TEXT("entries"), Values);
	return Json;
}

bool FBlueprintHelperTaskRuntimeContextRevisionManifest::Compare(
	const FBlueprintHelperTaskRuntimeContextRevisionManifest& Expected,
	const FBlueprintHelperTaskRuntimeContextRevisionManifest& Current,
	FBlueprintHelperTaskRuntimeContextRevisionMismatch& OutMismatch)
{
	if (Expected.ManifestHash == Current.ManifestHash)
	{
		return true;
	}

	OutMismatch.Code = TEXT("context_stale");
	OutMismatch.DetailCode = TEXT("action_context_stale");
	OutMismatch.Message = TEXT("Target Blueprint or graph structure changed after preview; run preview_task again.");
	OutMismatch.Field = TEXT("preview_token.context_revision");
	OutMismatch.Expected = Expected.ToJson();
	OutMismatch.Current = Current.ToJson();
	return false;
}

FBlueprintHelperTaskRuntimeContextRevisionManifest FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromTaskPlan(
	const TSharedPtr<FJsonObject>& TaskPlan)
{
	FBlueprintHelperTaskRuntimeContextRevisionManifest Manifest;
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!TaskPlan.IsValid() || !TaskPlan->TryGetArrayField(TEXT("steps"), Steps) || !Steps)
	{
		Manifest.RecomputeHash();
		return Manifest;
	}

	TSet<FString> SeenKeys;
	for (const TSharedPtr<FJsonValue>& StepValue : *Steps)
	{
		const TSharedPtr<FJsonObject> Step = StepValue.IsValid() ? StepValue->AsObject() : nullptr;
		if (!Step.IsValid())
		{
			continue;
		}

		const FString Capability = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadString(Step, TEXT("capability"));
		if (!Capability.Equals(TEXT("graph_write"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString AssetPath = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadNestedString(Step, TEXT("target"), TEXT("asset_path"));
		const FString GraphName = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadNestedString(Step, TEXT("target"), TEXT("graph"));
		if (AssetPath.IsEmpty())
		{
			continue;
		}

		const FString Key = AssetPath + TEXT("|") + GraphName;
		if (SeenKeys.Contains(Key))
		{
			continue;
		}
		SeenKeys.Add(Key);

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
		UEdGraph* Graph = Blueprint && !GraphName.IsEmpty()
			? FBlueprintGraphWriteFacade::FindGraphByName(Blueprint, GraphName)
			: nullptr;

		FBlueprintHelperTaskRuntimeContextRevisionEntry Entry;
		Entry.AssetPath = AssetPath;
		Entry.GraphName = GraphName;
		Entry.bGraphExists = Graph != nullptr;
		Entry.BlueprintRevision = FBlueprintHelperActionContextRevisionService::BuildBlueprintRevision(Blueprint);
		Entry.GraphRevision = FBlueprintHelperActionContextRevisionService::BuildGraphRevision(Graph);
		Manifest.Entries.Add(Entry);
	}

	Manifest.RecomputeHash();
	return Manifest;
}

FBlueprintHelperTaskRuntimeContextRevisionManifest FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromJson(
	const TSharedPtr<FJsonObject>& Json)
{
	FBlueprintHelperTaskRuntimeContextRevisionManifest Manifest;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
	{
		Manifest.RecomputeHash();
		return Manifest;
	}

	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> EntryJson = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
		if (!EntryJson.IsValid())
		{
			continue;
		}

		FBlueprintHelperTaskRuntimeContextRevisionEntry Entry;
		Entry.AssetPath = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadString(EntryJson, TEXT("asset_path"));
		Entry.GraphName = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadString(EntryJson, TEXT("graph_name"));
		Entry.bGraphExists = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadBool(EntryJson, TEXT("graph_exists"));
		Entry.BlueprintRevision = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadInt(EntryJson, TEXT("blueprint_revision"));
		Entry.GraphRevision = FBlueprintHelperTaskRuntimeContextRevisionManifestLocalUtils::ReadInt(EntryJson, TEXT("graph_revision"));
		Manifest.Entries.Add(Entry);
	}

	Manifest.RecomputeHash();
	return Manifest;
}
