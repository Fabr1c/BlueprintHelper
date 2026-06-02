#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutExecPinAnchor
{
public:
	static FVector2D GetPrimaryExecAnchorOffset(const FVector2D& NodeSize, ENodeRole Role);
	static float GetPrimaryExecAnchorOffsetY(const FNodeSnapshot& Node, ENodeRole Role);
};
}
