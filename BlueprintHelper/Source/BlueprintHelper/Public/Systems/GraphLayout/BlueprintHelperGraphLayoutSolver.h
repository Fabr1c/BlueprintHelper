#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FSolver
{
public:
	static FLayoutPlan Solve(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet);
};
}
