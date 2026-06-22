#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperK2GraphEntryIdentityResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperExternalBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"
#include "UObject/Package.h"

class FBlueprintHelperK2GraphEntryIdentityResolverTestUtils
{
public:
	static UBlueprint* CreateTransientBlueprint(const FString& Prefix)
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
			TEXT("BlueprintHelperK2GraphEntryIdentityResolverTests"));
		if (Blueprint)
		{
			Blueprint->SetFlags(RF_Transient);
			Blueprint->ClearFlags(RF_Standalone);
		}
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* GetEventGraph(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	}

	static UK2Node_Event* AddEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph || EventName.IsEmpty())
		{
			return nullptr;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
		EventNode->bOverrideFunction = true;
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static UK2Node_CustomEvent* AddCustomEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph || EventName.IsEmpty())
		{
			return nullptr;
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static UEdGraph* AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		if (!Blueprint || FunctionName.IsEmpty())
		{
			return nullptr;
		}

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

	static UEdGraph* CreateTransientK2Graph(UObject* Outer, const FString& GraphName)
	{
		UObject* GraphOuter = Outer ? Outer : GetTransientPackage();
		UEdGraph* Graph = NewObject<UEdGraph>(GraphOuter, FName(*GraphName), RF_Transient);
		if (Graph)
		{
			Graph->Schema = UEdGraphSchema_K2::StaticClass();
		}
		return Graph;
	}

	static UK2Node_Tunnel* AddTunnelNode(
		UEdGraph* Graph,
		const bool bCanHaveInputs,
		const bool bCanHaveOutputs)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_Tunnel* Tunnel = NewObject<UK2Node_Tunnel>(Graph);
		Graph->AddNode(Tunnel, true, false);
		Tunnel->CreateNewGuid();
		Tunnel->bCanHaveInputs = bCanHaveInputs;
		Tunnel->bCanHaveOutputs = bCanHaveOutputs;
		Tunnel->PostPlacedNewNode();
		Tunnel->AllocateDefaultPins();
		return Tunnel;
	}

	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

