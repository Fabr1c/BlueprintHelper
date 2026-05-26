#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeControlFlowExtensionName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeControlFlowExtensionBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperControlFlowExtension/%s"),
		*MakeControlFlowExtensionName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeControlFlowExtensionName(TEXT("BP_ControlFlowExtension")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperControlFlowExtensionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetControlFlowExtensionGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeControlRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& Operation)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeControlFlowExtensionName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("control_flow_extension_projected_context");
	Request.SemanticConstraintsHash = TEXT("control_flow_extension_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control;
	Request.Semantic.Query = Operation;
	Request.ContextEvidence.Add(TEXT("generic.control.operation"), Operation);
	Request.MaxCandidates = 4;
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsSwitchAndMultiGateResolveTest,
	"BlueprintHelper.GraphWrite.GenericOps.ControlFlow.ResolveDedicatedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsSwitchAndMultiGateResolveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeControlFlowExtensionBlueprint();
	UEdGraph* Graph = GetControlFlowExtensionGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest SwitchRequest = MakeControlRequest(Blueprint, Graph, TEXT("switch_int"));
	SwitchRequest.ContextEvidence.Add(TEXT("generic.control.case_values"), TEXT("0,1"));
	const FBlueprintHelperActionResolutionResult SwitchResult = FBlueprintHelperActionResolutionCore::Resolve(SwitchRequest);
	TestEqual(TEXT("switch_int resolved"), SwitchResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("switch_int selected spawner"), SwitchResult.SelectedSpawner.IsValid());
	TestTrue(TEXT("switch_int class"), SwitchResult.NodeClass.Contains(TEXT("K2Node_SwitchInteger")));
	TestTrue(TEXT("switch_int no dedicated builder blocker"), SwitchResult.ErrorCode != TEXT("dedicated_fragment_builder_required"));

	FString SpawnError;
	UK2Node* SwitchNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Graph,
		SwitchResult,
		FVector2D(128.0f, 64.0f),
		SpawnError);
	TestNotNull(TEXT("switch_int spawned node"), SwitchNode);

	FBlueprintHelperGenericOpsReadbackExpectation SwitchExpectation;
	SwitchExpectation.Family = TEXT("control");
	SwitchExpectation.OperationId = TEXT("generic_ops.control.switch_int");
	SwitchExpectation.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_SwitchInteger");
	SwitchExpectation.RequiredFacts.Add(TEXT("generic.control.case_values"), TEXT("0,1"));
	SwitchExpectation.RequiredInputPins.Add(TEXT("Selection"));
	SwitchExpectation.RequiredOutputPins.Add(TEXT("Default"));
	FString FailureCode;
	FString Failure;
	TestTrue(
		TEXT("switch_int readback verifies actual node"),
		FBlueprintHelperGenericOpsReadbackVerifier::Verify(SwitchResult, SwitchNode, SwitchExpectation, FailureCode, Failure));

	FBlueprintHelperActionResolutionRequest MultiGateRequest = MakeControlRequest(Blueprint, Graph, TEXT("multi_gate"));
	MultiGateRequest.ContextEvidence.Add(TEXT("generic.control.dynamic_output_count"), TEXT("2"));
	const FBlueprintHelperActionResolutionResult MultiGateResult = FBlueprintHelperActionResolutionCore::Resolve(MultiGateRequest);
	TestEqual(TEXT("multi_gate resolved"), MultiGateResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("multi_gate selected spawner"), MultiGateResult.SelectedSpawner.IsValid());
	TestTrue(TEXT("multi_gate class"), MultiGateResult.NodeClass.Contains(TEXT("K2Node_MultiGate")));
	return true;
}

#endif
