// BlueprintHelper Review FBlueprintHelperReviewStorePathUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

class FBlueprintHelperReviewStorePathUtils
{
public:
	static bool IsSafeReviewRecordId(const FString& ReviewRecordId);
	static FString GetRecordsDir();
	static FString GetRecordPath(const FString& ReviewRecordId);
};
