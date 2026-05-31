#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperLogicReadSnapshotFormatterTestsLocalUtils
{
public:
	static TSharedPtr<FJsonObject> MakeExportedFunctionGraphWithoutEntryRawJsonObject()
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("version"), TEXT("2.2"));
		Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

		TArray<TSharedPtr<FJsonValue>> FunctionNodes;
		TSharedRef<FJsonObject> BodyNode = MakeShared<FJsonObject>();
		BodyNode->SetStringField(TEXT("id"), TEXT("set_relative_rotation"));
		BodyNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
		BodyNode->SetStringField(TEXT("name"), TEXT("SetRelativeRotation"));
		BodyNode->SetStringField(TEXT("function_name"), TEXT("SetRelativeRotation"));
		FunctionNodes.Add(MakeShared<FJsonValueObject>(BodyNode));

		TArray<TSharedPtr<FJsonValue>> FunctionLinks;
		TSharedRef<FJsonObject> EntryToBodyLink = MakeShared<FJsonObject>();
		EntryToBodyLink->SetStringField(TEXT("from_id"), TEXT("__function_entry__"));
		EntryToBodyLink->SetStringField(TEXT("from_pin"), TEXT("then"));
		EntryToBodyLink->SetStringField(TEXT("to_id"), TEXT("set_relative_rotation"));
		EntryToBodyLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
		EntryToBodyLink->SetStringField(TEXT("kind"), TEXT("exec"));
		FunctionLinks.Add(MakeShared<FJsonValueObject>(EntryToBodyLink));

		TSharedRef<FJsonObject> BodyToResultLink = MakeShared<FJsonObject>();
		BodyToResultLink->SetStringField(TEXT("from_id"), TEXT("set_relative_rotation"));
		BodyToResultLink->SetStringField(TEXT("from_pin"), TEXT("then"));
		BodyToResultLink->SetStringField(TEXT("to_id"), TEXT("__function_result__"));
		BodyToResultLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
		BodyToResultLink->SetStringField(TEXT("kind"), TEXT("exec"));
		FunctionLinks.Add(MakeShared<FJsonValueObject>(BodyToResultLink));

		TSharedRef<FJsonObject> FunctionGraph = MakeShared<FJsonObject>();
		FunctionGraph->SetStringField(TEXT("graph"), TEXT("AddMazeRelativeRotation"));
		FunctionGraph->SetArrayField(TEXT("nodes"), FunctionNodes);
		FunctionGraph->SetArrayField(TEXT("links"), FunctionLinks);

		TArray<TSharedPtr<FJsonValue>> Graphs;
		Graphs.Add(MakeShared<FJsonValueObject>(FunctionGraph));
		Root->SetArrayField(TEXT("graphs"), Graphs);
		return Root;
	}

	static FBlueprintHelperLogicReadSnapshot MakeFunctionTargetSnapshot()
	{
		FBlueprintHelperLogicReadSnapshot Snapshot;
		Snapshot.Target.AssetPath = TEXT("/Game/Gameplay/Maze/BP_Maze");
		Snapshot.Target.TargetType = EBlueprintHelperTargetType::Function;
		Snapshot.Target.Function = TEXT("AddMazeRelativeRotation");
		Snapshot.AssetPath = Snapshot.Target.AssetPath;
		Snapshot.Scope = EBlueprintHelperLogicScope::TargetFunction;
		Snapshot.TargetEntryName = TEXT("AddMazeRelativeRotation");
		Snapshot.bTargetEntryScope = true;
		Snapshot.bExportSucceeded = true;
		Snapshot.RawJsonObject = MakeExportedFunctionGraphWithoutEntryRawJsonObject();
		return Snapshot;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadSnapshotFormatter_FormatsPureDto,
	"BlueprintHelper.Read.LogicSnapshotFormatter.FormatsPureDto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperLogicReadSnapshotFormatter_FormatsPureDto::RunTest(const FString& Parameters)
{
	FBlueprintHelperLogicReadSnapshot Snapshot;
	Snapshot.AssetPath = TEXT("/Game/BlueprintHelperRead/BP_Test");
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Scope = EBlueprintHelperLogicScope::TargetGraph;
	Snapshot.bExportSucceeded = false;

	FBlueprintHelperLogicReadSnapshotFormatter Formatter;
	TSharedPtr<FJsonObject> JsonPayload;
	FString Error;
	TestTrue(TEXT("logic_json payload builds from DTO"),
		Formatter.BuildFormattedPayload(TEXT("logic_json"), Snapshot, JsonPayload, Error));
	TestTrue(TEXT("logic_json payload valid"), JsonPayload.IsValid());
	TestEqual(TEXT("logic_json schema"),
		JsonPayload.IsValid() ? JsonPayload->GetStringField(TEXT("schema")) : FString(),
		FString(TEXT("LogicJson.v1")));

	TSharedPtr<FJsonObject> MdPayload;
	TestTrue(TEXT("logic_md payload builds from DTO"),
		Formatter.BuildFormattedPayload(TEXT("logic_md"), Snapshot, MdPayload, Error));
	TestTrue(TEXT("logic_md payload valid"), MdPayload.IsValid());
	TestEqual(TEXT("logic_md schema"),
		MdPayload.IsValid() ? MdPayload->GetStringField(TEXT("schema")) : FString(),
		FString(TEXT("LogicMd.v1")));

	TSharedPtr<FJsonObject> UnsupportedPayload;
	TestFalse(TEXT("unknown format fails"),
		Formatter.BuildFormattedPayload(TEXT("logic_xml"), Snapshot, UnsupportedPayload, Error));
	TestTrue(TEXT("unknown format reports error"), Error.Contains(TEXT("Unsupported read format")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadSnapshotFormatter_FormatsFunctionTargetWithoutExportedEntry,
	"BlueprintHelper.Read.LogicSnapshotFormatter.FormatsFunctionTargetWithoutExportedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperLogicReadSnapshotFormatter_FormatsFunctionTargetWithoutExportedEntry::RunTest(
	const FString& Parameters)
{
	const FBlueprintHelperLogicReadSnapshot Snapshot =
		FBlueprintHelperLogicReadSnapshotFormatterTestsLocalUtils::MakeFunctionTargetSnapshot();

	FBlueprintHelperLogicReadSnapshotFormatter Formatter;
	TSharedPtr<FJsonObject> JsonPayload;
	FString Error;
	TestTrue(TEXT("logic_json function target payload builds"),
		Formatter.BuildFormattedPayload(TEXT("logic_json"), Snapshot, JsonPayload, Error));

	const TSharedPtr<FJsonObject>* LogicObj = nullptr;
	TestTrue(TEXT("logic_json contains logic object"),
		JsonPayload.IsValid() && JsonPayload->TryGetObjectField(TEXT("logic"), LogicObj) && LogicObj && LogicObj->IsValid());
	if (LogicObj && LogicObj->IsValid())
	{
		TestEqual(TEXT("logic_json records function"), (*LogicObj)->GetStringField(TEXT("function")),
			FString(TEXT("AddMazeRelativeRotation")));

		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		TestTrue(TEXT("logic_json exposes function boundary and body nodes"),
			(*LogicObj)->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes && Nodes->Num() == 3);
		if (Nodes && Nodes->Num() == 3)
		{
			const TSharedPtr<FJsonObject>* EntryNode = nullptr;
			const TSharedPtr<FJsonObject>* BodyNode = nullptr;
			const TSharedPtr<FJsonObject>* ResultNode = nullptr;
			TestTrue(TEXT("logic_json entry boundary node object"),
				(*Nodes)[0].IsValid() && (*Nodes)[0]->TryGetObject(EntryNode) && EntryNode && EntryNode->IsValid());
			TestTrue(TEXT("logic_json body node object"),
				(*Nodes)[1].IsValid() && (*Nodes)[1]->TryGetObject(BodyNode) && BodyNode && BodyNode->IsValid());
			TestTrue(TEXT("logic_json result boundary node object"),
				(*Nodes)[2].IsValid() && (*Nodes)[2]->TryGetObject(ResultNode) && ResultNode && ResultNode->IsValid());

			if (EntryNode && EntryNode->IsValid() && BodyNode && BodyNode->IsValid() && ResultNode && ResultNode->IsValid())
			{
				TestEqual(TEXT("logic_json entry boundary node ref"), (*EntryNode)->GetStringField(TEXT("node_ref")),
					FString(TEXT("__function_entry__")));
				TestEqual(TEXT("logic_json entry boundary kind"), (*EntryNode)->GetStringField(TEXT("kind")),
					FString(TEXT("function")));
				TestEqual(TEXT("logic_json body node ref"), (*BodyNode)->GetStringField(TEXT("node_ref")),
					FString(TEXT("nodes[0]")));
				TestEqual(TEXT("logic_json result boundary node ref"), (*ResultNode)->GetStringField(TEXT("node_ref")),
					FString(TEXT("__function_result__")));
				TestEqual(TEXT("logic_json result boundary kind"), (*ResultNode)->GetStringField(TEXT("kind")),
					FString(TEXT("return")));

				const TArray<TSharedPtr<FJsonValue>>* EntryLinks = nullptr;
				TestTrue(TEXT("logic_json entry boundary has exec link"),
					(*EntryNode)->TryGetArrayField(TEXT("links"), EntryLinks) && EntryLinks && EntryLinks->Num() == 1);
				if (EntryLinks && EntryLinks->Num() == 1)
				{
					const TSharedPtr<FJsonObject>* EntryLink = nullptr;
					TestTrue(TEXT("logic_json entry boundary link object"),
						(*EntryLinks)[0].IsValid()
						&& (*EntryLinks)[0]->TryGetObject(EntryLink)
						&& EntryLink
						&& EntryLink->IsValid());
					if (EntryLink && EntryLink->IsValid())
					{
						TestEqual(TEXT("logic_json entry link target"), (*EntryLink)->GetStringField(TEXT("to_node")),
							FString(TEXT("nodes[0]")));
					}
				}

				const TArray<TSharedPtr<FJsonValue>>* BodyLinks = nullptr;
				TestTrue(TEXT("logic_json body has result boundary exec link"),
					(*BodyNode)->TryGetArrayField(TEXT("links"), BodyLinks) && BodyLinks && BodyLinks->Num() == 1);
				if (BodyLinks && BodyLinks->Num() == 1)
				{
					const TSharedPtr<FJsonObject>* BodyLink = nullptr;
					TestTrue(TEXT("logic_json body link object"),
						(*BodyLinks)[0].IsValid()
						&& (*BodyLinks)[0]->TryGetObject(BodyLink)
						&& BodyLink
						&& BodyLink->IsValid());
					if (BodyLink && BodyLink->IsValid())
					{
						TestEqual(TEXT("logic_json body link target"), (*BodyLink)->GetStringField(TEXT("to_node")),
							FString(TEXT("__function_result__")));
					}
				}
			}
		}
	}

	TSharedPtr<FJsonObject> MdPayload;
	TestTrue(TEXT("logic_md function target payload builds"),
		Formatter.BuildFormattedPayload(TEXT("logic_md"), Snapshot, MdPayload, Error));
	const FString Markdown = MdPayload.IsValid() ? MdPayload->GetStringField(TEXT("markdown")) : TEXT("");
	TestTrue(TEXT("logic_md includes function entry boundary link"),
		Markdown.Contains(TEXT("__function_entry__.then -> nodes[0].execute")));
	TestTrue(TEXT("logic_md includes function result boundary link"),
		Markdown.Contains(TEXT("nodes[0].then -> __function_result__.execute")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperLogicReadRequestSnapshotCache_IsRequestLocal,
	"BlueprintHelper.Read.LogicSnapshotCache.IsRequestLocal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperLogicReadRequestSnapshotCache_IsRequestLocal::RunTest(const FString& Parameters)
{
	FBlueprintHelperLogicReadSnapshotCacheKey Key;
	Key.AssetPath = TEXT("/Game/BlueprintHelperRead/BP_Test");
	Key.GraphName = TEXT("EventGraph");
	Key.Scope = TEXT("target_graph");
	Key.ReadDetail = TEXT("default");
	Key.SchemaVersion = TEXT("LogicReadSnapshot.v1");

	FBlueprintHelperLogicReadSnapshot Snapshot;
	Snapshot.AssetPath = Key.AssetPath;
	Snapshot.GraphName = Key.GraphName;

	FBlueprintHelperLogicReadRequestSnapshotCache Cache;
	FBlueprintHelperLogicReadSnapshot Found;
	TestFalse(TEXT("first lookup misses"), Cache.TryGet(Key, Found));
	Cache.Put(Key, Snapshot);
	TestTrue(TEXT("second lookup hits"), Cache.TryGet(Key, Found));
	TestEqual(TEXT("hit count"), Cache.GetHitCount(), 1);
	TestEqual(TEXT("miss count"), Cache.GetMissCount(), 1);
	Cache.Reset();
	TestFalse(TEXT("reset clears snapshot"), Cache.TryGet(Key, Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadCachePolicy_SeparatesPureDataFromUObjectState,
	"BlueprintHelper.Read.CachePolicy.SeparatesPureDataFromUObjectState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReadCachePolicy_SeparatesPureDataFromUObjectState::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("capability matrix can persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::ReadCapabilityMatrix));
	TestFalse(TEXT("asset snapshot cannot persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::AssetGraphSnapshot));
	TestTrue(TEXT("asset snapshot is request local only"),
		FBlueprintHelperReadCachePolicy::IsRequestLocalOnly(
			EBlueprintHelperReadCacheDataKind::AssetGraphSnapshot));
	TestFalse(TEXT("UObject pointer cannot persist"),
		FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
			EBlueprintHelperReadCacheDataKind::UObjectPointer));
	return true;
}

#endif