private:
	static void EnsureFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph || FindFunctionResult(FunctionGraph))
		{
			return;
		}

		FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*FunctionGraph);
		UK2Node_FunctionResult* ResultNode = NodeCreator.CreateNode(true);
		ResultNode->NodePosX = 600;
		ResultNode->NodePosY = 0;
		NodeCreator.Finalize();
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2GraphEntryIdentityTargetTypeTest,
	"BlueprintHelper.GraphWrite.K2EntryIdentity.TargetTypeSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2GraphEntryIdentityTargetTypeTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::CreateTransientBlueprint(
		TEXT("K2EntryIdentityTargetType"));
	UEdGraph* Graph = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::GetEventGraph(Blueprint);
	UK2Node_Event* NativeEvent = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddEventNode(
		Graph,
		TEXT("OnShooterFireStartedInput"));
	UK2Node_CustomEvent* CustomEvent = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddCustomEventNode(
		Graph,
		TEXT("BH_OnShooterFireStartedInput"));

	TestTrue(TEXT("native event exists"), NativeEvent != nullptr);
	TestTrue(TEXT("custom event exists"), CustomEvent != nullptr);
	if (!Graph || !NativeEvent || !CustomEvent)
	{
		return false;
	}

	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity NativeIdentity;
	FBlueprintHelperK2GraphEntryIdentity CustomIdentity;
	TestTrue(TEXT("native event resolves"), Resolver.TryResolveNodeIdentity(NativeEvent, NativeIdentity));
	TestTrue(TEXT("custom event resolves"), Resolver.TryResolveNodeIdentity(CustomEvent, CustomIdentity));
	TestEqual(
		TEXT("native event kind"),
		NativeIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::Event);
	TestEqual(
		TEXT("custom event kind"),
		CustomIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::CustomEvent);

	FBlueprintHelperK2GraphEntryQuery EventQuery;
	EventQuery.TargetType = TEXT("event");
	EventQuery.TargetName = TEXT("OnShooterFireStartedInput");
	EventQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;

	UEdGraphNode* FoundNode = nullptr;
	FBlueprintHelperK2GraphEntryIdentity FoundIdentity;
	FString Error;
	TestTrue(
		TEXT("target_type=event finds native event"),
		Resolver.TryFindEntryNode(Graph, EventQuery, FoundNode, FoundIdentity, Error));
	TestTrue(TEXT("native event node selected"), FoundNode == NativeEvent);
	TestEqual(TEXT("native event stable name"), FoundIdentity.StableName, FString(TEXT("OnShooterFireStartedInput")));
	TestFalse(
		TEXT("target_type=event does not match custom event identity"),
		Resolver.DoesIdentityMatchQuery(CustomIdentity, EventQuery));

	FBlueprintHelperK2GraphEntryQuery CustomEventQuery;
	CustomEventQuery.TargetType = TEXT("custom_event");
	CustomEventQuery.TargetName = TEXT("BH_OnShooterFireStartedInput");
	CustomEventQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;

	FoundNode = nullptr;
	FoundIdentity = FBlueprintHelperK2GraphEntryIdentity();
	Error.Reset();
	TestTrue(
		TEXT("target_type=custom_event finds custom event"),
		Resolver.TryFindEntryNode(Graph, CustomEventQuery, FoundNode, FoundIdentity, Error));
	TestTrue(TEXT("custom event node selected"), FoundNode == CustomEvent);
	TestEqual(TEXT("custom event stable name"), FoundIdentity.StableName, FString(TEXT("BH_OnShooterFireStartedInput")));
	TestFalse(
		TEXT("target_type=custom_event does not match native event identity"),
		Resolver.DoesIdentityMatchQuery(NativeIdentity, CustomEventQuery));

	FBlueprintHelperK2GraphEntryQuery MismatchedEventQuery;
	MismatchedEventQuery.TargetType = TEXT("event");
	MismatchedEventQuery.TargetName = TEXT("BH_OnShooterFireStartedInput");
	MismatchedEventQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;

	FoundNode = nullptr;
	FoundIdentity = FBlueprintHelperK2GraphEntryIdentity();
	Error.Reset();
	TestFalse(
		TEXT("target_type=event does not find a custom event by stable name"),
		Resolver.TryFindEntryNode(Graph, MismatchedEventQuery, FoundNode, FoundIdentity, Error));
	TestEqual(TEXT("not found error code"), Error, FString(TEXT("k2_entry_identity_not_found")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2GraphEntryIdentityExternalBodyAdapterTest,
	"BlueprintHelper.GraphWrite.K2EntryIdentity.ExternalBodyAdapterStrictEventSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2GraphEntryIdentityExternalBodyAdapterTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::CreateTransientBlueprint(
		TEXT("K2EntryIdentityExternalAdapter"));
	UEdGraph* Graph = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::GetEventGraph(Blueprint);
	UK2Node_CustomEvent* CustomEvent = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddCustomEventNode(
		Graph,
		TEXT("BH_OnShooterFireStartedInput"));
	UK2Node_Event* NativeEvent = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddEventNode(
		Graph,
		TEXT("OnShooterFireStartedInput"));

	TestTrue(TEXT("custom event exists"), CustomEvent != nullptr);
	TestTrue(TEXT("native event exists"), NativeEvent != nullptr);
	if (!Blueprint || !Graph || !CustomEvent || !NativeEvent)
	{
		return false;
	}

	FBlueprintHelperExternalBodyAdapter Adapter;
	FBlueprintHelperGraphBodyRequest Request;
	Request.OperationKind = TEXT("read_context");
	Request.TaskSpecStrategy = TEXT("read_context");
	Request.ReplaceScope = TEXT("event");
	Request.GraphName = Graph->GetName();
	Request.EntryName = TEXT("OnShooterFireStartedInput");
	Request.Blueprint = Blueprint;

	FBlueprintHelperGraphBodyTarget Target;
	FString Error;
	TestTrue(TEXT("event read target resolves"), Adapter.ResolveTarget(Request, Target, Error));
	TestEqual(TEXT("one event entry selected"), Target.EntryBoundaryNodes.Num(), 1);
	TestTrue(TEXT("native event selected"), Target.EntryBoundaryNodes.Num() == 1 && Target.EntryBoundaryNodes[0] == NativeEvent);

	FBlueprintHelperTargetRef ReadbackTarget;
	ReadbackTarget.AssetPath = Blueprint->GetPathName();
	ReadbackTarget.TargetType = EBlueprintHelperTargetType::Event;
	ReadbackTarget.Graph = Graph->GetName();
	ReadbackTarget.Event = TEXT("OnShooterFireStartedInput");

	FBlueprintHelperGraphBodyReadbackService ReadbackService;
	TSharedPtr<FJsonObject> AdapterBoundary;
	Error.Reset();
	TestTrue(TEXT("event adapter boundary builds"),
		ReadbackService.BuildAdapterBoundaryForTarget(ReadbackTarget, AdapterBoundary, Error));
	TestTrue(TEXT("event adapter boundary valid"), AdapterBoundary.IsValid());
	const TSharedPtr<FJsonObject>* BodyEntry = nullptr;
	TestTrue(TEXT("adapter boundary has body entry"),
		AdapterBoundary.IsValid()
		&& AdapterBoundary->TryGetObjectField(TEXT("body_entry"), BodyEntry)
		&& BodyEntry
		&& BodyEntry->IsValid());
	if (!BodyEntry || !BodyEntry->IsValid())
	{
		return false;
	}
	FString StableName;
	TestTrue(TEXT("body entry carries stable_name"), (*BodyEntry)->TryGetStringField(TEXT("stable_name"), StableName));
	TestEqual(TEXT("body entry stable native event name"), StableName, FString(TEXT("OnShooterFireStartedInput")));

	Request.EntryName = TEXT("BH_OnShooterFireStartedInput");
	Target = FBlueprintHelperGraphBodyTarget();
	Error.Reset();
	TestTrue(TEXT("read context keeps adapter boundary when event name is custom event"), Adapter.ResolveTarget(Request, Target, Error));
	TestEqual(TEXT("no mismatched entry selected"), Target.EntryBoundaryNodes.Num(), 0);
	TestEqual(TEXT("mismatch status"), Target.BodyEvidenceStatus, FString(TEXT("target_type_mismatch")));
	TestEqual(TEXT("mismatch error"), Target.BodyEvidenceErrorCode, FString(TEXT("k2_entry_identity_target_type_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2GraphEntryIdentityFunctionAndMacroTest,
	"BlueprintHelper.GraphWrite.K2EntryIdentity.FunctionAndMacroBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2GraphEntryIdentityFunctionAndMacroTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::CreateTransientBlueprint(
		TEXT("K2EntryIdentityFunctionMacro"));
	const FString FunctionName = TEXT("ComputeEntryIdentityScore");
	UEdGraph* FunctionGraph = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddFunctionGraph(
		Blueprint,
		FunctionName);
	UK2Node_FunctionEntry* FunctionEntry =
		FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::FindFunctionEntry(FunctionGraph);
	UK2Node_FunctionResult* FunctionResult =
		FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::FindFunctionResult(FunctionGraph);

	TestTrue(TEXT("function entry exists"), FunctionEntry != nullptr);
	TestTrue(TEXT("function result exists"), FunctionResult != nullptr);
	if (!FunctionGraph || !FunctionEntry || !FunctionResult)
	{
		return false;
	}

	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity EntryIdentity;
	FBlueprintHelperK2GraphEntryIdentity ResultIdentity;
	TestTrue(TEXT("function entry resolves"), Resolver.TryResolveNodeIdentity(FunctionEntry, EntryIdentity));
	TestTrue(TEXT("function result resolves"), Resolver.TryResolveNodeIdentity(FunctionResult, ResultIdentity));
	TestEqual(
		TEXT("function entry kind"),
		EntryIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::FunctionEntry);
	TestEqual(
		TEXT("function entry role"),
		EntryIdentity.Role,
		EBlueprintHelperK2GraphBoundaryRole::BodyEntry);
	TestEqual(TEXT("function entry stable graph name"), EntryIdentity.StableName, FunctionName);
	TestEqual(
		TEXT("function result kind"),
		ResultIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::FunctionResult);
	TestEqual(
		TEXT("function result role"),
		ResultIdentity.Role,
		EBlueprintHelperK2GraphBoundaryRole::BodyExit);
	TestEqual(TEXT("function result stable graph name"), ResultIdentity.StableName, FunctionName);

	FBlueprintHelperK2GraphEntryQuery FunctionEntryQuery;
	FunctionEntryQuery.TargetType = TEXT("function");
	FunctionEntryQuery.TargetName = FunctionName;
	FunctionEntryQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	TestTrue(
		TEXT("function query matches entry boundary"),
		Resolver.DoesIdentityMatchQuery(EntryIdentity, FunctionEntryQuery));
	TestFalse(
		TEXT("function entry query does not match result boundary"),
		Resolver.DoesIdentityMatchQuery(ResultIdentity, FunctionEntryQuery));

	FBlueprintHelperK2GraphEntryQuery FunctionResultQuery;
	FunctionResultQuery.TargetType = TEXT("function");
	FunctionResultQuery.TargetName = FunctionName;
	FunctionResultQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyExit;
	TestTrue(
		TEXT("function body-exit query matches function result"),
		Resolver.DoesIdentityMatchQuery(ResultIdentity, FunctionResultQuery));

	UEdGraph* MacroGraph = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::CreateTransientK2Graph(
		Blueprint,
		TEXT("IdentityMacroGraph"));
	UK2Node_Tunnel* MacroEntry = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddTunnelNode(
		MacroGraph,
		/*bCanHaveInputs=*/ false,
		/*bCanHaveOutputs=*/ true);
	UK2Node_Tunnel* MacroExit = FBlueprintHelperK2GraphEntryIdentityResolverTestUtils::AddTunnelNode(
		MacroGraph,
		/*bCanHaveInputs=*/ true,
		/*bCanHaveOutputs=*/ false);

	TestTrue(TEXT("macro entry tunnel exists"), MacroEntry != nullptr);
	TestTrue(TEXT("macro exit tunnel exists"), MacroExit != nullptr);
	if (!MacroGraph || !MacroEntry || !MacroExit)
	{
		return false;
	}

	FBlueprintHelperK2GraphEntryIdentity MacroEntryIdentity;
	FBlueprintHelperK2GraphEntryIdentity MacroExitIdentity;
	TestTrue(TEXT("macro entry resolves"), Resolver.TryResolveNodeIdentity(MacroEntry, MacroEntryIdentity));
	TestTrue(TEXT("macro exit resolves"), Resolver.TryResolveNodeIdentity(MacroExit, MacroExitIdentity));
	TestEqual(
		TEXT("macro entry kind"),
		MacroEntryIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::MacroEntry);
	TestEqual(
		TEXT("macro entry role"),
		MacroEntryIdentity.Role,
		EBlueprintHelperK2GraphBoundaryRole::BodyEntry);
	TestEqual(
		TEXT("macro exit kind"),
		MacroExitIdentity.Kind,
		EBlueprintHelperK2GraphEntryKind::MacroExit);
	TestEqual(
		TEXT("macro exit role"),
		MacroExitIdentity.Role,
		EBlueprintHelperK2GraphBoundaryRole::BodyExit);

	FBlueprintHelperK2GraphEntryQuery MacroEntryQuery;
	MacroEntryQuery.TargetType = TEXT("macro");
	MacroEntryQuery.TargetName = TEXT("IdentityMacroGraph");
	MacroEntryQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	TestTrue(
		TEXT("macro entry query matches entry tunnel"),
		Resolver.DoesIdentityMatchQuery(MacroEntryIdentity, MacroEntryQuery));
	TestFalse(
		TEXT("macro entry query does not match exit tunnel"),
		Resolver.DoesIdentityMatchQuery(MacroExitIdentity, MacroEntryQuery));

	FBlueprintHelperK2GraphEntryQuery MacroExitQuery;
	MacroExitQuery.TargetType = TEXT("macro");
	MacroExitQuery.TargetName = TEXT("IdentityMacroGraph");
	MacroExitQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyExit;
	TestTrue(
		TEXT("macro exit query matches exit tunnel"),
		Resolver.DoesIdentityMatchQuery(MacroExitIdentity, MacroExitQuery));

	return true;
}

#endif
