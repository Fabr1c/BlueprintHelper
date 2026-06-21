#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperK2GraphEntryIdentityResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"

class FBlueprintHelperK2GraphEntryIdentityResolverPrivate
{
public:
	static void FillCommonNodeFields(
		const UEdGraphNode* Node,
		FBlueprintHelperK2GraphEntryIdentity& OutIdentity)
	{
		if (!Node)
		{
			return;
		}

		OutIdentity.NodeGuid = Node->NodeGuid.IsValid()
			? Node->NodeGuid.ToString(EGuidFormats::Digits)
			: FString();
		OutIdentity.NodeClass = Node->GetClass()
			? Node->GetClass()->GetPathName()
			: FString();
		OutIdentity.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimStartAndEnd();
		if (const UEdGraph* Graph = Node->GetGraph())
		{
			OutIdentity.GraphName = Graph->GetName();
		}
	}

	static bool HasExecPin(const UEdGraphNode* Node, const EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return false;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin &&
				Pin->Direction == Direction &&
				Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return true;
			}
		}
		return false;
	}

	static bool CanOutputExecFlow(const UK2Node_Tunnel* Tunnel)
	{
		return Tunnel && (Tunnel->bCanHaveOutputs || HasExecPin(Tunnel, EGPD_Output));
	}

	static bool CanInputExecFlow(const UK2Node_Tunnel* Tunnel)
	{
		return Tunnel && (Tunnel->bCanHaveInputs || HasExecPin(Tunnel, EGPD_Input));
	}

	static bool EqualsQueryValue(const FString& Actual, const FString& Expected)
	{
		return Expected.IsEmpty() || Actual.Equals(Expected, ESearchCase::IgnoreCase);
	}

	static FString NormalizeTargetType(const FString& TargetType)
	{
		FString Normalized = TargetType.TrimStartAndEnd().ToLower();
		Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
		return Normalized;
	}

	static bool DoesKindMatchTargetType(
		const EBlueprintHelperK2GraphEntryKind Kind,
		const EBlueprintHelperK2GraphBoundaryRole RequiredRole,
		const FString& TargetType)
	{
		const FString NormalizedTargetType = NormalizeTargetType(TargetType);
		if (NormalizedTargetType.IsEmpty())
		{
			return true;
		}
		if (NormalizedTargetType == TEXT("event"))
		{
			return Kind == EBlueprintHelperK2GraphEntryKind::Event;
		}
		if (NormalizedTargetType == TEXT("custom_event"))
		{
			return Kind == EBlueprintHelperK2GraphEntryKind::CustomEvent;
		}
		if (NormalizedTargetType == TEXT("function"))
		{
			if (RequiredRole == EBlueprintHelperK2GraphBoundaryRole::BodyExit)
			{
				return Kind == EBlueprintHelperK2GraphEntryKind::FunctionResult;
			}
			return Kind == EBlueprintHelperK2GraphEntryKind::FunctionEntry;
		}
		if (NormalizedTargetType == TEXT("macro"))
		{
			if (RequiredRole == EBlueprintHelperK2GraphBoundaryRole::BodyEntry)
			{
				return Kind == EBlueprintHelperK2GraphEntryKind::MacroEntry;
			}
			if (RequiredRole == EBlueprintHelperK2GraphBoundaryRole::BodyExit)
			{
				return Kind == EBlueprintHelperK2GraphEntryKind::MacroExit;
			}
			return Kind == EBlueprintHelperK2GraphEntryKind::MacroEntry ||
				Kind == EBlueprintHelperK2GraphEntryKind::MacroExit;
		}
		return false;
	}

	static bool DoesRoleMatch(
		const EBlueprintHelperK2GraphBoundaryRole Role,
		const EBlueprintHelperK2GraphBoundaryRole RequiredRole)
	{
		return RequiredRole == EBlueprintHelperK2GraphBoundaryRole::Unknown || Role == RequiredRole;
	}

	static FString GraphNameForNode(const UEdGraphNode* Node)
	{
		const UEdGraph* Graph = Node ? Node->GetGraph() : nullptr;
		return Graph ? Graph->GetName() : FString();
	}

	static FString FunctionEntryNameFallback(const UK2Node_FunctionEntry* FunctionEntry)
	{
		if (!FunctionEntry)
		{
			return FString();
		}
		if (!FunctionEntry->CustomGeneratedFunctionName.IsNone())
		{
			return FunctionEntry->CustomGeneratedFunctionName.ToString();
		}
		return FunctionEntry->FunctionReference.GetMemberName().ToString();
	}

	static FString FunctionResultNameFallback(const UK2Node_FunctionResult* FunctionResult)
	{
		if (!FunctionResult)
		{
			return FString();
		}
		return FunctionResult->FunctionReference.GetMemberName().ToString();
	}
};

