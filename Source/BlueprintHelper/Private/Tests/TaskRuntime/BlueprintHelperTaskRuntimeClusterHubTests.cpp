#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/DataTable/BlueprintHelperDataTableTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FBlueprintHelperTaskRuntimeLoweredStep MakeLoweredStep(const FString& Capability, const FString& AdapterOperation)
{
	FBlueprintHelperTaskRuntimeLoweredStep Step;
	Step.StepId = TEXT("step-1");
	Step.Capability = Capability;
	Step.AdapterOperation = AdapterOperation;
	return Step;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeClusterHub_ResolvesLoweredSteps,
	"BlueprintHelper.TaskRuntime.Cluster.ResolvesLoweredSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeClusterHub_ResolvesLoweredSteps::RunTest(const FString& Parameters)
{
	const TPair<FBlueprintHelperTaskRuntimeLoweredStep, EBlueprintHelperTaskRuntimeCluster> Cases[] = {
		{MakeLoweredStep(TEXT(""), TEXT("append_blueprint_graph")), EBlueprintHelperTaskRuntimeCluster::GraphWrite},
		{MakeLoweredStep(TEXT("blueprint_variable"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::BlueprintVariables},
		{MakeLoweredStep(TEXT("asset_factory"), TEXT("asset_factory.create_asset")), EBlueprintHelperTaskRuntimeCluster::AssetFactory},
		{MakeLoweredStep(TEXT("blueprint_component"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::Component},
		{MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::ClassSettings},
		{MakeLoweredStep(TEXT("blueprint_signature"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::Signature},
		{MakeLoweredStep(TEXT("umg_widget"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::UMGWidget},
		{MakeLoweredStep(TEXT("data_table"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::DataTable},
		{MakeLoweredStep(TEXT("object_property"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::ObjectProperty},
		{MakeLoweredStep(TEXT("cleanup_ownership"), TEXT("")), EBlueprintHelperTaskRuntimeCluster::CleanupOwnership},
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
	const FBlueprintHelperTaskRuntimeLoweredStep Step = MakeLoweredStep(TEXT("unknown_capability"), TEXT("unknown_operation"));
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
			FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT(""), AdapterOperation)));
	}

	TestTrue(
		TEXT("graph_write capability resolves as GraphWrite"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	TestFalse(
		TEXT("cleanup lowered step is not GraphWrite"),
		FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("cleanup_ownership"), TEXT(""))));
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
		FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_variable"), TEXT(""))));
	TestFalse(
		TEXT("BlueprintVariables cluster rejects component capability"),
		FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_component"), TEXT(""))));

	TestTrue(
		TEXT("Component cluster recognizes component capability"),
		FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_component"), TEXT(""))));
	TestFalse(
		TEXT("Component cluster rejects class settings capability"),
		FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT(""))));

	TestTrue(
		TEXT("ClassSettings cluster recognizes class settings capability"),
		FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_class_settings"), TEXT(""))));
	TestFalse(
		TEXT("ClassSettings cluster rejects signature capability"),
		FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_signature"), TEXT(""))));

	TestTrue(
		TEXT("Signature cluster recognizes signature capability"),
		FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("blueprint_signature"), TEXT(""))));
	TestFalse(
		TEXT("Signature cluster rejects graph write capability"),
		FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
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
		FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("umg_widget"), TEXT(""))));
	TestFalse(
		TEXT("UMGWidget cluster rejects datatable capability"),
		FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("data_table"), TEXT(""))));

	TestTrue(
		TEXT("DataTable cluster recognizes datatable capability"),
		FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("data_table"), TEXT(""))));
	TestFalse(
		TEXT("DataTable cluster rejects object property capability"),
		FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("object_property"), TEXT(""))));

	TestTrue(
		TEXT("ObjectProperty cluster recognizes object property capability"),
		FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("object_property"), TEXT(""))));
	TestFalse(
		TEXT("ObjectProperty cluster rejects cleanup capability"),
		FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("cleanup_ownership"), TEXT(""))));

	TestTrue(
		TEXT("CleanupOwnership cluster recognizes cleanup capability"),
		FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("cleanup_ownership"), TEXT(""))));
	TestFalse(
		TEXT("CleanupOwnership cluster rejects graph write capability"),
		FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::CanExecuteStep(MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReservedTaskRuntimeClusters_DoNotClaimSteps,
	"BlueprintHelper.TaskRuntime.Cluster.ReservedClustersDoNotClaimSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReservedTaskRuntimeClusters_DoNotClaimSteps::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("AnimationBlueprint enum is reserved"),
		EBlueprintHelperTaskRuntimeCluster::AnimationBlueprint != EBlueprintHelperTaskRuntimeCluster::Unknown);
	TestTrue(
		TEXT("Material enum is reserved"),
		EBlueprintHelperTaskRuntimeCluster::Material != EBlueprintHelperTaskRuntimeCluster::Unknown);
	TestFalse(
		TEXT("AnimationBlueprint placeholder rejects current animation capability name"),
		FBlueprintHelperAnimationBlueprintTaskRuntimeCluster::CanExecuteStep(
			MakeLoweredStep(TEXT("animation_blueprint"), TEXT(""))));
	TestFalse(
		TEXT("AnimationBlueprint placeholder rejects graph write"),
		FBlueprintHelperAnimationBlueprintTaskRuntimeCluster::CanExecuteStep(
			MakeLoweredStep(TEXT("graph_write"), TEXT(""))));
	TestFalse(
		TEXT("Material placeholder rejects current material capability name"),
		FBlueprintHelperMaterialTaskRuntimeCluster::CanExecuteStep(
			MakeLoweredStep(TEXT("material"), TEXT(""))));
	TestFalse(
		TEXT("Material placeholder rejects asset factory"),
		FBlueprintHelperMaterialTaskRuntimeCluster::CanExecuteStep(
			MakeLoweredStep(TEXT("asset_factory"), TEXT(""))));
	return true;
}

#endif
