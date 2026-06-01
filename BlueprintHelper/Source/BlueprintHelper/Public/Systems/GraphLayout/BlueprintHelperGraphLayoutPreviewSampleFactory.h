#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewSampleFactory
{
public:
	static bool BuildSample(ESemanticScene Scene, FGraphLayoutPreviewSample& OutSample, FString& OutError);
};
}
