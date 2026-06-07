#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperWriteReviewEvidenceProjection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWriteFamilyReviewEvidenceUsesBoundaryModelTest,
	"BlueprintHelper.TaskRuntime.WriteFamilyReviewEvidence.UsesBoundaryModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWriteFamilyReviewEvidenceUsesBoundaryModelTest::RunTest(const FString&)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = TEXT("/Game/BH_Tests/BP_WriteFamilyReview");
	Target.TargetKey = TEXT("graph:EventGraph:block:BH_Block");
	Target.TargetKind = TEXT("graph_block");
	Target.TargetSubKind = TEXT("k2.event_body");
	Target.ScopeIdentity = TEXT("/Game/BH_Tests/BP_WriteFamilyReview|EventGraph|graph:EventGraph:block:BH_Block");
	Target.LifecycleObjectKey = Target.TargetKey;
	Target.VisualGroupKey = TEXT("graph_body|EventGraph");

	const FBlueprintHelperReviewBoundaryModel Boundary =
		FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(Target);
	TestEqual(TEXT("target key preserved"), Boundary.TargetKey, Target.TargetKey);
	TestEqual(TEXT("scope identity preserved"), Boundary.ScopeIdentity, Target.ScopeIdentity);
	TestEqual(TEXT("lifecycle key preserved"), Boundary.LifecycleObjectKey, Target.LifecycleObjectKey);

	FBlueprintHelperAcceptedPayloadModel AcceptedPayload;
	AcceptedPayload.TargetAssetPath = Target.AssetPath;
	AcceptedPayload.GraphName = TEXT("EventGraph");
	AcceptedPayload.WriteFamily = TEXT("graphwrite");

	FBlueprintHelperDiagnosticProjection Projection;
	Projection.Source = TEXT("test");
	Projection.Code = TEXT("compile_error");
	Projection.Message = TEXT("Compile failed.");
	Projection.Severity = TEXT("error");
	Projection.TargetKey = Target.TargetKey;
	Projection.ScopeIdentity = Target.ScopeIdentity;

	FBlueprintHelperReviewAtomicTarget ProjectedTarget =
		FBlueprintHelperWriteReviewEvidenceProjection::BuildAtomicTarget(
			AcceptedPayload,
			Boundary,
			Projection);
	TestEqual(TEXT("projected target key"), ProjectedTarget.TargetKey, Target.TargetKey);
	TestEqual(TEXT("projected scope identity"), ProjectedTarget.ScopeIdentity, Target.ScopeIdentity);
	TestEqual(TEXT("projected diagnostics"), ProjectedTarget.Diagnostics.Num(), 1);

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.AtomicTargets.Add(Target);
	FBlueprintHelperWriteReviewEvidenceProjection::AttachDiagnostics(Evidence, {Projection});
	TestEqual(TEXT("evidence diagnostic count"), Evidence.Diagnostics.Num(), 1);
	TestEqual(TEXT("target diagnostic count"), Evidence.AtomicTargets[0].Diagnostics.Num(), 1);
	return true;
}

#endif
