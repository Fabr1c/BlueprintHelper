#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeStandardMacroName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeStandardMacroBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperStandardMacro/%s"),
		*MakeStandardMacroName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeStandardMacroName(TEXT("BP_StandardMacro")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperStandardMacroControlFlowTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetStandardMacroGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsStandardMacroResolveTest,
	"BlueprintHelper.GraphWrite.GenericOps.StandardMacros.ResolveForLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsStandardMacroResolveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeStandardMacroBlueprint();
	UEdGraph* Graph = GetStandardMacroGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeStandardMacroName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("standard_macro_projected_context");
	Request.SemanticConstraintsHash = TEXT("standard_macro_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control;
	Request.Semantic.Query = TEXT("for_loop");
	Request.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("for_loop"));
	Request.ContextEvidence.Add(TEXT("generic.macro.graph_path"), TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop"));
	Request.ContextEvidence.Add(TEXT("generic.macro.pin_shape_snapshot"), TEXT("Exec,LoopBody,Completed,Index"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("for_loop macro resolved"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("for_loop macro selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("for_loop macro node class"), Result.NodeClass.Contains(TEXT("K2Node_MacroInstance")));
	TestTrue(TEXT("for_loop no dedicated builder blocker"), Result.ErrorCode != TEXT("dedicated_fragment_builder_required"));

	FString SpawnError;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Graph,
		Result,
		FVector2D(256.0f, 128.0f),
		SpawnError);
	TestNotNull(TEXT("for_loop macro spawned node"), SpawnedNode);
	TestNotNull(TEXT("spawned macro instance"), Cast<UK2Node_MacroInstance>(SpawnedNode));

	FBlueprintHelperGenericOpsReadbackExpectation Expectation;
	Expectation.Family = TEXT("control");
	Expectation.OperationId = TEXT("generic_ops.control.for_loop");
	Expectation.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_MacroInstance");
	Expectation.RequiredFacts.Add(TEXT("generic.macro.pin_shape_snapshot"), TEXT("Exec,LoopBody,Completed,Index"));
	FString FailureCode;
	FString Failure;
	TestTrue(
		TEXT("for_loop readback verifies actual macro node"),
		FBlueprintHelperGenericOpsReadbackVerifier::Verify(Result, SpawnedNode, Expectation, FailureCode, Failure));
	return true;
}

#endif
