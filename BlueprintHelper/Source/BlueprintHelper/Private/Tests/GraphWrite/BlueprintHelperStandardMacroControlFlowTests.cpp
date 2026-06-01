#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsStandardMacroFragmentPinsTest,
	"BlueprintHelper.GraphWrite.GenericOps.StandardMacros.ActionProviderFragmentPinsForEach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsStandardMacroFragmentPinsTest::RunTest(const FString& Parameters)
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
	Request.Semantic.Query = TEXT("foreach_loop");
	Request.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("foreach_loop"));
	Request.ContextEvidence.Add(TEXT("generic.macro.graph_path"), TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForEachLoop"));
	Request.ContextEvidence.Add(TEXT("generic.macro.pin_shape_snapshot"), TEXT("Exec,LoopBody,Completed,Array,Array Element,Array Index"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("foreach_loop macro resolved"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("foreach_loop macro selected spawner"), Result.SelectedSpawner.IsValid());
	if (!Result.IsResolved())
	{
		return false;
	}

	FString SpawnError;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Graph,
		Result,
		FVector2D(256.0f, 128.0f),
		SpawnError);
	TestNotNull(TEXT("foreach_loop macro spawned node"), SpawnedNode);
	TestNotNull(TEXT("spawned macro instance"), Cast<UK2Node_MacroInstance>(SpawnedNode));
	if (!SpawnedNode)
	{
		return false;
	}

	FBlueprintHelperNodeFragment Fragment;
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, Fragment);

	bool bPassed = true;
	bPassed &= TestNotNull(TEXT("action provider macro exposes exec entry"), Fragment.ExecEntryPin);
	bPassed &= TestTrue(TEXT("action provider macro binds execute alias"), Fragment.PinBindings.Contains(TEXT("execute")));
	bPassed &= TestTrue(TEXT("action provider macro binds exec alias"), Fragment.PinBindings.Contains(TEXT("exec")));
	bPassed &= TestNull(TEXT("foreach_loop macro has no synthetic sequential then exit"), Fragment.ExecExitPin);
	bPassed &= TestFalse(TEXT("foreach_loop macro does not bind LoopBody or Completed as then"), Fragment.PinBindings.Contains(TEXT("then")));
	bPassed &= TestTrue(TEXT("foreach_loop macro binds LoopBody output by name"), Fragment.PinBindings.Contains(TEXT("LoopBody")));
	bPassed &= TestTrue(TEXT("foreach_loop macro binds loopbody output by normalized name"), Fragment.PinBindings.Contains(TEXT("loopbody")));
	bPassed &= TestTrue(TEXT("foreach_loop macro binds Completed output by name"), Fragment.PinBindings.Contains(TEXT("Completed")));
	bPassed &= TestTrue(TEXT("foreach_loop macro binds completed output by normalized name"), Fragment.PinBindings.Contains(TEXT("completed")));
	if (const FBlueprintHelperFragmentPinRef* LoopBodyBinding = Fragment.PinBindings.Find(TEXT("LoopBody")))
	{
		bPassed &= TestNotNull(TEXT("LoopBody binding keeps its actual pin"), LoopBodyBinding->Pin);
		if (LoopBodyBinding->Pin)
		{
			bPassed &= TestEqual(TEXT("LoopBody binding keeps LoopBody pin name"), LoopBodyBinding->Pin->PinName.ToString(), FString(TEXT("LoopBody")));
		}
	}
	if (const FBlueprintHelperFragmentPinRef* CompletedBinding = Fragment.PinBindings.Find(TEXT("Completed")))
	{
		bPassed &= TestNotNull(TEXT("Completed binding keeps its actual pin"), CompletedBinding->Pin);
		if (CompletedBinding->Pin)
		{
			bPassed &= TestEqual(TEXT("Completed binding keeps Completed pin name"), CompletedBinding->Pin->PinName.ToString(), FString(TEXT("Completed")));
		}
	}
	return bPassed;
}

#endif
