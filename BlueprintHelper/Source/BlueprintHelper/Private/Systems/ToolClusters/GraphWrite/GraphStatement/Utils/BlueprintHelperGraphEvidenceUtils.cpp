#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceUtils.h"

FString FBlueprintHelperGraphEvidenceUtils::ContextEvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

FString FBlueprintHelperGraphEvidenceUtils::StableHashString(const FString& Stable)
{
	return LexToString(GetTypeHash(Stable));
}
