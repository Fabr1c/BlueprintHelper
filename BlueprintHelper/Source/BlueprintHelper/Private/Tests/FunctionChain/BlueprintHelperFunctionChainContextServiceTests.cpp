#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextService.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "UObject/Package.h"

class FBlueprintHelperFunctionChainContextServiceTestsLocalUtils
{
public:
	static FString MakeObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint()
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperFunctionChain/%s"),
			*MakeObjectName(TEXT("Pkg"))));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeObjectName(TEXT("BP_FunctionChain")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperFunctionChainContextServiceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, bool bPure = false)
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

		if (bPure)
		{
			if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
			{
				Entry->SetExtraFlags(Entry->GetExtraFlags() | FUNC_BlueprintPure);
			}
		}
		return FunctionGraph;
	}

	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindOrAddFunctionResult(UEdGraph* Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}

		FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*Graph);
		UK2Node_FunctionResult* Result = NodeCreator.CreateNode(true);
		Result->NodePosX = 600;
		Result->NodePosY = 0;
		NodeCreator.Finalize();
		return Result;
	}

	static void AddBoolReturnPin(UEdGraph* FunctionGraph, const FString& PinName)
	{
		UK2Node_FunctionResult* Result = FindOrAddFunctionResult(FunctionGraph);
		if (!Result)
		{
			return;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Input;
		Result->UserDefinedPins.Add(NewPin);
		Result->ReconstructNode();
	}

	static UEdGraph* FindEventGraph(UBlueprint* Blueprint)
	{
		if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
		{
			return nullptr;
		}
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph && Graph->GetName().Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return Blueprint->UbergraphPages[0];
	}

	static UK2Node_CustomEvent* AddCustomEvent(UEdGraph* Graph, const FString& EventName)
	{
		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static UK2Node_CallFunction* AddFunctionCall(UEdGraph* Graph, UFunction* Function)
	{
		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(CallNode, true, false);
		CallNode->CreateNewGuid();
		CallNode->PostPlacedNewNode();
		CallNode->SetFromFunction(Function);
		CallNode->AllocateDefaultPins();
		return CallNode;
	}

	static UK2Node_IfThenElse* AddBranch(UEdGraph* Graph)
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		Graph->AddNode(BranchNode, true, false);
		BranchNode->CreateNewGuid();
		BranchNode->PostPlacedNewNode();
		BranchNode->AllocateDefaultPins();
		return BranchNode;
	}

	static bool Link(UEdGraph* Graph, UEdGraphPin* A, UEdGraphPin* B)
	{
		const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
		return Schema && A && B ? Schema->TryCreateConnection(A, B) : false;
	}

	static bool HasRef(
		const FBlueprintHelperFunctionChainContextPack& Pack,
		const FString& TargetName,
		const FString& CallKind,
		const FString& Reason)
	{
		for (const FBlueprintHelperFunctionChainLogicRef& Ref : Pack.CustomLogicRefs)
		{
			if (Ref.TargetName.Equals(TargetName, ESearchCase::IgnoreCase) &&
				Ref.CallKind == CallKind &&
				Ref.Reason == Reason)
			{
				return true;
			}
		}
		return false;
	}

	static const FBlueprintHelperFunctionChainLogicRef* FindRef(
		const FBlueprintHelperFunctionChainContextPack& Pack,
		const FString& TargetName)
	{
		for (const FBlueprintHelperFunctionChainLogicRef& Ref : Pack.CustomLogicRefs)
		{
			if (Ref.TargetName.Equals(TargetName, ESearchCase::IgnoreCase))
			{
				return &Ref;
			}
		}
		return nullptr;
	}

	static FString AssetPath(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->GetOutermost()
			? Blueprint->GetOutermost()->GetName()
			: TEXT("");
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceFindsCustomLogicTest,
	"BlueprintHelper.FunctionChain.Context.FindsCustomLogicAndFiltersNative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceFindsCustomLogicTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* FireGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(Blueprint, TEXT("Fire"));
	UEdGraph* CanFireGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(Blueprint, TEXT("CanFire"), true);
	FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddBoolReturnPin(CanFireGraph, TEXT("ReturnValue"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UFunction* FireFunction = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(TEXT("Fire")) : nullptr;
	UFunction* CanFireFunction = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(TEXT("CanFire")) : nullptr;
	TestNotNull(TEXT("Fire function compiled"), FireFunction);
	TestNotNull(TEXT("CanFire function compiled"), CanFireFunction);
	if (!FireFunction || !CanFireFunction)
	{
		return false;
	}

	UK2Node_FunctionEntry* FireEntry = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindFunctionEntry(FireGraph);
	UK2Node_CallFunction* PrintCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(
		FireGraph,
		UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
	TestTrue(TEXT("Fire entry links to PrintString"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			FireGraph,
			FireEntry ? FireEntry->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			PrintCall ? PrintCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));

	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(EventGraph, TEXT("Input_Fire"));
	UK2Node_IfThenElse* BranchNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddBranch(EventGraph);
	UK2Node_CallFunction* FireCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(EventGraph, FireFunction);
	UK2Node_CallFunction* CanFireCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(EventGraph, CanFireFunction);

	TestTrue(TEXT("event links to branch"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			EventGraph,
			EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			BranchNode ? BranchNode->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));
	TestTrue(TEXT("branch links to Fire"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			EventGraph,
			BranchNode ? BranchNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			FireCall ? FireCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));
	TestTrue(TEXT("CanFire links to branch condition"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			EventGraph,
			CanFireCall ? CanFireCall->FindPin(TEXT("ReturnValue")) : nullptr,
			BranchNode ? BranchNode->FindPin(TEXT("Condition")) : nullptr));

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_Fire");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 3;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));

	TestTrue(TEXT("impure custom function returned"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::HasRef(Pack, TEXT("Fire"), TEXT("impure_function"), TEXT("exec_call")));
	TestTrue(TEXT("pure custom function returned as branch condition"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::HasRef(Pack, TEXT("CanFire"), TEXT("pure_function"), TEXT("branch_condition")));
	TestTrue(TEXT("engine native call filtered"), Pack.Summary.FilteredEngineOrTrustedPluginCalls > 0);
	TestEqual(TEXT("returned custom refs matches array count"), Pack.Summary.ReturnedCustomRefs, Pack.CustomLogicRefs.Num());

	const TSharedRef<FJsonObject> Json = Pack.ToJson();
	TestFalse(TEXT("result does not expose target echo"), Json->HasField(TEXT("target")));
	TestFalse(TEXT("result does not expose entry echo"), Json->HasField(TEXT("entry")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceMaxDepthTest,
	"BlueprintHelper.FunctionChain.Context.MaxDepthTruncates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceMaxDepthTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UEdGraph* FireGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(Blueprint, TEXT("Fire"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UFunction* FireFunction = Blueprint && Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->FindFunctionByName(TEXT("Fire"))
		: nullptr;
	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(EventGraph, TEXT("Input_Fire"));
	UK2Node_CallFunction* FireCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(EventGraph, FireFunction);
	FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
		EventGraph,
		EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
		FireCall ? FireCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr);

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_Fire");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 0;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds with max_depth=0"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));
	TestEqual(TEXT("max_depth=0 returns no custom refs"), Pack.CustomLogicRefs.Num(), 0);
	TestTrue(TEXT("max_depth=0 marks truncated"), Pack.Summary.bTruncated);
	TestNotNull(TEXT("FireGraph exists and keeps helper referenced"), FireGraph);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceCrossAssetTest,
	"BlueprintHelper.FunctionChain.Context.ResolvesCrossAssetBlueprintCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceCrossAssetTest::RunTest(const FString& Parameters)
{
	UBlueprint* ControllerBlueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UBlueprint* PawnBlueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UEdGraph* TryInteractGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(
		PawnBlueprint,
		TEXT("TryInteract"));
	FKismetEditorUtilities::CompileBlueprint(PawnBlueprint);

	UFunction* TryInteractFunction = PawnBlueprint && PawnBlueprint->GeneratedClass
		? PawnBlueprint->GeneratedClass->FindFunctionByName(TEXT("TryInteract"))
		: nullptr;
	TestNotNull(TEXT("TryInteract function compiled on target Blueprint"), TryInteractFunction);
	if (!ControllerBlueprint || !TryInteractFunction)
	{
		return false;
	}

	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(ControllerBlueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(
		EventGraph,
		TEXT("Input_Interact"));
	UK2Node_CallFunction* TryInteractCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(
		EventGraph,
		TryInteractFunction);
	TestTrue(TEXT("controller event links to cross-asset Blueprint function"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			EventGraph,
			EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			TryInteractCall ? TryInteractCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = ControllerBlueprint->GetPathName();
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_Interact");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 3;
	Request.bExpandCrossAsset = true;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds for cross-asset call"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));

	const FBlueprintHelperFunctionChainLogicRef* TryInteractRef =
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindRef(Pack, TEXT("TryInteract"));
	TestTrue(TEXT("cross-asset Blueprint function returned"), TryInteractRef != nullptr);
	if (TryInteractRef)
	{
		TestEqual(TEXT("cross-asset ref points at called Blueprint asset"),
			TryInteractRef->AssetPath,
			FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AssetPath(PawnBlueprint));
		TestEqual(TEXT("cross-asset ref stays function typed"), TryInteractRef->TargetType, FString(TEXT("function")));
		TestEqual(TEXT("cross-asset ref graph name"), TryInteractRef->GraphName, TryInteractGraph ? TryInteractGraph->GetName() : FString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceCycleTest,
	"BlueprintHelper.FunctionChain.Context.CountsRecursiveCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceCycleTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UEdGraph* GraphA = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(Blueprint, TEXT("LoopA"));
	UEdGraph* GraphB = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionGraph(Blueprint, TEXT("LoopB"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UFunction* LoopAFunction = Blueprint && Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->FindFunctionByName(TEXT("LoopA"))
		: nullptr;
	UFunction* LoopBFunction = Blueprint && Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->FindFunctionByName(TEXT("LoopB"))
		: nullptr;
	TestNotNull(TEXT("LoopA function compiled"), LoopAFunction);
	TestNotNull(TEXT("LoopB function compiled"), LoopBFunction);
	if (!LoopAFunction || !LoopBFunction)
	{
		return false;
	}

	UK2Node_FunctionEntry* EntryA = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindFunctionEntry(GraphA);
	UK2Node_FunctionEntry* EntryB = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindFunctionEntry(GraphB);
	UK2Node_CallFunction* CallB = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(GraphA, LoopBFunction);
	UK2Node_CallFunction* CallA = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(GraphB, LoopAFunction);
	TestTrue(TEXT("LoopA links to LoopB"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			GraphA,
			EntryA ? EntryA->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			CallB ? CallB->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));
	TestTrue(TEXT("LoopB links back to LoopA"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			GraphB,
			EntryB ? EntryB->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			CallA ? CallA->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));

	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(EventGraph, TEXT("Input_Loop"));
	UK2Node_CallFunction* CallLoopA = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(EventGraph, LoopAFunction);
	FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
		EventGraph,
		EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
		CallLoopA ? CallLoopA->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr);

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_Loop");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 5;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds for recursive functions"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));
	TestTrue(TEXT("LoopA returned"), FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindRef(Pack, TEXT("LoopA")) != nullptr);
	TestTrue(TEXT("LoopB returned"), FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindRef(Pack, TEXT("LoopB")) != nullptr);
	TestTrue(TEXT("cycle is counted"), Pack.Summary.CycleCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceInterfaceAmbiguousTest,
	"BlueprintHelper.FunctionChain.Context.ReportsInterfaceCallAmbiguous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceInterfaceAmbiguousTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(EventGraph, TEXT("Input_CheckInterface"));
	UFunction* InterfaceFunction = UInterface_AssetUserData::StaticClass()->FindFunctionByName(TEXT("HasAssetUserDataOfClass"));
	TestNotNull(TEXT("native interface test function exists"), InterfaceFunction);
	if (!InterfaceFunction)
	{
		return false;
	}

	UK2Node_CallFunction* InterfaceCall = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(
		EventGraph,
		InterfaceFunction);
	FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
		EventGraph,
		EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
		InterfaceCall ? InterfaceCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr);

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_CheckInterface");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 3;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds for interface call"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));
	TestEqual(TEXT("interface call reports one ambiguous target"), Pack.Summary.AmbiguousCalls, 1);
	TestEqual(TEXT("interface ambiguous issue count"), Pack.Ambiguous.Num(), 1);
	TestEqual(TEXT("interface call does not emit custom refs"), Pack.CustomLogicRefs.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFunctionChainContextServiceProjectNativeTerminalTest,
	"BlueprintHelper.FunctionChain.Context.CountsProjectNativeTerminalCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFunctionChainContextServiceProjectNativeTerminalTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::MakeBlueprint();
	UEdGraph* EventGraph = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::FindEventGraph(Blueprint);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddCustomEvent(
		EventGraph,
		TEXT("Input_ProjectNative"));

	const FString ProjectName = FApp::GetProjectName();
	const FString NativeClassPath = FString::Printf(TEXT("/Script/%s.TemplateCharacter"), *ProjectName);
	UClass* NativeClass = LoadObject<UClass>(nullptr, *NativeClassPath);
	TestNotNull(TEXT("project TemplateCharacter class is available"), NativeClass);
	if (!NativeClass)
	{
		return false;
	}

	UFunction* NativeFunction = NativeClass->FindFunctionByName(TEXT("DoJumpStart"));
	TestNotNull(TEXT("project native BlueprintCallable function is available"), NativeFunction);
	if (!NativeFunction)
	{
		return false;
	}

	UK2Node_CallFunction* NativeCall =
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::AddFunctionCall(EventGraph, NativeFunction);
	TestTrue(TEXT("custom event links to project native call"),
		FBlueprintHelperFunctionChainContextServiceTestsLocalUtils::Link(
			EventGraph,
			EventNode ? EventNode->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr,
			NativeCall ? NativeCall->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));

	FBlueprintHelperFunctionChainContextRequest Request;
	Request.AssetPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
	Request.TargetType = TEXT("custom_event");
	Request.TargetName = TEXT("Input_ProjectNative");
	Request.GraphName = EventGraph ? EventGraph->GetName() : TEXT("EventGraph");
	Request.MaxDepth = 3;

	FBlueprintHelperFunctionChainContextPack Pack;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperFunctionChainContextService Service;
	TestTrue(TEXT("function chain context builds for project native terminal call"),
		Service.TryBuildFunctionChainContext(Request, Pack, ErrorCode, ErrorMessage));
	TestEqual(TEXT("project native terminal call count"), Pack.Summary.ProjectNativeTerminalCalls, 1);
	TestEqual(TEXT("project native terminal call does not emit custom refs"), Pack.CustomLogicRefs.Num(), 0);
	TestEqual(TEXT("project native terminal call is not counted as engine/trusted filtered"), Pack.Summary.FilteredEngineOrTrustedPluginCalls, 0);
	return true;
}

#endif
