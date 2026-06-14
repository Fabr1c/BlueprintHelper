// BlueprintHelper Service Layer - Material LogicMd projection.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/** Projects a Material LogicJson payload into compact Markdown for read_context. */
class BLUEPRINTHELPER_API FBlueprintHelperMaterialLogicMdProjector
{
public:
	FString BuildMarkdown(const TSharedPtr<FJsonObject>& LogicJson) const;

private:
	void AppendParameters(const TSharedPtr<FJsonObject>& LogicJson, FString& OutMarkdown) const;
	void AppendOutputs(const TSharedPtr<FJsonObject>& LogicJson, FString& OutMarkdown) const;
};
