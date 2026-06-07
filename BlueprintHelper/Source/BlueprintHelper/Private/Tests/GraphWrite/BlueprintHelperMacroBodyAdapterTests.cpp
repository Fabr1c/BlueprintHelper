#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Systems/SharedServices/Utils/BlueprintHelperBlueprintStructureUtils.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2MacroBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

class FBlueprintHelperGraphBodyAdapterExtractionTestUtils
{
public:
	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		const FString UniqueName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = GetTransientPackage();
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *UniqueName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphBodyAdapterTests"));
		if (Blueprint)
		{
			Blueprint->SetFlags(RF_Transient);
			Blueprint->ClearFlags(RF_Standalone);
		}
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

	static UEdGraph* AddEmptyMacroGraphShell(UBlueprint* Blueprint, const FString& MacroName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*MacroName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (MacroGraph)
		{
			Blueprint->MacroGraphs.Add(MacroGraph);
		}
		return MacroGraph;
	}

	static UBlueprint* LoadOrCreatePersistentNodeGraphBodyFixture()
	{
		const FString PackageName = TEXT("/Game/BlueprintHelper/NodeGraphBody/BP_NodeGraphBodyAdapter");
		const FString AssetName = TEXT("BP_NodeGraphBodyAdapter");
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		if (UBlueprint* ExistingBlueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath))
		{
			return ExistingBlueprint;
		}

