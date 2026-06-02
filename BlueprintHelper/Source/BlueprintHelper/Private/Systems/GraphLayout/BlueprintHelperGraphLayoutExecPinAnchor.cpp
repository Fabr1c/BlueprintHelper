#include "Systems/GraphLayout/BlueprintHelperGraphLayoutExecPinAnchor.h"

namespace BlueprintHelper::GraphLayout
{
FVector2D FGraphLayoutExecPinAnchor::GetPrimaryExecAnchorOffset(
	const FVector2D& NodeSize,
	const ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::EventEntry:
		return FVector2D(FMath::Max(0.0f, NodeSize.X - 18.0f), 62.5f);
	case ENodeRole::ExecNode:
	case ENodeRole::BranchControl:
	case ENodeRole::AsyncNode:
	case ENodeRole::DelegateNode:
		return FVector2D(16.0f, 48.0f);
	case ENodeRole::PureFunction:
	case ENodeRole::OperatorOrCompare:
	case ENodeRole::VariableInput:
	case ENodeRole::Comment:
	case ENodeRole::Unknown:
	default:
		return NodeSize * 0.5f;
	}
}

float FGraphLayoutExecPinAnchor::GetPrimaryExecAnchorOffsetY(
	const FNodeSnapshot& Node,
	const ENodeRole Role)
{
	return GetPrimaryExecAnchorOffset(Node.Size, Role).Y;
}
}
