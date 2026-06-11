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

	TSet<FString> RouteIds;
	int32 ActiveRoutes = 0;
	for (const FBlueprintHelperGeneratedReadContextRouteDescriptor& Route : GBlueprintHelperReadContextRoutes)
	{
		TestFalse(TEXT("route id is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.RouteId));
		TestFalse(TEXT("domain is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Domain));
		TestFalse(TEXT("read cluster is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.ReadCluster));
		TestFalse(TEXT("target kind is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.TargetKind));
		TestFalse(TEXT("view template is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.ViewTemplate));
		TestFalse(TEXT("status is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Status));

		const FString RouteId(Route.RouteId);
		TestFalse(FString::Printf(TEXT("route id is unique: %s"), *RouteId), RouteIds.Contains(RouteId));
		RouteIds.Add(RouteId);

		const FString Status(Route.Status);
		if (Status == TEXT("active"))
		{
			++ActiveRoutes;
			TestFalse(
				FString::Printf(TEXT("active route has command: %s"), *RouteId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Command));
			TestFalse(
				FString::Printf(TEXT("active route has cluster: %s"), *RouteId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Cluster));
		}
	}

	TestTrue(TEXT("generated ReadContext route mirror exposes active routes"), ActiveRoutes > 0);
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
