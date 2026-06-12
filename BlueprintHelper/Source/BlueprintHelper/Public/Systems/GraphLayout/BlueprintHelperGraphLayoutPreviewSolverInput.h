#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewSolverInput
{
public:
	static FGraphSnapshot BuildSolverSnapshot(const FGraphLayoutPreviewSample& Sample);
	static bool IsPreviewOverlayNode(const FGraphLayoutPreviewSample& Sample, const FString& NodeId);
};
}
