#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"

namespace BlueprintHelper::GraphLayout
{
TArray<FExecRowAllocation> FGraphLayoutRowAllocationPolicy::Allocate(
	const TArray<FExecRowBudget>& Budgets,
	const FRuleSet& RuleSet)
{
	TArray<FExecRowAllocation> Allocations;
	Allocations.Reserve(Budgets.Num());

	float BaselineY = 0.0f;
	for (const FExecRowBudget& Budget : Budgets)
	{
		FExecRowAllocation& Allocation = Allocations.AddDefaulted_GetRef();
		Allocation.RowId = Budget.RowId;
		Allocation.BaselineY = BaselineY;
		Allocation.Height = FMath::Max(RuleSet.ExecRowSpacing, Budget.MinHeight);

		BaselineY += Allocation.Height + RuleSet.BranchRowPaddingY;
	}

	return Allocations;
}
}
