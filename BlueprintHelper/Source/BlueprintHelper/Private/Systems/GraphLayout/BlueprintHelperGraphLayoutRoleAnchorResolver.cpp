#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h"

namespace BlueprintHelper::GraphLayout
{
static FVector2D GetFallbackOffset(const FRuleSet& RuleSet, ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::VariableInput:
		return FVector2D(-RuleSet.VariableInputOffsetX, RuleSet.InputPinRowSpacing);
	case ENodeRole::OperatorOrCompare:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing * 2.0f);
	case ENodeRole::PureFunction:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing);
	default:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing);
	}
}

FRoleAnchor FRoleAnchorResolver::ResolveDataInputAnchor(const FRuleSet& RuleSet, ENodeRole Role)
{
	FRoleAnchor Anchor;
	Anchor.OffsetFromConsumer = GetFallbackOffset(RuleSet, Role);
	Anchor.bFromEditorCanvas = false;
	return Anchor;
}
}
