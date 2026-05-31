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

	const FVector2D* ExecCenter = RuleSet.EditorCanvasRoleCenters.Find(ENodeRole::ExecNode);
	const FVector2D* RoleCenter = RuleSet.EditorCanvasRoleCenters.Find(Role);
	if (!ExecCenter || !RoleCenter)
	{
		return Anchor;
	}

	const FVector2D RawOffset = *RoleCenter - *ExecCenter;
	if (!FMath::IsFinite(RawOffset.X) || !FMath::IsFinite(RawOffset.Y))
	{
		return Anchor;
	}

	const bool bCanvasOffsetIsLeftAndBelow = RawOffset.X < -1.0f && RawOffset.Y > 1.0f;
	if (!bCanvasOffsetIsLeftAndBelow)
	{
		return Anchor;
	}

	Anchor.OffsetFromConsumer.X = FMath::Clamp(RawOffset.X, -1200.0f, -1.0f);
	Anchor.OffsetFromConsumer.Y = FMath::Clamp(RawOffset.Y, 1.0f, 1200.0f);
	Anchor.bFromEditorCanvas = true;
	return Anchor;
}
}
