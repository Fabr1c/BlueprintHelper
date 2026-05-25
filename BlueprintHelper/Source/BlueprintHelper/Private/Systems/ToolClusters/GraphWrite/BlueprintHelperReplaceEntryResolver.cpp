#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h"

#include "EdGraph/EdGraphNode.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"

namespace
{
static bool NameMatches(const FString& Candidate, const FString& Expected)
{
	return !Candidate.TrimStartAndEnd().IsEmpty()
		&& Candidate.TrimStartAndEnd().Equals(Expected.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

static bool EntryNameMatchesAny(const TArray<FString>& Candidates, const FString& EntryName)
{
	const FString CleanEntryName = EntryName.TrimStartAndEnd();
	if (CleanEntryName.IsEmpty())
	{
		return true;
	}

	for (const FString& Candidate : Candidates)
	{
		if (NameMatches(Candidate, CleanEntryName))
		{
			return true;
		}
	}
	return false;
}
}

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
		return NodeClass->IsChildOf(UK2Node_FunctionEntry::StaticClass());
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
		return EntryNameMatchesAny({ CustomEvent->CustomFunctionName.ToString() }, Request.EntryName);
	}

	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		return EntryNameMatchesAny(
			{
				EventNode->GetFunctionName().ToString(),
				EventNode->EventReference.GetMemberName().ToString()
			},
			Request.EntryName);
	}

	if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
	{
		return EntryNameMatchesAny(
			{
				FunctionEntry->FunctionReference.GetMemberName().ToString(),
				FunctionEntry->CustomGeneratedFunctionName.ToString()
			},
			Request.EntryName);
	}

	return false;
}
