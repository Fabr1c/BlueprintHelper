#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

struct BLUEPRINTHELPER_API FBlueprintHelperArrayTypedPinEvidenceGuardResult
{
	bool bPassed = false;
	FString ErrorCode;
	FString Message;
	FBlueprintHelperCallFunctionPinType LhsPinType;
	FBlueprintHelperCallFunctionPinType RhsPinType;
};

class BLUEPRINTHELPER_API FBlueprintHelperArrayTypedPinEvidenceGuard
{
public:
	static FBlueprintHelperArrayTypedPinEvidenceGuardResult ValidateArrayIdenticalEvidence(
		const TMap<FString, FString>& Evidence);
};
