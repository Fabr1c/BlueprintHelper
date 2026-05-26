#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericOpsOwnershipObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericOpsOwnershipBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericOpsOwnership/%s"),
		*MakeGenericOpsOwnershipObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericOpsOwnershipObjectName(TEXT("BP_GenericOpsOwnership")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericOpsOwnershipTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericOpsOwnershipGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeGenericOpsOwnershipRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const EBlueprintHelperSpawnerClusterKind ClusterKind,
	const EBlueprintHelperActionSemanticKind SemanticKind)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = ClusterKind;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericOpsOwnershipObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_ops_ownership_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_ops_ownership_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.SearchMode = TEXT("exact");
	Request.Semantic.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	Request.MaxCandidates = 8;
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsFunctionBackedCreateOwnedByFunctionActionTest,
	"BlueprintHelper.GraphWrite.GenericOps.Ownership.FunctionBackedCreateOwnedByFunctionAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsFunctionBackedCreateOwnedByFunctionActionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionSemanticConstraints Semantic;
	Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	Semantic.FunctionOperation = TEXT("create_function");
	TestTrue(
		TEXT("FunctionAction supports create_function as a second-stage operation"),
		FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(Semantic));

	Semantic.FunctionOperation = TEXT("convert_function");
	TestFalse(
		TEXT("create does not borrow convert_function ownership"),
		FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(Semantic));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsAmbiguousGenericFunctionOwnerTest,
	"BlueprintHelper.GraphWrite.GenericOps.Ownership.AmbiguousGenericFunctionOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsAmbiguousGenericFunctionOwnerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericOpsOwnershipBlueprint();
	UEdGraph* Graph = GetGenericOpsOwnershipGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest ConvertRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::FunctionAction,
		EBlueprintHelperActionSemanticKind::Convert);
	ConvertRequest.Semantic.FunctionOperation = TEXT("convert_function");
	ConvertRequest.Semantic.TransformOperation = TEXT("dynamic_cast");
	ConvertRequest.Semantic.Query = TEXT("Conv_StringToName");
	const FBlueprintHelperActionResolutionResult ConvertResult =
		FBlueprintHelperActionResolutionCore::Resolve(ConvertRequest);
	TestEqual(TEXT("convert owner ambiguity status"), ConvertResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("convert owner ambiguity code"), ConvertResult.ErrorCode, FString(TEXT("ambiguous_generic_function_owner")));

	FBlueprintHelperActionResolutionRequest CreateRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::FunctionAction,
		EBlueprintHelperActionSemanticKind::Create);
	CreateRequest.Semantic.FunctionOperation = TEXT("create_function");
	CreateRequest.Semantic.CreateOperation = TEXT("spawn_actor");
	CreateRequest.Semantic.Query = TEXT("SpawnActor");
	const FBlueprintHelperActionResolutionResult CreateResult =
		FBlueprintHelperActionResolutionCore::Resolve(CreateRequest);
	TestEqual(TEXT("create owner ambiguity status"), CreateResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("create owner ambiguity code"), CreateResult.ErrorCode, FString(TEXT("ambiguous_generic_function_owner")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsGenericResolversRejectFunctionBackedOwnerTest,
	"BlueprintHelper.GraphWrite.GenericOps.Ownership.GenericResolversRejectFunctionBackedOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsGenericResolversRejectFunctionBackedOwnerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericOpsOwnershipBlueprint();
	UEdGraph* Graph = GetGenericOpsOwnershipGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest CreateRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction,
		EBlueprintHelperActionSemanticKind::Create);
	CreateRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
	CreateRequest.Semantic.CreateOperation = TEXT("function_backed_create");
	CreateRequest.Semantic.FunctionOperation = TEXT("create_function");
	CreateRequest.Semantic.Query = TEXT("CreateWidget");
	const FBlueprintHelperActionResolutionResult CreateResult =
		FBlueprintHelperActionResolutionCore::Resolve(CreateRequest);
	TestEqual(TEXT("function-backed create is wrong generic owner"), CreateResult.Status, EBlueprintHelperActionResolutionStatus::UnsupportedIntent);
	TestEqual(TEXT("function-backed create wrong owner code"), CreateResult.ErrorCode, FString(TEXT("function_backed_operation_wrong_owner")));

	FBlueprintHelperActionResolutionRequest ScheduleRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction,
		EBlueprintHelperActionSemanticKind::Schedule);
	ScheduleRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Schedule;
	ScheduleRequest.Semantic.ScheduleOperation = TEXT("timer_by_handle");
	ScheduleRequest.Semantic.FunctionOperation = TEXT("schedule_function");
	ScheduleRequest.Semantic.Query = TEXT("ClearAndInvalidateTimerHandle");
	const FBlueprintHelperActionResolutionResult ScheduleResult =
		FBlueprintHelperActionResolutionCore::Resolve(ScheduleRequest);
	TestEqual(TEXT("function-backed schedule is wrong generic owner"), ScheduleResult.Status, EBlueprintHelperActionResolutionStatus::UnsupportedIntent);
	TestEqual(TEXT("function-backed schedule wrong owner code"), ScheduleResult.ErrorCode, FString(TEXT("function_backed_operation_wrong_owner")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsControlBoundaryRequiresDedicatedBuildersTest,
	"BlueprintHelper.GraphWrite.GenericOps.Ownership.ControlBoundaryRequiresDedicatedBuilders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsControlBoundaryRequiresDedicatedBuildersTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericOpsOwnershipBlueprint();
	UEdGraph* Graph = GetGenericOpsOwnershipGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest SwitchRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction,
		EBlueprintHelperActionSemanticKind::Control);
	SwitchRequest.Semantic.Query = TEXT("switch_enum");
	SwitchRequest.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("switch_enum"));
	SwitchRequest.ContextEvidence.Add(TEXT("generic.control.case_values"), TEXT("Idle,Running"));
	const FBlueprintHelperGenericActionProviderBoundary SwitchBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(SwitchRequest);
	TestEqual(TEXT("switch uses dedicated control builder"), SwitchBoundary.Mode, EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired);
	TestEqual(TEXT("switch builder"), SwitchBoundary.RequiredBuilder, FString(TEXT("ControlFlowFragmentBuilder")));

	FBlueprintHelperActionResolutionRequest MacroRequest = MakeGenericOpsOwnershipRequest(
		Blueprint,
		Graph,
		EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction,
		EBlueprintHelperActionSemanticKind::Control);
	MacroRequest.Semantic.Query = TEXT("for_loop");
	MacroRequest.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("for_loop"));
	MacroRequest.ContextEvidence.Add(TEXT("generic.macro.graph_path"), TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop"));
	FBlueprintHelperGenericActionProviderBoundary MissingSnapshotBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(MacroRequest);
	TestEqual(TEXT("macro missing snapshot needs context"), MissingSnapshotBoundary.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);
	TestEqual(TEXT("macro builder"), MissingSnapshotBoundary.RequiredBuilder, FString(TEXT("MacroControlFragmentBuilder")));

	MacroRequest.ContextEvidence.Add(TEXT("generic.macro.pin_shape_snapshot"), TEXT("Exec,LoopBody,Completed,Index"));
	const FBlueprintHelperGenericActionProviderBoundary MacroBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(MacroRequest);
	TestEqual(TEXT("macro with snapshot uses dedicated macro builder"), MacroBoundary.Mode, EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired);
	TestEqual(TEXT("macro builder with snapshot"), MacroBoundary.RequiredBuilder, FString(TEXT("MacroControlFragmentBuilder")));
	return true;
}

#endif
