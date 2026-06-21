#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

class UEdGraph;

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutApplyScheduler
{
public:
	static void Enqueue(
		TWeakObjectPtr<UEdGraph> Graph,
		FLayoutPlan Plan,
		FRuleSet RuleSet);

	static bool Tick(float DeltaSeconds);
	static void ResetForTests();
	static int32 GetPendingPlacementCountForTests();
};
}
