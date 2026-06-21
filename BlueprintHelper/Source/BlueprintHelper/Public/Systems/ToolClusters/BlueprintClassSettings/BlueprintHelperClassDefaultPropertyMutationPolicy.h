#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassDefaultMutationTypes.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationResolver.h"

struct BLUEPRINTHELPER_API FBlueprintHelperClassDefaultMutationPolicyDecision
{
	EBlueprintHelperClassDefaultMutationStrategy Strategy =
		EBlueprintHelperClassDefaultMutationStrategy::Blocked;
	FString Code;
	FString Message;
	FString SafeNextAction;
	FBlueprintHelperToolSuggestedRoute SuggestedRoute;
};

class BLUEPRINTHELPER_API FBlueprintHelperClassDefaultPropertyMutationPolicy
{
public:
	FBlueprintHelperClassDefaultMutationPolicyDecision Decide(
		const FBlueprintHelperClassDefaultResolvedMutationTarget& Target,
		const FString& RequestedStrategy) const;

	static FBlueprintHelperToolSuggestedRoute MakeSetterAwareSuggestedRoute(const FString& PropertyPath);

private:
	static bool IsValidMutationStrategy(const FString& RequestedStrategy);
	static bool IsSetterGetterPairSupported(
		const FBlueprintHelperClassDefaultResolvedMutationTarget& Target,
		FString& OutCode,
		FString& OutMessage);
};
