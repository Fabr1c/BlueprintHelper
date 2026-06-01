#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FExecRowBudget
{
	int32 RowId = 0;
	float MinHeight = 0.0f;
};

struct FExecRowAllocation
{
	int32 RowId = 0;
	float BaselineY = 0.0f;
	float Height = 0.0f;
};

class BLUEPRINTHELPER_API FGraphLayoutRowAllocationPolicy
{
public:
	static TArray<FExecRowAllocation> Allocate(const TArray<FExecRowBudget>& Budgets, const FRuleSet& RuleSet);
};
}