bool FBlueprintHelperK2GraphEntryIdentityResolver::TryResolveNodeIdentity(
	const UEdGraphNode* Node,
	FBlueprintHelperK2GraphEntryIdentity& OutIdentity) const
{
	OutIdentity = FBlueprintHelperK2GraphEntryIdentity();
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperK2GraphEntryIdentityResolverPrivate::FillCommonNodeFields(Node, OutIdentity);

	if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
	{
		OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::CustomEvent;
		OutIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
		OutIdentity.StableName = CustomEvent->CustomFunctionName.ToString();
		OutIdentity.MemberName = OutIdentity.StableName;
		OutIdentity.FunctionName = OutIdentity.StableName;
		OutIdentity.bValid = !OutIdentity.StableName.IsEmpty();
		return OutIdentity.bValid;
	}

	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::Event;
		OutIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
		OutIdentity.StableName = EventNode->EventReference.GetMemberName().ToString();
		OutIdentity.MemberName = OutIdentity.StableName;
		OutIdentity.FunctionName = EventNode->GetFunctionName().ToString();
		OutIdentity.bValid = !OutIdentity.StableName.IsEmpty();
		return OutIdentity.bValid;
	}

	if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
	{
		OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::FunctionEntry;
		OutIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
		OutIdentity.StableName =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::GraphNameForNode(FunctionEntry);
		if (OutIdentity.StableName.IsEmpty())
		{
			OutIdentity.StableName =
				FBlueprintHelperK2GraphEntryIdentityResolverPrivate::FunctionEntryNameFallback(FunctionEntry);
		}
		OutIdentity.MemberName = FunctionEntry->FunctionReference.GetMemberName().ToString();
		OutIdentity.FunctionName =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::FunctionEntryNameFallback(FunctionEntry);
		OutIdentity.bValid = !OutIdentity.StableName.IsEmpty();
		return OutIdentity.bValid;
	}

	if (const UK2Node_FunctionResult* FunctionResult = Cast<UK2Node_FunctionResult>(Node))
	{
		OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::FunctionResult;
		OutIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyExit;
		OutIdentity.StableName =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::GraphNameForNode(FunctionResult);
		if (OutIdentity.StableName.IsEmpty())
		{
			OutIdentity.StableName =
				FBlueprintHelperK2GraphEntryIdentityResolverPrivate::FunctionResultNameFallback(FunctionResult);
		}
		OutIdentity.MemberName = FunctionResult->FunctionReference.GetMemberName().ToString();
		OutIdentity.FunctionName =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::FunctionResultNameFallback(FunctionResult);
		OutIdentity.bValid = !OutIdentity.StableName.IsEmpty();
		return OutIdentity.bValid;
	}

	if (const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
	{
		const bool bCanOutputExecFlow =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::CanOutputExecFlow(Tunnel);
		const bool bCanInputExecFlow =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::CanInputExecFlow(Tunnel);
		if (!bCanOutputExecFlow && !bCanInputExecFlow)
		{
			return false;
		}

		if (bCanOutputExecFlow)
		{
			OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::MacroEntry;
			OutIdentity.Role = bCanInputExecFlow
				? EBlueprintHelperK2GraphBoundaryRole::ExecBoundary
				: EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
		}
		else
		{
			OutIdentity.Kind = EBlueprintHelperK2GraphEntryKind::MacroExit;
			OutIdentity.Role = EBlueprintHelperK2GraphBoundaryRole::BodyExit;
		}

		OutIdentity.StableName =
			FBlueprintHelperK2GraphEntryIdentityResolverPrivate::GraphNameForNode(Tunnel);
		OutIdentity.bValid = !OutIdentity.StableName.IsEmpty();
		return OutIdentity.bValid;
	}

	return false;
}

bool FBlueprintHelperK2GraphEntryIdentityResolver::DoesIdentityMatchQuery(
	const FBlueprintHelperK2GraphEntryIdentity& Identity,
	const FBlueprintHelperK2GraphEntryQuery& Query) const
{
	if (!Identity.bValid)
	{
		return false;
	}

	if (Query.RequiredKind != EBlueprintHelperK2GraphEntryKind::Unknown &&
		Identity.Kind != Query.RequiredKind)
	{
		return false;
	}

	if (!FBlueprintHelperK2GraphEntryIdentityResolverPrivate::DoesKindMatchTargetType(
		Identity.Kind,
		Query.RequiredRole,
		Query.TargetType))
	{
		return false;
	}

	if (!FBlueprintHelperK2GraphEntryIdentityResolverPrivate::DoesRoleMatch(
		Identity.Role,
		Query.RequiredRole))
	{
		return false;
	}

	if (!FBlueprintHelperK2GraphEntryIdentityResolverPrivate::EqualsQueryValue(
		Identity.GraphName,
		Query.GraphName))
	{
		return false;
	}

	if (!FBlueprintHelperK2GraphEntryIdentityResolverPrivate::EqualsQueryValue(
		Identity.StableName,
		Query.TargetName))
	{
		return false;
	}

	return true;
}

bool FBlueprintHelperK2GraphEntryIdentityResolver::TryFindEntryNode(
	UEdGraph* Graph,
	const FBlueprintHelperK2GraphEntryQuery& Query,
	UEdGraphNode*& OutNode,
	FBlueprintHelperK2GraphEntryIdentity& OutIdentity,
	FString& OutError) const
{
	OutNode = nullptr;
	OutIdentity = FBlueprintHelperK2GraphEntryIdentity();
	OutError.Reset();

	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FBlueprintHelperK2GraphEntryIdentity Identity;
			if (TryResolveNodeIdentity(Node, Identity) && DoesIdentityMatchQuery(Identity, Query))
			{
				OutNode = Node;
				OutIdentity = Identity;
				return true;
			}
		}
	}

	OutError = TEXT("k2_entry_identity_not_found");
	return false;
}
