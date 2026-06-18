#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphConnectivityPolicy
{
	FString PolicyId;
	EBlueprintHelperGraphBodyKind BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	bool bAllowExternalAnchorBoundary = false;
	bool bAllowExitBoundaryReachability = false;
	bool bAllowOwnedBlockDisconnectedPreview = false;
	bool bRequireFunctionEntryReachability = false;
	bool bRequireFunctionResultReachability = false;
	bool bRequireReturnDataflowConsumption = false;
	TArray<FString> ReturnDataPinRefs;
	TArray<FString> FunctionParamSourceRefs;
	TArray<FString> ViolationCodes;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphConnectivityPolicyUtils
{
public:
	static FBlueprintHelperGraphConnectivityPolicy FromBoundaryModel(
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel);
};
