#include "Shared/FunctionChain/Utils/BlueprintHelperFunctionChainTraversalUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "Misc/App.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace BlueprintHelperFunctionChain
{
struct FResolvedEntry
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	TArray<UEdGraphNode*> EntryNodes;
	FString AssetPath;
	FString TargetType;
	FString TargetName;
	FString GraphName;
};

struct FTraversalState
{
	const FBlueprintHelperFunctionChainContextRequest& Request;
	FBlueprintHelperFunctionChainContextPack& Context;
	TSet<FString> ExpandedEntries;
	TSet<FString> ReturnedRefs;
	int32 NextOrder = 1;

	explicit FTraversalState(
		const FBlueprintHelperFunctionChainContextRequest& InRequest,
		FBlueprintHelperFunctionChainContextPack& InContext)
		: Request(InRequest)
		, Context(InContext)
	{
	}
};

static FString BlueprintAssetPath(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->GetOutermost()
		? Blueprint->GetOutermost()->GetName()
		: TEXT("");
}

static bool IsSameAssetPath(const FString& A, const FString& B)
{
	return A.Equals(B, ESearchCase::IgnoreCase);
}

static FString NormalizeTargetType(const FString& TargetType)
{
	if (TargetType.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		return TEXT("custom_event");
	}
	if (TargetType.Equals(TEXT("event"), ESearchCase::IgnoreCase))
	{
		return TEXT("event");
	}
	return TEXT("function");
}

static FString EntryKey(const FResolvedEntry& Entry)
{
	return FString::Printf(
		TEXT("%s|%s|%s|%s"),
		*Entry.AssetPath.ToLower(),
		*Entry.TargetType.ToLower(),
		*Entry.TargetName.ToLower(),
		*Entry.GraphName.ToLower());
}

static UEdGraph* FindGraphByName(const TArray<TObjectPtr<UEdGraph>>& Graphs, const FString& GraphName)
{
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}
	return nullptr;
}

static UEdGraph* FindGraphByName(const TArray<UEdGraph*>& Graphs, const FString& GraphName)
{
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}
	return nullptr;
}

static UEdGraph* FindDefaultEventGraph(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return nullptr;
	}
	if (UEdGraph* EventGraph = FindGraphByName(Blueprint->UbergraphPages, TEXT("EventGraph")))
	{
		return EventGraph;
	}
	return Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static UK2Node_CustomEvent* FindCustomEvent(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
		if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
		{
			return CustomEvent;
		}
	}
	return nullptr;
}

static UK2Node_Event* FindEventNode(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (!EventNode)
		{
			continue;
		}
		const FName MemberName = EventNode->EventReference.GetMemberName();
		if (MemberName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
		{
			return EventNode;
		}
	}
	return nullptr;
}

static TArray<UEdGraphNode*> FindFunctionEntryNodes(UEdGraph* Graph)
{
	TArray<UEdGraphNode*> Nodes;
	if (!Graph)
	{
		return Nodes;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			Nodes.Add(Node);
		}
	}
	return Nodes;
}

static bool ResolveEntry(
	UBlueprint* Blueprint,
	const FString& TargetType,
	const FString& TargetName,
	const FString& GraphName,
	FResolvedEntry& OutEntry)
{
	OutEntry = FResolvedEntry();
	OutEntry.Blueprint = Blueprint;
	OutEntry.AssetPath = BlueprintAssetPath(Blueprint);
	OutEntry.TargetType = NormalizeTargetType(TargetType);
	OutEntry.TargetName = TargetName;

	if (!Blueprint || TargetName.IsEmpty())
	{
		return false;
	}

	if (OutEntry.TargetType == TEXT("function"))
	{
		UEdGraph* FunctionGraph = !GraphName.IsEmpty()
			? FindGraphByName(Blueprint->FunctionGraphs, GraphName)
			: FindGraphByName(Blueprint->FunctionGraphs, TargetName);
		if (!FunctionGraph)
		{
			return false;
		}
		OutEntry.Graph = FunctionGraph;
		OutEntry.GraphName = FunctionGraph->GetName();
		OutEntry.EntryNodes = FindFunctionEntryNodes(FunctionGraph);
		if (OutEntry.EntryNodes.Num() == 0)
		{
			for (UEdGraphNode* Node : FunctionGraph->Nodes)
			{
				if (Node)
				{
					OutEntry.EntryNodes.Add(Node);
				}
			}
		}
		return true;
	}

	UEdGraph* EventGraph = !GraphName.IsEmpty()
		? FindGraphByName(Blueprint->UbergraphPages, GraphName)
		: FindDefaultEventGraph(Blueprint);
	if (!EventGraph)
	{
		return false;
	}
	OutEntry.Graph = EventGraph;
	OutEntry.GraphName = EventGraph->GetName();

	if (OutEntry.TargetType == TEXT("custom_event"))
	{
		if (UK2Node_CustomEvent* CustomEvent = FindCustomEvent(EventGraph, TargetName))
		{
			OutEntry.EntryNodes.Add(CustomEvent);
			return true;
		}
		return false;
	}

	if (UK2Node_Event* EventNode = FindEventNode(EventGraph, TargetName))
	{
		OutEntry.EntryNodes.Add(EventNode);
		return true;
	}
	if (UK2Node_CustomEvent* CustomEvent = FindCustomEvent(EventGraph, TargetName))
	{
		OutEntry.TargetType = TEXT("custom_event");
		OutEntry.EntryNodes.Add(CustomEvent);
		return true;
	}
	return false;
}

