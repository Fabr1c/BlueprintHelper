#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "BlueprintActionDatabase.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeFunctionActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeFunctionActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperFunctionAction/%s"),
		*MakeFunctionActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeFunctionActionTestObjectName(TEXT("BP_FunctionAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperFunctionActionClusterTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetFunctionActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperCallFunctionPinType MakeFunctionActionPinType(const FString& Category)
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Category;
	return PinType;
}

static FBlueprintHelperActionResolutionRequest MakeFunctionActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeFunctionActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("function_action_projected_context");
	Request.SemanticConstraintsHash = TEXT("function_action_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.Query = Query;
	Request.Semantic.SearchMode = TEXT("contextual");
	Request.Semantic.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	Request.MaxCandidates = 16;
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionConvertRequiresEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ConvertRequiresEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionConvertRequiresEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFunctionActionTestBlueprint();
	UEdGraph* Graph = GetFunctionActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert, TEXT("Conv_StringToName"));
	Request.Semantic.FunctionOperation = TEXT("convert_function");
	Request.Semantic.TransformOperation = TEXT("convert");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("convert without typed pin evidence is invalid"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("convert missing evidence error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestEqual(TEXT("convert result cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestFalse(TEXT("convert missing evidence has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionScheduleRequiresEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ScheduleRequiresEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionScheduleRequiresEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFunctionActionTestBlueprint();
	UEdGraph* Graph = GetFunctionActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule, FString());
	Request.Semantic.FunctionOperation = TEXT("schedule_function");
	Request.Semantic.ScheduleOperation = TEXT("latent_or_async");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("schedule without query is invalid"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("schedule missing evidence error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestEqual(TEXT("schedule result cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestFalse(TEXT("schedule missing evidence has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionConvertDispatchesToCallResolverTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.ConvertDispatchesToCallResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionConvertDispatchesToCallResolverTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFunctionActionTestBlueprint();
	UEdGraph* Graph = GetFunctionActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert, TEXT("PrintString"));
	Request.Semantic.FunctionOperation = TEXT("convert_function");
	Request.Semantic.TransformOperation = TEXT("convert");
	Request.Semantic.ArgumentNames.Add(TEXT("InString"));
	Request.Semantic.ArgumentTypes.Add(TEXT("InString"), TEXT("string"));
	Request.Semantic.ArgumentPinTypes.Add(TEXT("InString"), MakeFunctionActionPinType(TEXT("string")));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("convert dispatches to call resolver status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("convert dispatches through FunctionAction"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("convert selected stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
	TestTrue(TEXT("convert selected spawner preserved"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("convert selected function preserved"), Result.SelectedFunction.IsValid());
	TestFalse(TEXT("convert candidate diagnostics preserved"), Result.CandidateActions.IsEmpty());
	TestFalse(TEXT("convert selected function candidate preserved"), Result.FunctionCandidate.StableId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionActionLatentScheduleRequiresLatentGraphTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.LatentScheduleRequiresLatentGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionActionLatentScheduleRequiresLatentGraphTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFunctionActionTestBlueprint();
	UEdGraph* Graph = GetFunctionActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request =
		MakeFunctionActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule, TEXT("Delay"));
	Request.Semantic.FunctionOperation = TEXT("latent_or_async_function");
	Request.Semantic.ScheduleOperation = TEXT("latent_or_async");
	Request.ContextEvidence.Add(TEXT("graph_latent_allowed"), TEXT("false"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("latent schedule is invalid without latent graph evidence"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("latent schedule error"), Result.ErrorCode, FString(TEXT("latent_function_not_allowed_in_graph")));
	TestEqual(TEXT("latent schedule result cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestFalse(TEXT("latent schedule rejection has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

#endif
