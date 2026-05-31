#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FRoleAnchor
{
	FVector2D OffsetFromConsumer = FVector2D::ZeroVector;
	bool bFromEditorCanvas = false;
};

class BLUEPRINTHELPER_API FRoleAnchorResolver
{
public:
	static FRoleAnchor ResolveDataInputAnchor(const FRuleSet& RuleSet, ENodeRole Role);
};
}
