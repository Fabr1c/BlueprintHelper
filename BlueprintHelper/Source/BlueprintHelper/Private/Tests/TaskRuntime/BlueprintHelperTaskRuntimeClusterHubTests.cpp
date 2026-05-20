#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/AssetFactory/BlueprintHelperAssetFactoryTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/DataTable/BlueprintHelperDataTableTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils
{
public:
static FBlueprintHelperTaskRuntimeLoweredStep MakeLoweredStep(const FString& Capability, const FString& AdapterOperation)
{
	FBlueprintHelperTaskRuntimeLoweredStep Step;
	Step.StepId = TEXT("step-1");
	Step.Capability = Capability;
	Step.AdapterOperation = AdapterOperation;
	return Step;
}

static FBlueprintHelperToolResultBase MakeClusterEvidenceAppliedResult(const FString& Operation)
{
	return FBlueprintHelperToolResultBuilder::Applied(Operation, TEXT("trace_cluster_evidence"));
}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeClusterHub_ResolvesLoweredSteps,
	"BlueprintHelper.TaskRuntime.Cluster.ResolvesLoweredSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeClusterHub_ResolvesLoweredSteps::RunTest(const FString& Parameters)
{
	const TPair<FBlueprintHelperTaskRuntimeLoweredStep, EBlueprintHelperTaskRuntimeCluster> Cases[] = {
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("append_blueprint_graph")), EBlueprintHelperTaskRuntimeCluster::GraphWrite},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_variable"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::BlueprintVariables},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("asset_factory"), TEXT("asset_factory.create_asset")), EBlueprintHelperTaskRuntimeCluster::AssetFactory},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_component"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::Component},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::ClassSettings},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_signature"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::Signature},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("umg_widget"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::UMGWidget},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("data_table"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::DataTable},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("object_property"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::ObjectProperty},
	};

	for (const TPair<FBlueprintHelperTaskRuntimeLoweredStep, EBlueprintHelperTaskRuntimeCluster>& Case : Cases)
	{
		const EBlueprintHelperTaskRuntimeCluster Cluster = FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(Case.Key);
		TestTrue(FString::Printf(TEXT("%s/%s maps to expected cluster"), *Case.Key.Capability, *Case.Key.AdapterOperation), Cluster == Case.Value);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeClusterHub_UnknownLoweredStepIsUnknown,
	"BlueprintHelper.TaskRuntime.Cluster.UnknownLoweredStepIsUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeClusterHub_UnknownLoweredStepIsUnknown::RunTest(const FString& Parameters)
{
	const FBlueprintHelperTaskRuntimeLoweredStep Step = FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("unknown_capability"), TEXT("unknown_operation"));
	const EBlueprintHelperTaskRuntimeCluster Cluster = FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(Step);
	TestTrue(TEXT("Unknown lowered step maps to Unknown cluster"), Cluster == EBlueprintHelperTaskRuntimeCluster::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteTaskRuntimeCluster_RecognizesOnlyGraphWriteSteps,
	"BlueprintHelper.TaskRuntime.Cluster.GraphWriteClusterRecognizesOnlyGraphWriteSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphWriteTaskRuntimeCluster_RecognizesOnlyGraphWriteSteps::RunTest(const FString& Parameters)
{
	const FString GraphWriteOperations[] = {
		TEXT("append_blueprint_graph"),
		TEXT("replace_blueprint_graph"),
		TEXT("patch_blueprint_graph"),
		TEXT("merge_blueprint_graph"),
	};

	for (const FString& AdapterOperation : GraphWriteOperations)
	{
		TestTrue(
			FString::Printf(TEXT("%s is a GraphWrite lowered step"), *AdapterOperation),
			FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), AdapterOperation)));
	}

	TestTrue(
		TEXT("graph_write capability resolves as GraphWrite"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	TestFalse(
		TEXT("unknown lowered step is not GraphWrite"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("unknown_capability"), TEXT(""))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSecondBatchTaskRuntimeClusters_RecognizeOnlyOwnedSteps,
	"BlueprintHelper.TaskRuntime.Cluster.SecondBatchClustersRecognizeOnlyOwnedSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperSecondBatchTaskRuntimeClusters_RecognizeOnlyOwnedSteps::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("BlueprintVariables cluster recognizes variable capability"),
		FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_variable"), TEXT(""))));
	TestFalse(
		TEXT("BlueprintVariables cluster rejects component capability"),
		FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_component"), TEXT(""))));

	TestTrue(
		TEXT("Component cluster recognizes component capability"),
		FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_component"), TEXT(""))));
	TestFalse(
		TEXT("Component cluster rejects class settings capability"),
		FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT(""))));

	TestTrue(
		TEXT("ClassSettings cluster recognizes class settings capability"),
		FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT(""))));
	TestFalse(
		TEXT("ClassSettings cluster rejects signature capability"),
		FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_signature"), TEXT(""))));

	TestTrue(
		TEXT("Signature cluster recognizes signature capability"),
		FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_signature"), TEXT(""))));
	TestFalse(
		TEXT("Signature cluster rejects graph write capability"),
		FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFinalBatchTaskRuntimeClusters_RecognizeOnlyOwnedSteps,
	"BlueprintHelper.TaskRuntime.Cluster.FinalBatchClustersRecognizeOnlyOwnedSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperFinalBatchTaskRuntimeClusters_RecognizeOnlyOwnedSteps::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("UMGWidget cluster recognizes widget capability"),
		FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("umg_widget"), TEXT(""))));
	TestFalse(
		TEXT("UMGWidget cluster rejects datatable capability"),
		FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("data_table"), TEXT(""))));

	TestTrue(
		TEXT("DataTable cluster recognizes datatable capability"),
		FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("data_table"), TEXT(""))));
	TestFalse(
		TEXT("DataTable cluster rejects object property capability"),
		FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("object_property"), TEXT(""))));

	TestTrue(
		TEXT("ObjectProperty cluster recognizes object property capability"),
		FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("object_property"), TEXT(""))));
	TestFalse(
		TEXT("ObjectProperty cluster rejects graph write capability"),
		FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep(FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRetiredEmptyTaskRuntimeClustersAreUnknown,
	"BlueprintHelper.TaskRuntime.Cluster.RetiredEmptyClustersAreUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperRetiredEmptyTaskRuntimeClustersAreUnknown::RunTest(const FString& Parameters)
{
	const FBlueprintHelperTaskRuntimeLoweredStep AnimationCapability =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("animation_blueprint"), TEXT(""));
	TestEqual(TEXT("animation_blueprint capability resolves Unknown"),
		FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(AnimationCapability),
		EBlueprintHelperTaskRuntimeCluster::Unknown);

	const FBlueprintHelperTaskRuntimeLoweredStep AnimationOperation =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("animation_blueprint"));
	TestEqual(TEXT("animation_blueprint operation resolves Unknown"),
		FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(AnimationOperation),
		EBlueprintHelperTaskRuntimeCluster::Unknown);

	const FBlueprintHelperTaskRuntimeLoweredStep MaterialCapability =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("material"), TEXT(""));
	TestEqual(TEXT("material capability resolves Unknown"),
		FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(MaterialCapability),
		EBlueprintHelperTaskRuntimeCluster::Unknown);

	const FBlueprintHelperTaskRuntimeLoweredStep MaterialOperation =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("material"));
	TestEqual(TEXT("material operation resolves Unknown"),
		FBlueprintHelperTaskRuntimeClusterHub::ResolveClusterForLoweredStep(MaterialOperation),
		EBlueprintHelperTaskRuntimeCluster::Unknown);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeCluster_BuildsProducerOwnedReviewEvidence,
	"BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeCluster_BuildsProducerOwnedReviewEvidence::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Door"));
	Payload->SetStringField(TEXT("component_name"), TEXT("DoorMesh"));

	FBlueprintHelperTaskRuntimeLoweredStep ComponentStep = FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("blueprint_component"), TEXT("blueprint_component.add_component"));
	ComponentStep.Payload = Payload;

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence(
		ComponentStep,
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceAppliedResult(ComponentStep.AdapterOperation),
		TEXT("archive_cluster_evidence"),
		TEXT("task_cluster_evidence"),
		2,
		Evidence);

	TestTrue(TEXT("component cluster owns Review evidence production"), bBuilt);
	TestEqual(TEXT("archive session id is required"),
		Evidence.ArchiveSessionId,
		FString(TEXT("archive_cluster_evidence")));
	TestEqual(TEXT("task run id is required"),
		Evidence.TaskRunId,
		FString(TEXT("task_cluster_evidence")));
	TestEqual(TEXT("producer evidence id is required"),
		Evidence.EvidenceId,
		FString(TEXT("task_step_task_cluster_evidence_2")));
	TestEqual(TEXT("asset path is required"),
		Evidence.AssetPath,
		FString(TEXT("/Game/BP_Door")));
	TestEqual(TEXT("one target is emitted"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind is required"),
		Target.TargetKind,
		FString(TEXT("component")));
	TestFalse(TEXT("target anchor is required"), Target.TargetKey.IsEmpty());
	TestFalse(TEXT("visual group key is required"), Target.VisualGroupKey.IsEmpty());
	TestFalse(TEXT("baseline hash is required"), Target.BaselineHash.IsEmpty());
	TestFalse(TEXT("recorded-after hash is required"), Target.RecordedAfterHash.IsEmpty());

	TSharedRef<FJsonObject> AssetPayload = MakeShared<FJsonObject>();
	AssetPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/Data/DA_Door"));
	AssetPayload->SetStringField(TEXT("asset_type"), TEXT("DataAsset"));

	FBlueprintHelperTaskRuntimeLoweredStep AssetFactoryStep = FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("asset_factory"), TEXT("create_asset"));
	AssetFactoryStep.Payload = AssetPayload;

	FBlueprintHelperWriteReviewEvidence AssetEvidence;
	TestTrue(TEXT("asset factory cluster owns Review evidence production"),
		FBlueprintHelperAssetFactoryTaskRuntimeCluster::BuildReviewEvidence(
			AssetFactoryStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceAppliedResult(AssetFactoryStep.AdapterOperation),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			4,
			AssetEvidence));
	TestEqual(TEXT("asset factory target kind is required"),
		AssetEvidence.AtomicTargets.Num() == 1 ? AssetEvidence.AtomicTargets[0].TargetKind : FString(),
		FString(TEXT("asset_factory")));
	TestEqual(TEXT("DataAsset asset factory routes to DataAsset surface"),
		AssetEvidence.AtomicTargets.Num() == 1
			? static_cast<int32>(AssetEvidence.AtomicTargets[0].Surface)
			: static_cast<int32>(EBlueprintHelperReviewSurface::Unknown),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));

	FBlueprintHelperTaskRuntimeLoweredStep GraphWriteStep = FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT("append_blueprint_graph"));
	GraphWriteStep.Payload = Payload;
	FBlueprintHelperWriteReviewEvidence GraphWriteEvidence;
	TestFalse(TEXT("journal-backed graph write cluster does not use runtime fallback evidence"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			GraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceAppliedResult(GraphWriteStep.AdapterOperation),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			3,
			GraphWriteEvidence));
	return true;
}

#endif