static bool IsExecPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

static bool IsInputDataPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->Direction == EGPD_Input && !IsExecPin(Pin);
}

static void CollectExecReachableNodes(const FResolvedEntry& Entry, TArray<UEdGraphNode*>& OutNodes)
{
	TSet<UEdGraphNode*> Seen;
	TArray<UEdGraphNode*> Queue = Entry.EntryNodes;

	while (Queue.Num() > 0)
	{
		UEdGraphNode* Node = Queue[0];
		Queue.RemoveAt(0);
		if (!Node || Seen.Contains(Node))
		{
			continue;
		}
		Seen.Add(Node);
		OutNodes.Add(Node);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
			{
				continue;
			}
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode() && !Seen.Contains(LinkedPin->GetOwningNode()))
				{
					Queue.Add(LinkedPin->GetOwningNode());
				}
			}
		}
	}

	if (OutNodes.Num() == 0 && Entry.Graph)
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (Node)
			{
				OutNodes.Add(Node);
			}
		}
	}
}

static FString ReasonForInputPin(UEdGraphNode* Consumer, UEdGraphPin* Pin)
{
	if (Cast<UK2Node_IfThenElse>(Consumer))
	{
		return TEXT("branch_condition");
	}
	if (Cast<UK2Node_FunctionResult>(Consumer))
	{
		return TEXT("return_value_source");
	}
	if (Consumer && Consumer->GetClass()->GetName().Contains(TEXT("VariableSet")))
	{
		return TEXT("set_value_source");
	}
	return TEXT("argument_source");
}

static void CollectDataDependencyNodes(
	UEdGraphNode* Consumer,
	TSet<UEdGraphNode*>& Seen,
	TMap<UEdGraphNode*, FString>& Reasons)
{
	if (!Consumer)
	{
		return;
	}

	for (UEdGraphPin* Pin : Consumer->Pins)
	{
		if (!IsInputDataPin(Pin))
		{
			continue;
		}

		const FString Reason = ReasonForInputPin(Consumer, Pin);
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (!SourceNode || Seen.Contains(SourceNode))
			{
				continue;
			}
			Seen.Add(SourceNode);
			Reasons.Add(SourceNode, Reason);
			CollectDataDependencyNodes(SourceNode, Seen, Reasons);
		}
	}
}

static bool IsTrustedNativePackage(const FString& PackageName)
{
	return PackageName.StartsWith(TEXT("/Script/Engine")) ||
		PackageName.StartsWith(TEXT("/Script/CoreUObject")) ||
		PackageName.StartsWith(TEXT("/Script/UMG")) ||
		PackageName.StartsWith(TEXT("/Script/GameplayTags")) ||
		PackageName.StartsWith(TEXT("/Script/Kismet")) ||
		PackageName.StartsWith(TEXT("/Script/BlueprintHelper"));
}

static bool IsProjectNativePackage(const FString& PackageName)
{
	const FString ProjectName = FApp::GetProjectName();
	return !ProjectName.IsEmpty() &&
		PackageName.Equals(FString::Printf(TEXT("/Script/%s"), *ProjectName), ESearchCase::IgnoreCase);
}

static bool IsInterfaceFunction(UFunction* Function)
{
	UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass && OwnerClass->HasAnyClassFlags(CLASS_Interface);
}

static UBlueprint* BlueprintFromFunction(UFunction* Function)
{
	UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass ? Cast<UBlueprint>(OwnerClass->ClassGeneratedBy) : nullptr;
}

static bool IsProjectBlueprint(UBlueprint* Blueprint)
{
	const FString AssetPath = BlueprintAssetPath(Blueprint);
	return AssetPath.StartsWith(TEXT("/Game/"));
}