		const FBlueprintHelperAssetFactoryService AssetFactoryService;
		const FBlueprintHelperAssetFactoryData FactoryData = AssetFactoryService.CreateAsset(
			PackageName,
			EBlueprintHelperAssetType::BlueprintClass,
			TEXT("Actor"),
			TEXT(""),
			EBlueprintHelperAssetCollisionPolicy::ReuseIfExists,
			false);
		if (!FactoryData.Asset.bCreated && !FactoryData.Collision.bHandled)
		{
			return nullptr;
		}
		return LoadObject<UBlueprint>(nullptr, *ObjectPath);
	}

	static TSharedPtr<FJsonObject> MakePinPayload(const FString& PinName, const FString& PinCategory)
	{
		TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
		Pin->SetStringField(TEXT("name"), PinName);
		TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
		PinType->SetStringField(TEXT("category"), PinCategory);
		Pin->SetObjectField(TEXT("pin_type"), PinType);
		return Pin;
	}

	static TArray<TSharedPtr<FJsonValue>> MakePinArray(const TArray<TPair<FString, FString>>& Pins)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const TPair<FString, FString>& Pin : Pins)
		{
			Values.Add(MakeShared<FJsonValueObject>(MakePinPayload(Pin.Key, Pin.Value)));
		}
		return Values;
	}

	static bool EnsurePersistentFunctionSignature(UBlueprint* Blueprint, FString& OutError)
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("name"), TEXT("ComputeNodeGraphScore"));
		Payload->SetArrayField(TEXT("inputs"), MakePinArray({{TEXT("InputScore"), TEXT("int")}}));
		Payload->SetArrayField(TEXT("outputs"), MakePinArray({{TEXT("ReturnValue"), TEXT("int")}}));
		return UBlueprintHelperBlueprintStructureUtils::AddFunctionGraphDirect(Blueprint, Payload, OutError);
	}

	static bool EnsurePersistentMacroGraph(UBlueprint* Blueprint, FString& OutError)
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("name"), TEXT("ClampScoreMacro"));
		Payload->SetStringField(TEXT("graph_type"), TEXT("Macro"));
		Payload->SetArrayField(TEXT("inputs"), MakePinArray({{TEXT("Execute"), TEXT("exec")}}));
		Payload->SetArrayField(TEXT("outputs"), MakePinArray({{TEXT("Then"), TEXT("exec")}}));
		return UBlueprintHelperBlueprintStructureUtils::AddMacroGraphDirect(Blueprint, Payload, OutError);
	}

	static bool SavePersistentFixture(UBlueprint* Blueprint, FString& OutError)
	{
		if (!Blueprint)
		{
			OutError = TEXT("fixture save failed: Blueprint is null.");
			return false;
		}

		UPackage* Package = Blueprint->GetOutermost();
		if (!Package)
		{
			OutError = TEXT("fixture save failed: package is null.");
			return false;
		}

		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
		{
			OutError = FString::Printf(TEXT("fixture save failed: %s"), *Filename);
			return false;
		}
		return true;
	}

	static bool MacroGraphHasTunnelExecPin(
		UBlueprint* Blueprint,
		const FString& MacroName,
		const bool bEntryTunnel,
		const EEdGraphPinDirection Direction)
	{
		if (!Blueprint)
		{
			return false;
		}

		UEdGraph* MacroGraph = nullptr;
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (Graph && Graph->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
			{
				MacroGraph = Graph;
				break;
			}
		}
		if (!MacroGraph)
		{
			return false;
		}

		for (UEdGraphNode* Node : MacroGraph->Nodes)
		{
			const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node);
			if (!Tunnel)
			{
				continue;
			}
			if (bEntryTunnel && !FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelEntry(Tunnel))
			{
				continue;
			}
			if (!bEntryTunnel && !FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelExit(Tunnel))
			{
				continue;
			}
			return FBlueprintHelperK2GraphBodyAdapterUtils::HasExecPin(Tunnel, Direction);
		}
		return false;
	}

	static bool MacroGraphEntryExitExecLinked(UBlueprint* Blueprint, const FString& MacroName)
	{
		UEdGraph* MacroGraph = FindMacroGraph(Blueprint, MacroName);
		if (!MacroGraph)
		{
			return false;
		}

		UEdGraphPin* EntryExecOut = nullptr;
		UEdGraphPin* ExitExecIn = nullptr;
		for (UEdGraphNode* Node : MacroGraph->Nodes)
		{
			UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node);
			if (!Tunnel)
			{
				continue;
			}

			if (!EntryExecOut && FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelEntry(Tunnel))
			{
				EntryExecOut = FindFirstExecPin(Tunnel, EGPD_Output);
			}
			if (!ExitExecIn && FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelExit(Tunnel))
			{
				ExitExecIn = FindFirstExecPin(Tunnel, EGPD_Input);
			}
		}

		return EntryExecOut &&
			ExitExecIn &&
			EntryExecOut->LinkedTo.Contains(ExitExecIn) &&
			ExitExecIn->LinkedTo.Contains(EntryExecOut);
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
	static UEdGraph* FindMacroGraph(UBlueprint* Blueprint, const FString& MacroName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (Graph && Graph->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, const EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

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
	FBlueprintHelperNodeGraphBodyAdapterE2EFixturePreparesAssetTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.E2EFixture.PrepareNodeGraphBodyAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperNodeGraphBodyAdapterE2EFixturePreparesAssetTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::LoadOrCreatePersistentNodeGraphBodyFixture();
	TestTrue(TEXT("fixture blueprint exists"), Blueprint != nullptr);
	if (!Blueprint)
	{
		return false;
	}

	FString Error;
	TestTrue(
		TEXT("fixture function signature exists"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::EnsurePersistentFunctionSignature(Blueprint, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("function fixture message: %s"), *Error));
	}

	Error.Reset();
	TestTrue(
		TEXT("fixture macro graph exists"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::EnsurePersistentMacroGraph(Blueprint, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("macro fixture message: %s"), *Error));
	}
	TestTrue(
		TEXT("fixture macro entry has exec output"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphHasTunnelExecPin(
			Blueprint,
			TEXT("ClampScoreMacro"),
			true,
			EGPD_Output));
	TestTrue(
		TEXT("fixture macro exit has exec input"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphHasTunnelExecPin(
			Blueprint,
			TEXT("ClampScoreMacro"),
			false,
			EGPD_Input));
	TestTrue(
		TEXT("fixture macro entry exec links to exit exec"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphEntryExitExecLinked(
			Blueprint,
			TEXT("ClampScoreMacro")));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Error.Reset();
	TestTrue(
		TEXT("fixture blueprint saved"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::SavePersistentFixture(Blueprint, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAddMacroGraphDirectConnectsDefaultTunnelExecTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2Macro.AddMacroGraphDirectConnectsDefaultTunnelExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAddMacroGraphDirectConnectsDefaultTunnelExecTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("MacroDirect"));
	TestTrue(TEXT("blueprint exists"), Blueprint != nullptr);
	if (!Blueprint)
	{
		return false;
	}

	FString Error;
	TestTrue(
		TEXT("macro graph direct add succeeds"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::EnsurePersistentMacroGraph(Blueprint, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}
	TestTrue(
		TEXT("new macro entry exec links directly to exit exec"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphEntryExitExecLinked(
			Blueprint,
			TEXT("ClampScoreMacro")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAddMacroGraphDirectRepairsEmptyShellTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2Macro.AddMacroGraphDirectRepairsEmptyShell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAddMacroGraphDirectRepairsEmptyShellTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("MacroShell"));
	TestTrue(TEXT("blueprint exists"), Blueprint != nullptr);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* Shell = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddEmptyMacroGraphShell(
		Blueprint,
		TEXT("ClampScoreMacro"));
	TestTrue(TEXT("empty macro shell exists"), Shell != nullptr);
	TestFalse(
		TEXT("empty shell initially has no tunnel exec connection"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphEntryExitExecLinked(
			Blueprint,
			TEXT("ClampScoreMacro")));

	FString Error;
	TestTrue(
		TEXT("macro graph direct add repairs empty shell"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::EnsurePersistentMacroGraph(Blueprint, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}
	TestTrue(
		TEXT("repaired macro entry has exec output"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphHasTunnelExecPin(
			Blueprint,
			TEXT("ClampScoreMacro"),
			true,
			EGPD_Output));
	TestTrue(
		TEXT("repaired macro exit has exec input"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphHasTunnelExecPin(
			Blueprint,
			TEXT("ClampScoreMacro"),
			false,
			EGPD_Input));
	TestTrue(
		TEXT("repaired macro entry exec links directly to exit exec"),
		FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MacroGraphEntryExitExecLinked(
			Blueprint,
			TEXT("ClampScoreMacro")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyReadbackServiceUsesRegistryTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.ReadbackService.UsesRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyReadbackServiceUsesRegistryTest::RunTest(const FString&)
{
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.cpp"));
	FString Source;
	TestTrue(TEXT("readback service source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestTrue(TEXT("readback service uses adapter resolver"), Source.Contains(TEXT("FBlueprintHelperGraphBodyAdapterResolver")));

	const TArray<FString> ForbiddenTokens =
	{
		TEXT("Target.TargetType =="),
		TEXT("FBlueprintHelperK2FunctionBodyAdapter"),
		TEXT("FBlueprintHelperK2MacroBodyAdapter"),
		TEXT("FBlueprintHelperK2EventBodyAdapter"),
		TEXT("FBlueprintHelperK2CustomEventBodyAdapter"),
		TEXT("Ref.Equals(TEXT(\"FunctionResult\")"),
		TEXT("Ref.Equals(TEXT(\"TunnelEntry\")"),
		TEXT("Ref.Equals(TEXT(\"TunnelExit\")")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		TestFalse(
			FString::Printf(TEXT("readback service does not contain %s"), *Token),
			Source.Contains(Token));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyAdapterTemporaryFixturesAreTransientTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.TestAssets.TemporaryFixturesAreTransient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyAdapterTemporaryFixturesAreTransientTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphBodyAdapterExtractionTestUtils::MakeActorBlueprint(TEXT("TransientFixture"));
	TestTrue(TEXT("temporary fixture blueprint exists"), Blueprint != nullptr);
	if (!Blueprint)
	{
		return false;
	}

	UPackage* Package = Blueprint->GetOutermost();
	TestTrue(TEXT("temporary fixture package exists"), Package != nullptr);
	TestFalse(
		TEXT("temporary fixture package is not under /Game content"),
		Package && Package->GetName().StartsWith(TEXT("/Game/")));

	FBlueprintHelperGraphBodyAdapterExtractionTestUtils::AddFunctionGraph(Blueprint, TEXT("ComputeScore"));
	TestFalse(
		TEXT("temporary fixture remains outside /Game after graph mutation"),
		Package && Package->GetName().StartsWith(TEXT("/Game/")));
	if (Package)
	{
		Package->SetDirtyFlag(false);
	}
	return true;
}

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
	FBlueprintHelperGraphBodyAdapterResolverMapsMacroReplaceScopeTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2Macro.ResolverMapsReplaceScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyAdapterResolverMapsMacroReplaceScopeTest::RunTest(const FString&)
{
	EBlueprintHelperReplaceScope Scope = EBlueprintHelperReplaceScope::Graph;
	TestTrue(TEXT("macro_body scope parses"), ParseReplaceScope(TEXT("macro_body"), Scope));
	TestEqual(TEXT("macro_body scope enum"), Scope, EBlueprintHelperReplaceScope::MacroBody);
	TestEqual(
		TEXT("macro_body scope string"),
		FString(ReplaceScopeToString(Scope)),
		FString(TEXT("macro_body")));
	TestEqual(
		TEXT("macro_body runtime adapter id"),
		FBlueprintHelperGraphBodyAdapterResolver::RuntimeAdapterIdForReplaceScope(Scope),
		FString(TEXT("k2.macro_body")));

	TUniquePtr<IBlueprintHelperGraphBodyAdapter> Adapter;
	FString Error;
	TestTrue(
		TEXT("macro_body scope creates adapter"),
		FBlueprintHelperGraphBodyAdapterResolver::TryCreateForReplaceScope(Scope, Adapter, Error));
	TestTrue(TEXT("macro_body adapter valid"), Adapter.IsValid());
	TestEqual(
		TEXT("macro_body adapter id"),
		Adapter.IsValid() ? Adapter->GetAdapterId() : FString(),
		FString(TEXT("k2.macro_body")));
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
