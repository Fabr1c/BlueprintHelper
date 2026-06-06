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
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipValidator.h"

#include "Dom/JsonValue.h"
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

static FBlueprintHelperToolResultBase MakeGraphWriteAppliedResultWithBlockRefs(
	const FString& Operation,
	const TArray<FString>& BlockRefs)
{
	FBlueprintHelperToolResultBase Result = MakeClusterEvidenceAppliedResult(Operation);
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> AppendResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BlockRefValues;
	for (const FString& BlockRef : BlockRefs)
	{
		BlockRefValues.Add(MakeShared<FJsonValueString>(BlockRef));
	}
	AppendResult->SetArrayField(TEXT("block_refs"), BlockRefValues);
	Data->SetObjectField(TEXT("append_result"), AppendResult);
	Result.Data = Data;
	return Result;
}

static FBlueprintHelperToolResultBase MakeGraphWriteAppliedResultWithTopLevelBlockRefs(
	const FString& Operation,
	const TArray<FString>& BlockRefs)
{
	FBlueprintHelperToolResultBase Result = MakeClusterEvidenceAppliedResult(Operation);
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BlockRefValues;
	for (const FString& BlockRef : BlockRefs)
	{
		BlockRefValues.Add(MakeShared<FJsonValueString>(BlockRef));
	}
	Data->SetArrayField(TEXT("block_refs"), BlockRefValues);
	Result.Data = Data;
	return Result;
}

