#include "Generated/BlueprintHelperReadContextRouteManifest.generated.h"

#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReadContextRouteManifestTestUtils
{
public:
	static bool IsEmpty(const TCHAR* Value)
	{
		return Value == nullptr || FCString::Strlen(Value) == 0;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadContextGeneratedRouteMirrorTest,
	"BlueprintHelper.ReadContext.RouteManifest.GeneratedMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReadContextGeneratedRouteMirrorTest::RunTest(const FString&)
{
	TestTrue(TEXT("generated ReadContext route manifest has rows"), GBlueprintHelperReadContextRouteCount > 0);

	TSet<FString> TemplateIds;
	int32 ActiveRoutes = 0;
	const FString RemovedMarkdownView = FString(TEXT("logic")) + TEXT("_md");
	const FString RemovedMarkdownCommand = FString(TEXT("read_blueprint_logic")) + TEXT("_md");
	const FString RemovedMaterialMarkdownCommand = FString(TEXT("read_material_logic")) + TEXT("_md");
	for (const FBlueprintHelperGeneratedReadContextRouteDescriptor& Route : GBlueprintHelperReadContextRoutes)
	{
		TestFalse(TEXT("template id is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.TemplateId));
		TestFalse(TEXT("family is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Family));
		TestFalse(TEXT("read cluster is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.ReadCluster));
		TestFalse(TEXT("status is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Status));

		const FString TemplateId(Route.TemplateId);
		TestFalse(FString::Printf(TEXT("template id is unique: %s"), *TemplateId), TemplateIds.Contains(TemplateId));
		TestFalse(FString::Printf(TEXT("template id excludes removed markdown view: %s"), *TemplateId),
			TemplateId.Contains(RemovedMarkdownView));
		TemplateIds.Add(TemplateId);

		TestFalse(TEXT("format excludes removed markdown view"),
			FString(Route.Format).Equals(RemovedMarkdownView));
		TestFalse(TEXT("supported command excludes material removed markdown bridge command"),
			FString(Route.Command).Equals(RemovedMaterialMarkdownCommand));
		TestFalse(TEXT("command excludes removed markdown bridge command"),
			FString(Route.Command).Equals(RemovedMarkdownCommand));

		const FString Status(Route.Status);
		if (Status == TEXT("active"))
		{
			++ActiveRoutes;
			TestFalse(
				FString::Printf(TEXT("active route has command: %s"), *TemplateId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Command));
			TestFalse(
				FString::Printf(TEXT("active route has cluster: %s"), *TemplateId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Cluster));
			TestFalse(
				FString::Printf(TEXT("active route has target type: %s"), *TemplateId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.TargetType));
			TestFalse(
				FString::Printf(TEXT("active route has format: %s"), *TemplateId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Format));
		}
	}

	TestTrue(TEXT("generated ReadContext route mirror exposes active routes"), ActiveRoutes > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadContextMaterialRouteMirrorTest,
	"BlueprintHelper.ReadContext.RouteManifest.MaterialRoutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReadContextMaterialRouteMirrorTest::RunTest(const FString&)
{
	TMap<FString, FString> ExpectedCommands;
	ExpectedCommands.Add(TEXT("material.logic.graph.json"), TEXT("read_material_logic_json"));
	ExpectedCommands.Add(TEXT("material.logic.graph.flow"), TEXT("read_material_logic_json"));

	TSet<FString> SeenTemplates;
	for (const FBlueprintHelperGeneratedReadContextRouteDescriptor& Route : GBlueprintHelperReadContextRoutes)
	{
		const FString TemplateId(Route.TemplateId);
		const FString* ExpectedCommand = ExpectedCommands.Find(TemplateId);
		if (!ExpectedCommand)
		{
			continue;
		}

		SeenTemplates.Add(TemplateId);
		TestFalse(TEXT("material route excludes removed markdown view"), TemplateId.Contains(FString(TEXT("logic")) + TEXT("_md")));
		TestEqual(FString::Printf(TEXT("material route is active: %s"), *TemplateId), FString(Route.Status), FString(TEXT("active")));
		TestEqual(FString::Printf(TEXT("material route command matches runtime: %s"), *TemplateId), FString(Route.Command), *ExpectedCommand);
		TestEqual(FString::Printf(TEXT("material route cluster matches runtime: %s"), *TemplateId), FString(Route.Cluster), FString(TEXT("SharedServices")));
		TestEqual(FString::Printf(TEXT("material route family: %s"), *TemplateId), FString(Route.Family), FString(TEXT("material")));
	}

	for (const TPair<FString, FString>& Expected : ExpectedCommands)
	{
		TestTrue(FString::Printf(TEXT("material generated route exists: %s"), *Expected.Key), SeenTemplates.Contains(Expected.Key));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadContextAdapterBoundaryBodyEvidenceTest,
	"BlueprintHelper.ReadContext.AdapterBoundary.BodyEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReadContextAdapterBoundaryBodyEvidenceTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = TEXT("k2.external_graph.replace_body");
	Boundary.TaskSpecStrategy = TEXT("replace_external_body");
	Boundary.TargetAssetPath = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter");
	Boundary.GraphName = TEXT("EventGraph");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	Boundary.EntryNodeRefs.Add(TEXT("BodyEntry"));

	FBlueprintHelperGraphBodyReadbackProjection Projection;
	Projection.BodyEntryNodeGuid = TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	Projection.BodyEntryNodeClass = TEXT("/Script/BlueprintGraph.K2Node_Event");
	Projection.BodyEntryFingerprint = TEXT("body_entry_fp");
	Projection.BodyFingerprint = TEXT("body_fp_before");

	const FBlueprintHelperGraphBodyReadbackService ReadbackService;
	const TSharedRef<FJsonObject> Json = ReadbackService.BuildAdapterBoundaryJson(Boundary, Projection);

	TestEqual(
		TEXT("runtime adapter"),
		Json->GetStringField(TEXT("runtime_adapter_id")),
		FString(TEXT("k2.external_graph.replace_body")));
	TestEqual(TEXT("graph name"), Json->GetStringField(TEXT("graph_name")), FString(TEXT("EventGraph")));
	TestEqual(TEXT("body fingerprint"), Json->GetStringField(TEXT("body_fingerprint")), FString(TEXT("body_fp_before")));

	const TSharedPtr<FJsonObject>* BodyEntry = nullptr;
	TestTrue(TEXT("body entry object exists"), Json->TryGetObjectField(TEXT("body_entry"), BodyEntry) && BodyEntry && BodyEntry->IsValid());
	if (BodyEntry && BodyEntry->IsValid())
	{
		TestEqual(
			TEXT("body entry node guid"),
			(*BodyEntry)->GetStringField(TEXT("node_guid")),
			FString(TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")));
		TestEqual(
			TEXT("body entry node class"),
			(*BodyEntry)->GetStringField(TEXT("node_class")),
			FString(TEXT("/Script/BlueprintGraph.K2Node_Event")));
		TestEqual(
			TEXT("body entry semantic role"),
			(*BodyEntry)->GetStringField(TEXT("semantic_role")),
			FString(TEXT("body_entry")));
		TestEqual(
			TEXT("body entry fingerprint"),
			(*BodyEntry)->GetStringField(TEXT("fingerprint")),
			FString(TEXT("body_entry_fp")));
	}

	return true;
}

#endif
