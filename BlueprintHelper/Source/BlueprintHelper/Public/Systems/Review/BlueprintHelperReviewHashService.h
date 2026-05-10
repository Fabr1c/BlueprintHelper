// BlueprintHelper Review target hash helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

class BLUEPRINTHELPER_API FBlueprintHelperReviewHashService
{
public:
	static bool ComputeAtomicTargetHash(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutHash,
		FString& OutError);

	static FString MakeStableHash(const FString& Payload);

private:
	static UBlueprint* LoadBlueprint(const FString& AssetPath, FString& OutError);
	static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName);
	static UEdGraphNode* FindNodeByName(UEdGraph* Graph, const FString& NodeName);
	static FString ExtractAnchorName(const FString& TargetKey, const FString& Prefix);
	static FString ComputeNodeHash(UEdGraphNode* Node);
	static bool ComputeGraphNodeHash(
		UBlueprint* Blueprint,
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutHash,
		FString& OutError);
	static bool ComputeGraphBlockHash(
		UBlueprint* Blueprint,
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutHash,
		FString& OutError);
};
