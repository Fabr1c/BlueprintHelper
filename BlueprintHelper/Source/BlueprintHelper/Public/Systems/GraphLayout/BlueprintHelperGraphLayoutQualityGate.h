#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutQualityIssue
{
	FString Code;
	FString Message;
	FString NodeA;
	FString NodeB;
};

struct FGraphLayoutQualityResult
{
	TArray<FGraphLayoutQualityIssue> Issues;

	bool HasBlockingIssues() const
	{
		return Issues.Num() > 0;
	}
};

class BLUEPRINTHELPER_API FGraphLayoutQualityGate
{
public:
	static FGraphLayoutQualityResult Evaluate(
		const FGraphSnapshot& Snapshot,
		const FLayoutPlan& Plan,
		const FRuleSet& RuleSet);
};
}
