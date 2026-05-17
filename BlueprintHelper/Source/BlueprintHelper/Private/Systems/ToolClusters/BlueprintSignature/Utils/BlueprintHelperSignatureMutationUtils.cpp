// BlueprintHelper signature mutation helpers.

#include "Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureMutationUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"

UBlueprint* FBlueprintHelperSignatureMutationUtils::LoadSignatureBlueprint(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}
	return LoadObject<UBlueprint>(nullptr, *AssetPath);
}

bool FBlueprintHelperSignatureMutationUtils::RemoveEventDispatcherSignatureDirect(
	UBlueprint* Blueprint,
	const FString& DispatcherName,
	bool& bOutRemoved,
	FString& OutError)
{
	bOutRemoved = false;
	if (!Blueprint)
	{
		OutError = TEXT("Target Blueprint is invalid.");
		return false;
	}

	const FName DispatcherFName(*DispatcherName);
	UEdGraph* DelegateSignatureGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(Blueprint, DispatcherFName);
	const bool bHasDispatcherVariable = IsEventDispatcherVariable(Blueprint, DispatcherFName);
	if (!DelegateSignatureGraph && !bHasDispatcherVariable)
	{
		return true;
	}

	if (bHasDispatcherVariable)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, DispatcherFName);
		bOutRemoved = true;
	}
	if (DelegateSignatureGraph)
	{
		FBlueprintEditorUtils::RemoveGraph(Blueprint, DelegateSignatureGraph, EGraphRemoveFlags::Recompile);
		bOutRemoved = true;
	}

	for (TObjectIterator<UK2Node_CreateDelegate> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
	{
		if (IsValid(*It) && IsValid(It->GetGraph()))
		{
			It->HandleAnyChange();
		}
	}
	return true;
}

bool FBlueprintHelperSignatureMutationUtils::RemoveSignatureDirect(
	UBlueprint* Blueprint,
	const FBlueprintHelperRemoveSignatureRequest& Request,
	bool& bOutRemoved,
	FString& OutError)
{
	bOutRemoved = false;
	if (!Blueprint)
	{
		OutError = TEXT("Target Blueprint is invalid.");
		return false;
	}

	if (Request.SignatureKind == TEXT("function") || Request.SignatureKind == TEXT("interface_function"))
	{
		UEdGraph* FunctionGraph = FindGraphByName(Blueprint->FunctionGraphs, Request.SignatureName);
		if (!FunctionGraph)
		{
			return true;
		}
		FBlueprintEditorUtils::RemoveGraph(Blueprint, FunctionGraph, EGraphRemoveFlags::Recompile);
		bOutRemoved = true;
		return true;
	}

	if (Request.SignatureKind == TEXT("custom_event") || Request.SignatureKind == TEXT("interface_event"))
	{
		UEdGraph* EventGraph = FindGraphByName(Blueprint->UbergraphPages, Request.GraphName);
		if (!EventGraph)
		{
			OutError = FString::Printf(TEXT("Blueprint graph not found: %s."), *Request.GraphName);
			return false;
		}
		UK2Node_CustomEvent* EventNode = FindCustomEventInGraph(EventGraph, Request.SignatureName);
		if (!EventNode)
		{
			return true;
		}
		FBlueprintEditorUtils::RemoveNode(Blueprint, EventNode);
		bOutRemoved = true;
		return true;
	}

	if (Request.SignatureKind == TEXT("event_dispatcher"))
	{
		return RemoveEventDispatcherSignatureDirect(Blueprint, Request.SignatureName, bOutRemoved, OutError);
	}

	if (Request.SignatureKind == TEXT("override_event") || Request.SignatureKind == TEXT("native_event"))
	{
		UK2Node_Event* EventNode = FindOverrideEventInBlueprint(Blueprint, Request.SignatureName);
		if (!EventNode)
		{
			return true;
		}
		FBlueprintEditorUtils::RemoveNode(Blueprint, EventNode);
		bOutRemoved = true;
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported signature kind: %s."), *Request.SignatureKind);
	return false;
}

UEdGraph* FBlueprintHelperSignatureMutationUtils::FindGraphByName(
	const TArray<TObjectPtr<UEdGraph>>& Graphs,
	const FString& GraphName)
{
	const FName TargetName(*GraphName);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetFName() == TargetName)
		{
			return Graph;
		}
	}
	return nullptr;
}

