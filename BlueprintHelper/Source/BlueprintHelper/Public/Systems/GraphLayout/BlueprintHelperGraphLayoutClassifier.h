#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FClassifier
{
public:
	static FNodeClassification ClassifyNode(const FNodeSnapshot& Node, const FRuleSet& RuleSet);
	static TArray<FNodeClassification> ClassifyGraph(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet);
};
}
