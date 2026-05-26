#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"

struct BLUEPRINTHELPER_API FBlueprintHelperOpCallableEvidence
{
	FString OperationId;
	FBlueprintHelperOpCallableSpec Spec;
	TMap<FString, FString> Facts;
};

class BLUEPRINTHELPER_API FBlueprintHelperOpCallableEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperOpCallableEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};