UEdGraph* FBlueprintHelperSignatureMutationUtils::FindGraphByName(
	const TArray<UEdGraph*>& Graphs,
	const FString& GraphName)
{
	const FName TargetName(*GraphName);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetFName() == TargetName)
		{
			return Graph;
		}
	}
	return nullptr;
}

UK2Node_CustomEvent* FBlueprintHelperSignatureMutationUtils::FindCustomEventInGraph(
	UEdGraph* Graph,
	const FString& EventName)
{
	if (!Graph)
	{
		return nullptr;
	}

	const FName TargetName(*EventName);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node);
		if (EventNode && EventNode->CustomFunctionName == TargetName)
		{
			return EventNode;
		}
	}
	return nullptr;
}

FName FBlueprintHelperSignatureMutationUtils::ResolveNativeOrOverrideEventName(const FString& InEventName)
{
	if (InEventName.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveBeginPlay"));
	}
	if (InEventName.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveTick"));
	}
	if (InEventName.Equals(TEXT("EndPlay"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveEndPlay"));
	}
	if (InEventName.Equals(TEXT("AnyDamage"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveAnyDamage"));
	}
	if (InEventName.Equals(TEXT("ActorBeginOverlap"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveActorBeginOverlap"));
	}
	if (InEventName.Equals(TEXT("ActorEndOverlap"), ESearchCase::IgnoreCase))
	{
		return FName(TEXT("ReceiveActorEndOverlap"));
	}
	return FName(*InEventName);
}

UFunction* FBlueprintHelperSignatureMutationUtils::ResolveNativeOrOverrideEventDeclarationFunction(UFunction* EventFunction)
{
	if (!EventFunction)
	{
		return nullptr;
	}
	return EventFunction->GetSuperFunction() ? EventFunction->GetSuperFunction() : EventFunction;
}

UClass* FBlueprintHelperSignatureMutationUtils::ResolveNativeOrOverrideEventSignatureClass(
	UFunction* EventFunction,
	UClass* FallbackSignatureClass)
{
	return EventFunction && EventFunction->GetOwnerClass()
		? EventFunction->GetOwnerClass()
		: FallbackSignatureClass;
}

UK2Node_Event* FBlueprintHelperSignatureMutationUtils::FindOverrideEventInBlueprint(
	UBlueprint* Blueprint,
	const FString& EventName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	const FName EventFName = ResolveNativeOrOverrideEventName(EventName);
	UFunction* EventFunction = nullptr;
	UClass* const SignatureClass = FBlueprintEditorUtils::GetOverrideFunctionClass(
		Blueprint,
		EventFName,
		&EventFunction);
	UFunction* const EventDeclarationFunction = ResolveNativeOrOverrideEventDeclarationFunction(EventFunction);
	UClass* const EventSignatureClass = ResolveNativeOrOverrideEventSignatureClass(EventDeclarationFunction, SignatureClass);
	if (EventDeclarationFunction && EventSignatureClass)
	{
		if (UK2Node_Event* ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, EventSignatureClass, EventFName))
		{
			return ExistingEvent;
		}
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
			if (EventNode && EventNode->bOverrideFunction && EventNode->EventReference.GetMemberName() == EventFName)
			{
				return EventNode;
			}
		}
	}
	return nullptr;
}

bool FBlueprintHelperSignatureMutationUtils::IsEventDispatcherVariable(
	UBlueprint* Blueprint,
	const FName DispatcherName)
{
	if (!Blueprint)
	{
		return false;
	}
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarName == DispatcherName &&
			Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			return true;
		}
	}
	return false;
}
