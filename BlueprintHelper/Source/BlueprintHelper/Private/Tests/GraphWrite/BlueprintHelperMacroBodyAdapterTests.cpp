#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2MacroBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"
#include "UObject/Package.h"

class FBlueprintHelperGraphBodyAdapterExtractionTestUtils
{
public:
	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		const FString UniqueName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperGraphBody/%s"), *UniqueName));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *UniqueName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphBodyAdapterTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*FunctionName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
			Blueprint,
			FunctionGraph,
			/*bIsUserCreated=*/ true,
			nullptr);
		EnsureFunctionResult(FunctionGraph);
		return FunctionGraph;
	}

	static UEdGraph* AddMacroGraphWithTunnelNodes(UBlueprint* Blueprint, const FString& MacroName)
	{
		UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*MacroName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);
		return MacroGraph;
	}

	static bool BoundaryArrayContainsNodeRef(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FieldName,
		const FString& NodeRef)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Items) || !Items)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			const TSharedPtr<FJsonObject>* ItemObject = nullptr;
			if (Item.IsValid()
				&& Item->TryGetObject(ItemObject)
				&& ItemObject
				&& ItemObject->IsValid()
				&& (*ItemObject)->GetStringField(TEXT("node_ref")) == NodeRef)
			{
				return true;
			}
		}
		return false;
	}

	static bool StringArrayContains(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FieldName,
		const FString& Value)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Items) || !Items)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			if (Item.IsValid() && Item->AsString() == Value)
			{
				return true;
			}
		}
		return false;
	}

