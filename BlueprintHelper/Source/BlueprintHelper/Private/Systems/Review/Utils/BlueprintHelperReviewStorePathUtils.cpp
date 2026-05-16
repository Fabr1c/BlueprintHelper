// BlueprintHelper Review BlueprintHelperReviewStorePathUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewStorePathUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

bool FBlueprintHelperReviewStorePathUtils::IsSafeReviewRecordId(const FString& ReviewRecordId)
	{
		if (ReviewRecordId.IsEmpty())
		{
			return false;
		}

		for (const TCHAR Ch : ReviewRecordId)
		{
			if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}
FString FBlueprintHelperReviewStorePathUtils::GetRecordsDir()
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ TEXT("Records");
	}
FString FBlueprintHelperReviewStorePathUtils::GetRecordPath(const FString& ReviewRecordId)
	{
		return GetRecordsDir() / FString::Printf(TEXT("%s.json"), *ReviewRecordId);
	}