static void AddIssue(
	TArray<FBlueprintHelperFunctionChainIssue>& Issues,
	const FString& Code,
	const FString& Message,
	UEdGraphNode* Node)
{
	FBlueprintHelperFunctionChainIssue Issue;
	Issue.Code = Code;
	Issue.Message = Message;
	if (Node && Node->GetGraph())
	{
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Node->GetGraph()))
		{
			Issue.AssetPath = BlueprintAssetPath(Blueprint);
		}
		Issue.GraphName = Node->GetGraph()->GetName();
	}
	Issues.Add(Issue);
}

static FString FindCustomEventGraphName(UBlueprint* Blueprint, const FString& EventName)
{
	if (!Blueprint)
	{
		return TEXT("");
	}
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (FindCustomEvent(Graph, EventName))
		{
			return Graph->GetName();
		}
	}
	return TEXT("");
}

static bool ResolveFunctionTargetEntry(
	UFunction* Function,
	UBlueprint* OwnerBlueprint,
	FString& OutTargetType,
	FString& OutGraphName)
{
	if (!Function || !OwnerBlueprint)
	{
		return false;
	}

	const FString FunctionName = Function->GetName();
	if (UEdGraph* FunctionGraph = FindGraphByName(OwnerBlueprint->FunctionGraphs, FunctionName))
	{
		OutTargetType = TEXT("function");
		OutGraphName = FunctionGraph->GetName();
		return true;
	}

	const FString CustomEventGraphName = FindCustomEventGraphName(OwnerBlueprint, FunctionName);
	if (!CustomEventGraphName.IsEmpty())
	{
		OutTargetType = TEXT("custom_event");
		OutGraphName = CustomEventGraphName;
		return true;
	}

	return false;
}

static FString RefKey(
	const FString& AssetPath,
	const FString& TargetType,
	const FString& TargetName,
	const FString& GraphName)
{
	return FString::Printf(
		TEXT("%s|%s|%s|%s"),
		*AssetPath.ToLower(),
		*TargetType.ToLower(),
		*TargetName.ToLower(),
		*GraphName.ToLower());
}

static bool ShouldExpandRef(
	const FTraversalState& State,
	const FBlueprintHelperFunctionChainLogicRef& Ref,
	const FResolvedEntry& ParentEntry,
	int32 Depth)
{
	if (Depth >= State.Request.MaxDepth)
	{
		return false;
	}
	if (!State.Request.bExpandCrossAsset && !IsSameAssetPath(Ref.AssetPath, ParentEntry.AssetPath))
	{
		return false;
	}
	return true;
}

static void TraverseEntry(
	FTraversalState& State,
	const FResolvedEntry& Entry,
	int32 Depth,
	int32 ParentOrder);