static FBlueprintHelperToolResultBase MakeClusterEvidenceFailedResult(const FString& Operation)
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("test_failure");
	Error.Stage = EBlueprintHelperToolStage::Execute;
	Error.Message = TEXT("intentional test failure");
	return FBlueprintHelperToolResultBuilder::Failure(Operation, TEXT("trace_cluster_evidence"), Error);
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
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("merge_external_flow")), EBlueprintHelperTaskRuntimeCluster::GraphWrite},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("patch_external_graph")), EBlueprintHelperTaskRuntimeCluster::GraphWrite},
		{FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT(""), TEXT("replace_external_body")), EBlueprintHelperTaskRuntimeCluster::GraphWrite},
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
	FBlueprintHelperTaskRuntimeClusterHub_RegisteredClustersCoverImplementedClusters,
	"BlueprintHelper.TaskRuntime.ClusterRegistry.CoversImplementedClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeClusterHub_RegisteredClustersCoverImplementedClusters::RunTest(const FString& Parameters)
{
	const TArray<EBlueprintHelperTaskRuntimeCluster>& RegisteredClusters =
		FBlueprintHelperTaskRuntimeClusterHub::GetRegisteredClusters();
	const EBlueprintHelperTaskRuntimeCluster ExpectedClusters[] = {
		EBlueprintHelperTaskRuntimeCluster::GraphWrite,
		EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
		EBlueprintHelperTaskRuntimeCluster::AssetFactory,
		EBlueprintHelperTaskRuntimeCluster::Component,
		EBlueprintHelperTaskRuntimeCluster::ClassSettings,
		EBlueprintHelperTaskRuntimeCluster::Signature,
		EBlueprintHelperTaskRuntimeCluster::UMGWidget,
		EBlueprintHelperTaskRuntimeCluster::DataTable,
		EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
	};

	TestEqual(TEXT("registered cluster count"), RegisteredClusters.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedClusters)));
	for (const EBlueprintHelperTaskRuntimeCluster ExpectedCluster : ExpectedClusters)
	{
		TestTrue(TEXT("expected cluster is registered"), RegisteredClusters.Contains(ExpectedCluster));
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
		TEXT("merge_external_flow"),
		TEXT("patch_external_graph"),
		TEXT("replace_external_body"),
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

	TSharedRef<FJsonObject> GraphWritePayload = MakeShared<FJsonObject>();
	GraphWritePayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Door"));
	GraphWritePayload->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	GraphWritePayload->SetStringField(TEXT("kind"), TEXT("create"));
	GraphWritePayload->SetStringField(TEXT("create_operation"), TEXT("asset_action"));
	TSharedRef<FJsonObject> GraphWriteContextEvidence = MakeShared<FJsonObject>();
	GraphWriteContextEvidence->SetStringField(TEXT("asset_action_stable_id"), TEXT("action_database:/Script/Engine.Blueprint:/Script/BlueprintGraph.K2Node_MakeArray:MakeArray"));
	GraphWriteContextEvidence->SetStringField(TEXT("asset_action_node_class"), TEXT("/Script/BlueprintGraph.K2Node_MakeArray"));
	GraphWriteContextEvidence->SetStringField(TEXT("asset_action_spawner_signature"), TEXT("MakeArray"));
	GraphWriteContextEvidence->SetStringField(TEXT("asset_action_owner_path"), TEXT("/Script/Engine.Blueprint"));
	GraphWritePayload->SetObjectField(TEXT("context_evidence"), GraphWriteContextEvidence);

	FBlueprintHelperTaskRuntimeLoweredStep GraphWriteStep = FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT("append_blueprint_graph"));
	GraphWriteStep.Payload = GraphWritePayload;
	FBlueprintHelperWriteReviewEvidence GraphWriteEvidence;
	TestTrue(TEXT("graph write cluster owns Review evidence production"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			GraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeGraphWriteAppliedResultWithBlockRefs(
				GraphWriteStep.AdapterOperation,
				{TEXT("CE_DumpGlobalStateForReview0")}),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			3,
			GraphWriteEvidence));
	TestEqual(TEXT("graph write evidence asset path is required"),
		GraphWriteEvidence.AssetPath,
		FString(TEXT("/Game/BP_Door")));
	TestEqual(TEXT("graph write operation kind is required"),
		GraphWriteEvidence.OperationKind,
		FString(TEXT("append_blueprint_graph")));
	TestEqual(TEXT("graph write task step index is required"),
		GraphWriteEvidence.TaskStepIndex,
		3);
	TestEqual(TEXT("graph write emits one graph block target from result refs"), GraphWriteEvidence.AtomicTargets.Num(), 1);
	if (GraphWriteEvidence.AtomicTargets.Num() != 1)
	{
		return false;
	}
	const FBlueprintHelperReviewAtomicTarget& GraphTarget = GraphWriteEvidence.AtomicTargets[0];
	TestEqual(TEXT("graph write target kind is graph_block"),
		GraphTarget.TargetKind,
		FString(TEXT("graph_block")));
	TestEqual(TEXT("graph write target key uses actual block ref"),
		GraphTarget.TargetKey,
		FString(TEXT("graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0")));
	TestEqual(TEXT("graph write visual group is graph body"),
		GraphTarget.VisualGroupKey,
		FString(TEXT("graph_body|EventGraph")));
	TestEqual(TEXT("graph write target graph name is required"),
		GraphTarget.GraphName,
		FString(TEXT("EventGraph")));
	TestEqual(TEXT("graph write target operation step is required"),
		GraphTarget.TaskStepIndex,
		3);
	TestEqual(TEXT("graph write target ownership is producer owned"),
		GraphTarget.Ownership,
		FString(TEXT("graph_write")));
	TestEqual(TEXT("graph write target latest evidence id is preserved"),
		GraphTarget.LatestEvidenceId,
		GraphWriteEvidence.EvidenceId);
	TestEqual(TEXT("graph write target keeps one source evidence id"),
		GraphTarget.SourceEvidenceIds.Num(),
		1);
	TestEqual(TEXT("graph write target source evidence id is preserved"),
		GraphTarget.SourceEvidenceIds.Num() == 1 ? GraphTarget.SourceEvidenceIds[0] : FString(),
		GraphWriteEvidence.EvidenceId);
	TestEqual(TEXT("graph write target surface is graph"),
		static_cast<int32>(GraphTarget.Surface),
		static_cast<int32>(EBlueprintHelperReviewSurface::Graph));
	TestTrue(TEXT("graph write anchor preserves asset action stable id"),
		GraphTarget.AnchorJson.Contains(TEXT("asset_action_stable_id")));
	TestTrue(TEXT("graph write anchor preserves asset action operation"),
		GraphTarget.AnchorJson.Contains(TEXT("create_operation")));

	FBlueprintHelperTaskRuntimeLoweredStep ReplaceGraphWriteStep = GraphWriteStep;
	ReplaceGraphWriteStep.AdapterOperation = TEXT("replace_blueprint_graph");
	FBlueprintHelperWriteReviewEvidence ReplaceGraphWriteEvidence;
	TestTrue(TEXT("replace graph write cluster reads top-level block refs"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			ReplaceGraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeGraphWriteAppliedResultWithTopLevelBlockRefs(
				ReplaceGraphWriteStep.AdapterOperation,
				{TEXT("EventGraph_CE_DumpGlobalStateForReview0")}),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			4,
			ReplaceGraphWriteEvidence));
	TestEqual(TEXT("replace graph write evidence uses actual top-level block ref"),
		ReplaceGraphWriteEvidence.AtomicTargets.Num() == 1 ? ReplaceGraphWriteEvidence.AtomicTargets[0].TargetKey : FString(),
		FString(TEXT("graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0")));

	FBlueprintHelperTaskRuntimeLoweredStep PatchGraphWriteStep = GraphWriteStep;
	PatchGraphWriteStep.AdapterOperation = TEXT("patch_blueprint_graph");

	FBlueprintHelperWriteReviewEvidence LinkPatchGraphWriteEvidence;
	TestTrue(TEXT("link patch graph write cluster reads top-level block refs"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			PatchGraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeGraphWriteAppliedResultWithTopLevelBlockRefs(
				PatchGraphWriteStep.AdapterOperation,
				{TEXT("EventGraph_OpenDoorLinkPatch")}),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			5,
			LinkPatchGraphWriteEvidence));
	TestEqual(TEXT("link patch graph write emits one graph block target"),
		LinkPatchGraphWriteEvidence.AtomicTargets.Num(),
		1);
	if (LinkPatchGraphWriteEvidence.AtomicTargets.Num() != 1)
	{
		return false;
	}
	const FBlueprintHelperReviewAtomicTarget& LinkPatchTarget = LinkPatchGraphWriteEvidence.AtomicTargets[0];
	TestEqual(TEXT("link patch target kind is graph_block"),
		LinkPatchTarget.TargetKind,
		FString(TEXT("graph_block")));
	TestEqual(TEXT("link patch target key uses actual patch block ref"),
		LinkPatchTarget.TargetKey,
		FString(TEXT("graph:EventGraph:block:EventGraph_OpenDoorLinkPatch")));
	TestEqual(TEXT("link patch visual group is graph body"),
		LinkPatchTarget.VisualGroupKey,
		FString(TEXT("graph_body|EventGraph")));
	TestEqual(TEXT("link patch target ownership is graph_write"),
		LinkPatchTarget.Ownership,
		FString(TEXT("graph_write")));

	FBlueprintHelperWriteReviewEvidence DeletePatchGraphWriteEvidence;
	TestTrue(TEXT("delete owned node graph write cluster reads top-level block refs"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			PatchGraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeGraphWriteAppliedResultWithTopLevelBlockRefs(
				PatchGraphWriteStep.AdapterOperation,
				{TEXT("EventGraph_DeleteDebugPrint")}),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			6,
			DeletePatchGraphWriteEvidence));
	TestEqual(TEXT("delete owned node patch emits one graph block target"),
		DeletePatchGraphWriteEvidence.AtomicTargets.Num(),
		1);
	if (DeletePatchGraphWriteEvidence.AtomicTargets.Num() != 1)
	{
		return false;
	}
	const FBlueprintHelperReviewAtomicTarget& DeletePatchTarget = DeletePatchGraphWriteEvidence.AtomicTargets[0];
	TestEqual(TEXT("delete owned node target kind is graph_block"),
		DeletePatchTarget.TargetKind,
		FString(TEXT("graph_block")));
	TestEqual(TEXT("delete owned node target key uses actual patch block ref"),
		DeletePatchTarget.TargetKey,
		FString(TEXT("graph:EventGraph:block:EventGraph_DeleteDebugPrint")));
	TestEqual(TEXT("delete owned node visual group is graph body"),
		DeletePatchTarget.VisualGroupKey,
		FString(TEXT("graph_body|EventGraph")));
	TestEqual(TEXT("delete owned node target ownership is graph_write"),
		DeletePatchTarget.Ownership,
		FString(TEXT("graph_write")));

	FBlueprintHelperWriteReviewEvidence FailedGraphWriteEvidence;
	TestFalse(TEXT("failed graph write step does not produce Review evidence"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			GraphWriteStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceFailedResult(GraphWriteStep.AdapterOperation),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			3,
			FailedGraphWriteEvidence));

	TSharedRef<FJsonObject> MissingGraphPayload = MakeShared<FJsonObject>();
	MissingGraphPayload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Door"));
	FBlueprintHelperTaskRuntimeLoweredStep MissingGraphStep = GraphWriteStep;
	MissingGraphStep.Payload = MissingGraphPayload;
	FBlueprintHelperWriteReviewEvidence MissingGraphEvidence;
	TestFalse(TEXT("graph write step without graph does not produce Review evidence"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			MissingGraphStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceAppliedResult(MissingGraphStep.AdapterOperation),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			3,
			MissingGraphEvidence));

	TSharedRef<FJsonObject> MissingAssetPayload = MakeShared<FJsonObject>();
	MissingAssetPayload->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	FBlueprintHelperTaskRuntimeLoweredStep MissingAssetStep = GraphWriteStep;
	MissingAssetStep.Payload = MissingAssetPayload;
	FBlueprintHelperWriteReviewEvidence MissingAssetEvidence;
	TestFalse(TEXT("graph write step without asset does not produce Review evidence"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
			MissingAssetStep,
			FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeClusterEvidenceAppliedResult(MissingAssetStep.AdapterOperation),
			TEXT("archive_cluster_evidence"),
			TEXT("task_cluster_evidence"),
			3,
			MissingAssetEvidence));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeGraphWriteOwnershipValidatorRejectsMissingAtomicTargetTest,
	"BlueprintHelper.TaskRuntime.GraphWrite.OwnershipValidation.RejectsMissingAtomicTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeGraphWriteOwnershipValidatorRejectsMissingAtomicTargetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteOwnershipValidationInput Input;
	Input.GeneratedBlockRefs.Add(TEXT("EventGraph_CE_Orphan"));

	const FBlueprintHelperGraphWriteOwnershipValidationResult Result =
		FBlueprintHelperGraphWriteOwnershipValidator::Validate(Input);

	TestFalse(TEXT("missing atomic target fails ownership validation"), Result.bPassed);
	TestEqual(TEXT("one missing ownership diagnostic"), Result.Diagnostics.Num(), 1);
	if (Result.Diagnostics.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("missing target diagnostic code"), Result.Diagnostics[0].Code, FString(TEXT("unregistered_generated_node")));
	TestEqual(TEXT("missing target diagnostic node id"), Result.Diagnostics[0].NodeId, FString(TEXT("EventGraph_CE_Orphan")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeMergeExternalFlow_BuildsExternalBoundaryReviewEvidence,
	"BlueprintHelper.TaskRuntime.GraphWrite.MergeExternalFlow.BuildsExternalBoundaryReviewEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeMergeExternalFlow_BuildsExternalBoundaryReviewEvidence::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
	Payload->SetObjectField(TEXT("target"), Target);

	TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
	Anchor->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ExternalGraphAnchor.v1"));
	Anchor->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Anchor->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	Anchor->SetStringField(TEXT("node_guid"), TEXT("11111111-2222-3333-4444-555555555555"));
	Anchor->SetStringField(TEXT("node_class"), TEXT("/Script/BlueprintGraph.K2Node_CustomEvent"));
	Anchor->SetStringField(TEXT("pin_name"), TEXT("then"));
	Anchor->SetStringField(TEXT("pin_direction"), TEXT("output"));
	Anchor->SetStringField(TEXT("semantic_role"), TEXT("exec_boundary"));
	Anchor->SetStringField(TEXT("fingerprint"), TEXT("fingerprint"));
	Payload->SetObjectField(TEXT("anchor"), Anchor);

	TSharedRef<FJsonObject> Inserted = MakeShared<FJsonObject>();
	Inserted->SetStringField(TEXT("block_id"), TEXT("ExternalMerge_step_1"));
	Payload->SetObjectField(TEXT("inserted"), Inserted);

	FBlueprintHelperTaskRuntimeLoweredStep Step =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT("merge_external_flow"));
	Step.Payload = Payload;

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
		Step,
		TEXT("archive_external_flow"),
		TEXT("task_external_flow"),
		7,
		Evidence);

	TestTrue(TEXT("merge_external_flow builds Review evidence"), bBuilt);
	TestEqual(TEXT("nested target asset path is used"),
		Evidence.AssetPath,
		FString(TEXT("/Game/BP_External")));
	TestEqual(TEXT("operation kind is merge_external_flow"),
		Evidence.OperationKind,
		FString(TEXT("merge_external_flow")));
	TestEqual(TEXT("two atomic targets are emitted"),
		Evidence.AtomicTargets.Num(),
		2);
	if (Evidence.AtomicTargets.Num() != 2)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& BoundaryTarget = Evidence.AtomicTargets[0];
	TestEqual(TEXT("boundary target kind"),
		BoundaryTarget.TargetKind,
		FString(TEXT("graph_external_boundary")));
	TestEqual(TEXT("boundary handler is external boundary"),
		static_cast<int32>(FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(BoundaryTarget.TargetKind)),
		static_cast<int32>(EBlueprintHelperReviewTargetHandlerKind::GraphExternalBoundary));
	TestEqual(TEXT("boundary target graph"),
		BoundaryTarget.GraphName,
		FString(TEXT("EventGraph")));
	TestEqual(TEXT("boundary ownership is external"),
		BoundaryTarget.Ownership,
		FString(TEXT("external_user_authored")));
	TestEqual(TEXT("boundary node guid is preserved"),
		BoundaryTarget.NodeGuid,
		FString(TEXT("11111111-2222-3333-4444-555555555555")));
	TestEqual(TEXT("boundary pin path is preserved"),
		BoundaryTarget.PinPath,
		FString(TEXT("then")));
	TestTrue(TEXT("boundary anchor stores external anchor schema"),
		BoundaryTarget.AnchorJson.Contains(TEXT("BlueprintHelper.ExternalGraphAnchor.v1")));

	const FBlueprintHelperReviewAtomicTarget& InsertedBlockTarget = Evidence.AtomicTargets[1];
	TestEqual(TEXT("inserted target kind"),
		InsertedBlockTarget.TargetKind,
		FString(TEXT("graph_block")));
	TestEqual(TEXT("inserted block target key uses full block id"),
		InsertedBlockTarget.TargetKey,
		FString(TEXT("graph_block:block:EventGraph_ExternalMerge_step_1")));
	TestEqual(TEXT("inserted block ownership is owned"),
		InsertedBlockTarget.Ownership,
		FString(TEXT("blueprinthelper_owned")));
	TestTrue(TEXT("inserted block review payload stores inserted block id"),
		InsertedBlockTarget.AnchorJson.Contains(TEXT("ExternalMerge_step_1")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePatchExternalGraph_BuildsExternalNodeReviewEvidence,
	"BlueprintHelper.TaskRuntime.GraphWrite.PatchExternalGraph.BuildsExternalNodeReviewEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePatchExternalGraph_BuildsExternalNodeReviewEvidence::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
	Payload->SetObjectField(TEXT("target"), Target);
	Payload->SetStringField(TEXT("patch_type"), TEXT("set_external_pin_default"));

	TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
	Anchor->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ExternalGraphAnchor.v1"));
	Anchor->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_External"));
	Anchor->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	Anchor->SetStringField(TEXT("node_guid"), TEXT("11111111-2222-3333-4444-555555555555"));
	Anchor->SetStringField(TEXT("node_class"), TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));
	Anchor->SetStringField(TEXT("pin_name"), TEXT("InString"));
	Anchor->SetStringField(TEXT("pin_direction"), TEXT("input"));
	Anchor->SetStringField(TEXT("semantic_role"), TEXT("node"));
	Anchor->SetStringField(TEXT("fingerprint"), TEXT("fingerprint"));
	Payload->SetObjectField(TEXT("anchor"), Anchor);

	FBlueprintHelperTaskRuntimeLoweredStep Step =
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeLoweredStep(TEXT("graph_write"), TEXT("patch_external_graph"));
	Step.Payload = Payload;

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
		Step,
		TEXT("archive_external_patch"),
		TEXT("task_external_patch"),
		8,
		Evidence);

	TestTrue(TEXT("patch_external_graph builds Review evidence"), bBuilt);
	TestEqual(TEXT("operation kind is patch_external_graph"),
		Evidence.OperationKind,
		FString(TEXT("patch_external_graph")));
	TestEqual(TEXT("one atomic target is emitted"),
		Evidence.AtomicTargets.Num(),
		1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& TargetEvidence = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind"),
		TargetEvidence.TargetKind,
		FString(TEXT("graph_external_node")));
	TestEqual(TEXT("handler kind is external node"),
		static_cast<int32>(FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(TargetEvidence.TargetKind)),
		static_cast<int32>(EBlueprintHelperReviewTargetHandlerKind::GraphExternalNode));
	TestEqual(TEXT("ownership is external"),
		TargetEvidence.Ownership,
		FString(TEXT("external_user_authored")));
	TestEqual(TEXT("node guid is preserved"),
		TargetEvidence.NodeGuid,
		FString(TEXT("11111111-2222-3333-4444-555555555555")));
	TestEqual(TEXT("pin path is preserved"),
		TargetEvidence.PinPath,
		FString(TEXT("InString")));
	TestEqual(TEXT("field kind is preserved"),
		TargetEvidence.PropertyPath,
		FString(TEXT("pin_default")));
	return true;
}

#endif
