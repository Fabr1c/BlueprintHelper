#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"

class UEdGraphNode;
class UClass;

struct FBlueprintHelperReplaceEntryResolveRequest
{
	EBlueprintHelperReplaceScope Scope = EBlueprintHelperReplaceScope::Graph;
	FString EntryName;
	FString EventTaxonomy;
	FString SignatureEvidenceId;
};

class FBlueprintHelperReplaceEntryResolver
{
public:
	static bool MatchesEntryClass(const FBlueprintHelperReplaceEntryResolveRequest& Request, const UClass* NodeClass);
	static bool ShouldPreserveEntryNode(const FBlueprintHelperReplaceEntryResolveRequest& Request, const UClass* NodeClass);
	static bool NodeMatchesEntry(const FBlueprintHelperReplaceEntryResolveRequest& Request, UEdGraphNode* Node);
};
