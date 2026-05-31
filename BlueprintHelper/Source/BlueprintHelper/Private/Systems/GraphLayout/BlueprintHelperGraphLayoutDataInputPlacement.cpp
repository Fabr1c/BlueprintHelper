#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h"

namespace BlueprintHelper::GraphLayout
{
bool FDataInputPlacement::IsDataInputRole(ENodeRole Role)
{
	return Role == ENodeRole::VariableInput ||
		Role == ENodeRole::PureFunction ||
		Role == ENodeRole::OperatorOrCompare;
}

FVector2D FDataInputPlacement::BuildDesiredTarget(
	const FRuleSet& RuleSet,
	const FDataInputPlacementRequest& Request)
{
	const FRoleAnchor Anchor = FRoleAnchorResolver::ResolveDataInputAnchor(RuleSet, Request.SourceRole);
	return FVector2D(
		Request.ConsumerTarget.X + Anchor.OffsetFromConsumer.X,
		Request.ConsumerTarget.Y + Anchor.OffsetFromConsumer.Y + Request.InputOrder * RuleSet.InputPinRowSpacing);
}

const TCHAR* FDataInputPlacement::GetReason(ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::VariableInput:
		return TEXT("node_input_variable_alignment");
	case ENodeRole::OperatorOrCompare:
		return TEXT("node_input_operator_or_compare_alignment");
	case ENodeRole::PureFunction:
		return TEXT("node_input_pure_alignment");
	default:
		return TEXT("node_input_alignment");
	}
}
}