static void ProcessCallNode(
	FTraversalState& State,
	const FResolvedEntry& ParentEntry,
	UK2Node_CallFunction* CallNode,
	const FString& Reason,
	int32 Depth,
	int32 ParentOrder)
{
	if (!CallNode)
	{
		return;
	}

	UFunction* Function = CallNode->GetTargetFunction();
	if (!Function)
	{
		State.Context.Summary.UnresolvedCalls++;
		AddIssue(State.Context.Unresolved, TEXT("function_unresolved"), TEXT("CallFunction target could not be resolved."), CallNode);
		return;
	}

	const bool bPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure) || CallNode->IsNodePure();
	if (IsInterfaceFunction(Function))
	{
		State.Context.Summary.AmbiguousCalls++;
		AddIssue(State.Context.Ambiguous, TEXT("interface_target_ambiguous"), TEXT("Interface call target is not statically unique."), CallNode);
		return;
	}

	UBlueprint* OwnerBlueprint = BlueprintFromFunction(Function);
	if (!OwnerBlueprint || !IsProjectBlueprint(OwnerBlueprint))
	{
		const FString PackageName = Function->GetOutermost() ? Function->GetOutermost()->GetName() : TEXT("");
		if (IsProjectNativePackage(PackageName))
		{
			State.Context.Summary.ProjectNativeTerminalCalls++;
			return;
		}

		if (bPure)
		{
			State.Context.Summary.FilteredNativePureCalls++;
		}

		if (IsTrustedNativePackage(PackageName) || OwnerBlueprint)
		{
			State.Context.Summary.FilteredEngineOrTrustedPluginCalls++;
			return;
		}

		State.Context.Summary.FilteredEngineOrTrustedPluginCalls++;
		return;
	}

	FString TargetType;
	FString GraphName;
	if (!ResolveFunctionTargetEntry(Function, OwnerBlueprint, TargetType, GraphName))
	{
		State.Context.Summary.ProjectNativeTerminalCalls++;
		return;
	}

	const int32 RefDepth = Depth + 1;
	if (RefDepth > State.Request.MaxDepth)
	{
		State.Context.Summary.bTruncated = true;
		return;
	}

	FBlueprintHelperFunctionChainLogicRef Ref;
	Ref.Order = State.NextOrder++;
	Ref.Depth = RefDepth;
	Ref.ParentOrder = ParentOrder;
	Ref.AssetPath = BlueprintAssetPath(OwnerBlueprint);
	Ref.TargetType = TargetType;
	Ref.TargetName = Function->GetName();
	Ref.GraphName = GraphName;
	Ref.CallKind = TargetType == TEXT("custom_event")
		? TEXT("custom_event")
		: (bPure ? TEXT("pure_function") : TEXT("impure_function"));
	Ref.Reason = Reason;

	const FString LogicRefKey = RefKey(Ref.AssetPath, Ref.TargetType, Ref.TargetName, Ref.GraphName);
	const bool bAlreadyReturned = State.ReturnedRefs.Contains(LogicRefKey);
	if (!bAlreadyReturned)
	{
		State.ReturnedRefs.Add(LogicRefKey);
		State.Context.CustomLogicRefs.Add(Ref);
		State.Context.Summary.ReturnedCustomRefs = State.Context.CustomLogicRefs.Num();
	}

	if (ShouldExpandRef(State, Ref, ParentEntry, Depth))
	{
		FResolvedEntry ChildEntry;
		if (ResolveEntry(OwnerBlueprint, Ref.TargetType, Ref.TargetName, Ref.GraphName, ChildEntry))
		{
			TraverseEntry(State, ChildEntry, RefDepth, Ref.Order);
		}
	}
	else if (Depth + 1 >= State.Request.MaxDepth)
	{
		State.Context.Summary.bTruncated = true;
	}
}

static void TraverseEntry(
	FTraversalState& State,
	const FResolvedEntry& Entry,
	int32 Depth,
	int32 ParentOrder)
{
	const FString Key = EntryKey(Entry);
	if (State.ExpandedEntries.Contains(Key))
	{
		State.Context.Summary.CycleCount++;
		return;
	}
	State.ExpandedEntries.Add(Key);

	TArray<UEdGraphNode*> ExecNodes;
	CollectExecReachableNodes(Entry, ExecNodes);
	TSet<UEdGraphNode*> ExecSet;
	for (UEdGraphNode* Node : ExecNodes)
	{
		if (Node)
		{
			ExecSet.Add(Node);
		}
	}

	TMap<UEdGraphNode*, FString> DataDependencyReasons;
	if (State.Request.bIncludeDataDependencies)
	{
		TSet<UEdGraphNode*> DataSeen = ExecSet;
		for (UEdGraphNode* Node : ExecNodes)
		{
			CollectDataDependencyNodes(Node, DataSeen, DataDependencyReasons);
		}
	}

	State.Context.Summary.VisitedNodes += ExecNodes.Num() + DataDependencyReasons.Num();

	for (UEdGraphNode* Node : ExecNodes)
	{
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			ProcessCallNode(State, Entry, CallNode, TEXT("exec_call"), Depth, ParentOrder);
		}
	}

	for (const TPair<UEdGraphNode*, FString>& Pair : DataDependencyReasons)
	{
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Pair.Key))
		{
			ProcessCallNode(State, Entry, CallNode, Pair.Value, Depth, ParentOrder);
		}
	}
}
}

bool FBlueprintHelperFunctionChainTraversalUtils::BuildContext(
	UBlueprint* Blueprint,
	const FBlueprintHelperFunctionChainContextRequest& Request,
	FBlueprintHelperFunctionChainContextPack& OutContext,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	using namespace BlueprintHelperFunctionChain;

	FResolvedEntry RootEntry;
	if (!ResolveEntry(Blueprint, Request.TargetType, Request.TargetName, Request.GraphName, RootEntry))
	{
		OutErrorCode = TEXT("target_entry_not_found");
		OutErrorMessage = FString::Printf(
			TEXT("Could not resolve %s '%s' in %s."),
			*Request.TargetType,
			*Request.TargetName,
			*BlueprintAssetPath(Blueprint));
		return false;
	}

	FTraversalState State(Request, OutContext);
	TraverseEntry(State, RootEntry, 0, 0);
	OutContext.Summary.ReturnedCustomRefs = OutContext.CustomLogicRefs.Num();
	return true;
}