private:
	static void EnsureFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return;
		}
		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (Cast<UK2Node_FunctionResult>(Node))
			{
				return;
			}
		}

		FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*FunctionGraph);
		UK2Node_FunctionResult* ResultNode = NodeCreator.CreateNode(true);
		ResultNode->NodePosX = 600;
		ResultNode->NodePosY = 0;
		NodeCreator.Finalize();
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyAdapterExtractsFunctionBoundariesTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2Function.ExtractsFunctionBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyAdapterExtractsFunctionBoundariesTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("FunctionAdapter"));
	UEdGraph* FunctionGraph =
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddFunctionGraph(Blueprint, TEXT("ComputeScore"));
	TestTrue(TEXT("function graph exists"), FunctionGraph != nullptr);

	FBlueprintHelperGraphBodyRequest Request;
	Request.Blueprint = Blueprint;
	Request.AssetPath = Blueprint->GetPathName();
	Request.GraphName = TEXT("ComputeScore");
	Request.ReplaceScope = TEXT("function_body");
	Request.SelectorKind = TEXT("function");

	FBlueprintHelperK2FunctionBodyAdapter Adapter;
	FBlueprintHelperGraphBodyTarget Target;
	FString Error;
	TestTrue(TEXT("function target resolves"), Adapter.ResolveTarget(Request, Target, Error));

	const FBlueprintHelperGraphBodyBoundaryModel Boundary = Adapter.BuildBoundaryModel(Target, Request);
	TestEqual(TEXT("body kind"), Boundary.BodyKind, EBlueprintHelperGraphBodyKind::K2FunctionBody);
	TestTrue(TEXT("FunctionEntry protected"), Boundary.ProtectedNodeRefs.Contains(TEXT("FunctionEntry")));
	TestTrue(TEXT("FunctionResult protected"), Boundary.ProtectedNodeRefs.Contains(TEXT("FunctionResult")));
	TestTrue(TEXT("FunctionResult exit boundary"), Boundary.ExitNodeRefs.Contains(TEXT("FunctionResult")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyReadbackServiceBuildsFunctionAdapterBoundaryTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.ReadbackService.BuildsFunctionAdapterBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyReadbackServiceBuildsFunctionAdapterBoundaryTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("FunctionReadback"));
	UEdGraph* FunctionGraph =
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddFunctionGraph(Blueprint, TEXT("ComputeScore"));
	TestTrue(TEXT("function graph exists"), FunctionGraph != nullptr);

	FBlueprintHelperTargetRef Target;
	Target.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
	Target.TargetType = EBlueprintHelperTargetType::Function;
	Target.Function = TEXT("ComputeScore");

	FBlueprintHelperGraphBodyReadbackService Service;
	TSharedPtr<FJsonObject> AdapterBoundary;
	FString Error;
	TestTrue(TEXT("function adapter boundary builds"),
		Service.BuildAdapterBoundaryForTarget(Target, AdapterBoundary, Error));
	TestTrue(TEXT("function adapter boundary valid"), AdapterBoundary.IsValid());
	TestEqual(TEXT("function runtime adapter id"),
		AdapterBoundary.IsValid() ? AdapterBoundary->GetStringField(TEXT("runtime_adapter_id")) : FString(),
		FString(TEXT("k2.function_body")));
	TestEqual(TEXT("function adapter graph name"),
		AdapterBoundary.IsValid() ? AdapterBoundary->GetStringField(TEXT("graph_name")) : FString(),
		FString(TEXT("ComputeScore")));
	TestTrue(TEXT("function entry boundary projected"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::BoundaryArrayContainsNodeRef(
			AdapterBoundary,
			TEXT("entry_boundaries"),
			TEXT("FunctionEntry")));
	TestTrue(TEXT("function result boundary projected"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::BoundaryArrayContainsNodeRef(
			AdapterBoundary,
			TEXT("exit_boundaries"),
			TEXT("FunctionResult")));
	TestTrue(TEXT("function entry folded for compact readback"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::StringArrayContains(
			AdapterBoundary,
			TEXT("folded_boundary_node_refs"),
			TEXT("FunctionEntry")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2MacroBodyAdapterExtractsTunnelBoundariesTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2Macro.ExtractsTunnelBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2MacroBodyAdapterExtractsTunnelBoundariesTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("MacroAdapter"));
	UEdGraph* MacroGraph =
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddMacroGraphWithTunnelNodes(Blueprint, TEXT("ClampScoreMacro"));
	TestTrue(TEXT("macro graph exists"), MacroGraph != nullptr);

	FBlueprintHelperGraphBodyRequest Request;
	Request.Blueprint = Blueprint;
	Request.AssetPath = Blueprint->GetPathName();
	Request.GraphName = TEXT("ClampScoreMacro");
	Request.ReplaceScope = TEXT("macro_body");
	Request.SelectorKind = TEXT("macro");

	FBlueprintHelperK2MacroBodyAdapter Adapter;
	FBlueprintHelperGraphBodyTarget Target;
	FString Error;
	TestTrue(TEXT("macro target resolves"), Adapter.ResolveTarget(Request, Target, Error));

	const FBlueprintHelperGraphBodyBoundaryModel Boundary = Adapter.BuildBoundaryModel(Target, Request);
	TestEqual(TEXT("body kind"), Boundary.BodyKind, EBlueprintHelperGraphBodyKind::K2MacroBody);
	TestTrue(TEXT("Tunnel entry protected"), Boundary.ProtectedNodeRefs.Contains(TEXT("TunnelEntry")));
	TestTrue(TEXT("Tunnel exit protected"), Boundary.ProtectedNodeRefs.Contains(TEXT("TunnelExit")));
	TestTrue(TEXT("Tunnel entry boundary"), Boundary.EntryNodeRefs.Contains(TEXT("TunnelEntry")));
	TestTrue(TEXT("Tunnel exit boundary"), Boundary.ExitNodeRefs.Contains(TEXT("TunnelExit")));
	TestTrue(TEXT("tunnel pins are semantic sources"), Boundary.SemanticSourceRefs.Contains(TEXT("TunnelEntry.Execute")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyReadbackServiceBuildsMacroAdapterBoundaryTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.ReadbackService.BuildsMacroAdapterBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyReadbackServiceBuildsMacroAdapterBoundaryTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("MacroReadback"));
	UEdGraph* MacroGraph =
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddMacroGraphWithTunnelNodes(Blueprint, TEXT("ClampScoreMacro"));
	TestTrue(TEXT("macro graph exists"), MacroGraph != nullptr);

	FBlueprintHelperTargetRef Target;
	Target.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
	Target.TargetType = EBlueprintHelperTargetType::Graph;
	Target.Graph = TEXT("ClampScoreMacro");

	FBlueprintHelperGraphBodyReadbackService Service;
	TSharedPtr<FJsonObject> AdapterBoundary;
	FString Error;
	TestTrue(TEXT("macro adapter boundary builds"),
		Service.BuildAdapterBoundaryForTarget(Target, AdapterBoundary, Error));
	TestTrue(TEXT("macro adapter boundary valid"), AdapterBoundary.IsValid());
	TestEqual(TEXT("macro runtime adapter id"),
		AdapterBoundary.IsValid() ? AdapterBoundary->GetStringField(TEXT("runtime_adapter_id")) : FString(),
		FString(TEXT("k2.macro_body")));
	TestTrue(TEXT("macro entry boundary projected"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::BoundaryArrayContainsNodeRef(
			AdapterBoundary,
			TEXT("entry_boundaries"),
			TEXT("TunnelEntry")));
	TestTrue(TEXT("macro exit boundary projected"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::BoundaryArrayContainsNodeRef(
			AdapterBoundary,
			TEXT("exit_boundaries"),
			TEXT("TunnelExit")));
	TestTrue(TEXT("macro tunnels stay visible for readback"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::StringArrayContains(
			AdapterBoundary,
			TEXT("visible_boundary_node_refs"),
			TEXT("TunnelEntry")));
	return true;
}

#endif
