#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

#include "EdGraph/EdGraphNode.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"

bool FBlueprintHelperReplaceEntryResolver::MatchesEntryClass(
	const FBlueprintHelperReplaceEntryResolveRequest& Request,
	const UClass* NodeClass)
{
	if (!NodeClass)
	{
		return false;
	}

	if (Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody)
	{
		return NodeClass->IsChildOf(UK2Node_CustomEvent::StaticClass());
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::EventBody)
	{
		return NodeClass->IsChildOf(UK2Node_Event::StaticClass())
			&& !NodeClass->IsChildOf(UK2Node_CustomEvent::StaticClass());
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		return FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntryNodeClass(NodeClass);
	}
	return false;
}

bool FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(
	const FBlueprintHelperReplaceEntryResolveRequest& Request,
	const UClass* NodeClass)
{
	if (!NodeClass)
	{
		return false;
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::Graph)
	{
		return NodeClass->IsChildOf(UK2Node_Event::StaticClass());
	}
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody &&
		FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResultNodeClass(NodeClass))
	{
		return true;
	}
	return MatchesEntryClass(Request, NodeClass);
}

bool FBlueprintHelperReplaceEntryResolver::NodeMatchesEntry(
	const FBlueprintHelperReplaceEntryResolveRequest& Request,
	UEdGraphNode* Node)
{
	if (!Node || !MatchesEntryClass(Request, Node->GetClass()))
	{
		return false;
	}

	if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
	{
		return UGraphWriteCoreUtils::EntryNameMatchesAny({ CustomEvent->CustomFunctionName.ToString() }, Request.EntryName);
	}

	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		return UGraphWriteCoreUtils::EntryNameMatchesAny(
			{
				EventNode->GetFunctionName().ToString(),
				EventNode->EventReference.GetMemberName().ToString()
			},
			Request.EntryName);
	}

	if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
	{
		return UGraphWriteCoreUtils::EntryNameMatchesAny(
			{
				FunctionEntry->FunctionReference.GetMemberName().ToString(),
				FunctionEntry->CustomGeneratedFunctionName.ToString()
			},
			Request.EntryName);
	}

	return false;
}
