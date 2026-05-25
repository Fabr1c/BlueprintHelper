#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UBlueprint;
class UEdGraph;

struct FBlueprintHelperMergeCallableFragmentRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	FString Query;
	FString FragmentId;
	FString SourceStatementId;
	FString ActionContextStatementId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FVector2D Location = FVector2D::ZeroVector;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FString> ContextEvidence;
	FString ResolvedStableId;
};

struct FBlueprintHelperMergeCallableFragmentResult
{
	bool bOk = false;
	FString Code;
	FString Message;
	FString ResolvedStableId;
	FBlueprintHelperNodeFragment Fragment;
	UK2Node* PrimaryNode = nullptr;
};

class FBlueprintHelperMergeCallableFragmentService
{
public:
	static FBlueprintHelperMergeCallableFragmentResult ValidateCallable(
		const FBlueprintHelperMergeCallableFragmentRequest& Request);

	static FBlueprintHelperMergeCallableFragmentResult BuildCallableFragment(
		const FBlueprintHelperMergeCallableFragmentRequest& Request);

private:
	static FBlueprintHelperGraphFragmentBuildRequest MakeBuildRequest(
		const FBlueprintHelperMergeCallableFragmentRequest& Request);
};
