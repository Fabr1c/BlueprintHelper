#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Shared/Review/BlueprintHelperReviewBoundaryModel.h"
#include "Shared/Review/BlueprintHelperReviewLifecycleLinkPolicy.h"
#include "Shared/Review/BlueprintHelperReviewTargetMatcher.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBoundaryModelAtomicTargetTest,
	"BlueprintHelper.Review.BoundaryModel.AtomicTargetShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBoundaryModelAtomicTargetTest::RunTest(const FString&)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = TEXT("/Game/BP_Door.BP_Door");
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("graph:node:OpenDoor");
	Target.TargetKind = TEXT("graph_node");
	Target.TargetSubKind = TEXT("call_function");
	Target.ScopeIdentity = TEXT("/Game/BP_Door.BP_Door|EventGraph|graph:node:OpenDoor");
	Target.LifecycleObjectKey = TEXT("node:OpenDoor");
	Target.LifecycleParentKey = TEXT("asset:asset");
	Target.VisualGroupKey = TEXT("graph:OpenDoor");

	const FBlueprintHelperReviewBoundaryModel Boundary =
		FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(Target);

	TestEqual(TEXT("asset key maps from asset path"), Boundary.AssetKey, Target.AssetPath);
	TestEqual(TEXT("location key maps from graph name"), Boundary.LocationKey, Target.GraphName);
	TestEqual(TEXT("target key is preserved"), Boundary.TargetKey, Target.TargetKey);
	TestEqual(TEXT("target kind is preserved"), Boundary.TargetKind, Target.TargetKind);
	TestEqual(TEXT("target sub-kind is preserved"), Boundary.TargetSubKind, Target.TargetSubKind);
	TestEqual(TEXT("scope identity is preserved"), Boundary.ScopeIdentity, Target.ScopeIdentity);
	TestEqual(TEXT("lifecycle object key is preserved"), Boundary.LifecycleObjectKey, Target.LifecycleObjectKey);
	TestEqual(TEXT("lifecycle parent key is preserved"), Boundary.LifecycleParentKey, Target.LifecycleParentKey);
	TestEqual(TEXT("visual group key is preserved"), Boundary.VisualGroupKey, Target.VisualGroupKey);
	TestFalse(TEXT("child target is not asset lifecycle root"), Boundary.bIsAssetLifecycleRoot);
	TestFalse(TEXT("child target is not object lifecycle root when it has a parent"), Boundary.bIsObjectLifecycleRoot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBoundaryModelVisibleChangeTest,
	"BlueprintHelper.Review.BoundaryModel.VisibleChangeShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBoundaryModelVisibleChangeTest::RunTest(const FString&)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = TEXT("/Game/BP_Door.BP_Door");
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("asset:asset");
	Target.TargetKind = TEXT("asset");
	Target.ScopeIdentity = TEXT("/Game/BP_Door.BP_Door|asset");
	Target.LifecycleObjectKey = TEXT("asset:asset");

	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = Target.AssetPath;
	Change.GraphName = Target.GraphName;
	Change.LocationKey = TEXT("/Game/BP_Door.BP_Door|EventGraph");
	Change.ScopeIdentity = TEXT("change-scope");
	Change.ParentChangeId = TEXT("parent-change-id");
	Change.AtomicTargets.Add(Target);
	Change.bIsAssetLifecycleRoot = true;
	Change.bRejectRemovesChildren = true;

	const FBlueprintHelperReviewBoundaryModel Boundary =
		FBlueprintHelperReviewBoundaryModelBuilder::FromVisibleChange(Change);

	TestEqual(TEXT("visible change location key is preserved"), Boundary.LocationKey, Change.LocationKey);
	TestEqual(TEXT("visible change target key comes from atomic target"), Boundary.TargetKey, Target.TargetKey);
	TestEqual(TEXT("visible change uses atomic scope before change fallback"), Boundary.ScopeIdentity, Target.ScopeIdentity);
	TestEqual(TEXT("ParentChangeId maps to lifecycle parent key when no target parent exists"), Boundary.LifecycleParentKey, Change.ParentChangeId);
	TestTrue(TEXT("asset lifecycle root flag is preserved"), Boundary.bIsAssetLifecycleRoot);
	TestTrue(TEXT("asset lifecycle root is also object lifecycle root"), Boundary.bIsObjectLifecycleRoot);
	TestTrue(TEXT("reject child cascade flag is preserved"), Boundary.bRejectRemovesChildren);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBoundaryModelTargetMatchingTest,
	"BlueprintHelper.Review.BoundaryModel.TargetMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBoundaryModelTargetMatchingTest::RunTest(const FString&)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = TEXT("/Game/BP_Door.BP_Door");
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("graph:node:OpenDoor");
	Target.TargetKind = TEXT("graph_node");
	Target.ScopeIdentity = TEXT("/Game/BP_Door.BP_Door|EventGraph|graph:node:OpenDoor");

	const FBlueprintHelperReviewBoundaryModel Boundary =
		FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(Target);

	TestTrue(
		TEXT("exact target key matches"),
		FBlueprintHelperReviewTargetMatcher::MatchesTargetKey(Boundary, TEXT("graph:node:OpenDoor")));
	TestTrue(
		TEXT("candidate target key list matches"),
		FBlueprintHelperReviewTargetMatcher::MatchesAnyTargetKey(
			Boundary,
			{TEXT("component:Door"), TEXT("graph:node:OpenDoor")}));
	TestFalse(
		TEXT("unrelated target key does not match"),
		FBlueprintHelperReviewTargetMatcher::MatchesTargetKey(Boundary, TEXT("graph:node:CloseDoor")));
	TestTrue(
		TEXT("scope identity matches"),
		FBlueprintHelperReviewTargetMatcher::MatchesScopeIdentity(
			Boundary,
			TEXT("/Game/BP_Door.BP_Door|EventGraph|graph:node:OpenDoor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBoundaryModelLifecycleLinkingTest,
	"BlueprintHelper.Review.BoundaryModel.LifecycleLinking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBoundaryModelLifecycleLinkingTest::RunTest(const FString&)
{
	FBlueprintHelperReviewBoundaryModel AssetRoot;
	AssetRoot.AssetKey = TEXT("/Game/BP_Door.BP_Door");
	AssetRoot.TargetKey = TEXT("asset:asset");
	AssetRoot.LifecycleObjectKey = TEXT("asset:asset");
	AssetRoot.bIsAssetLifecycleRoot = true;
	AssetRoot.bIsObjectLifecycleRoot = true;
	AssetRoot.bRejectRemovesChildren = true;

	FBlueprintHelperReviewBoundaryModel GraphNode;
	GraphNode.AssetKey = TEXT("/Game/BP_Door.BP_Door");
	GraphNode.LocationKey = TEXT("EventGraph");
	GraphNode.TargetKey = TEXT("graph:node:OpenDoor");
	GraphNode.LifecycleObjectKey = TEXT("node:OpenDoor");
	GraphNode.LifecycleParentKey = TEXT("asset:asset");

	FBlueprintHelperReviewBoundaryModel OtherAssetChild = GraphNode;
	OtherAssetChild.AssetKey = TEXT("/Game/BP_Window.BP_Window");

	TestTrue(
		TEXT("asset lifecycle root links same-asset child"),
		FBlueprintHelperReviewLifecycleLinkPolicy::CanLinkAsChild(AssetRoot, GraphNode));
	TestTrue(
		TEXT("asset lifecycle root can cascade when it removes children"),
		FBlueprintHelperReviewLifecycleLinkPolicy::CanCascadeReject(AssetRoot, GraphNode));
	TestTrue(
		TEXT("matcher reports child belongs to lifecycle root"),
		FBlueprintHelperReviewTargetMatcher::BelongsToLifecycleRoot(GraphNode, AssetRoot));
	TestFalse(
		TEXT("lifecycle policy rejects cross-asset child"),
		FBlueprintHelperReviewLifecycleLinkPolicy::CanLinkAsChild(AssetRoot, OtherAssetChild));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBoundaryModelRejectUsesEvidenceBeforeSnapshotTest,
	"BlueprintHelper.Review.BoundaryModel.RejectUsesEvidenceBeforeSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBoundaryModelRejectUsesEvidenceBeforeSnapshotTest::RunTest(const FString&)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = TEXT("/Game/BP_Door.BP_Door");
	Change.ChangeId = TEXT("change-door-open");
	Change.ParentChangeId = TEXT("asset-root-change");
	Change.BeforeSnapshotJson = TEXT("{\"value\":\"before\"}");
	Change.AfterSnapshotJson = TEXT("{\"value\":\"after\"}");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.TargetKey = TEXT("graph:node:OpenDoor");
	Target.TargetKind = TEXT("graph_node");
	Target.BeforeSnapshotJson = TEXT("{\"value\":\"evidence-before\"}");
	Target.AfterSnapshotJson = TEXT("{\"value\":\"evidence-after\"}");
	Change.AtomicTargets.Add(Target);

	const FBlueprintHelperReviewBoundaryModel Boundary =
		FBlueprintHelperReviewBoundaryModelBuilder::FromVisibleChange(Change);

	TestTrue(
		TEXT("reject target still matches evidence target"),
		FBlueprintHelperReviewTargetMatcher::MatchesTargetKey(Boundary, Target.TargetKey));
	TestEqual(
		TEXT("ParentChangeId is kept as lifecycle parent for reject cascade"),
		Boundary.LifecycleParentKey,
		Change.ParentChangeId);
	return true;
}

#endif
