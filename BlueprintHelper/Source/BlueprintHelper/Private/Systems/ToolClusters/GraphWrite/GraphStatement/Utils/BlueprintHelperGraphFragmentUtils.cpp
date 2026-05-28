#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"

void FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(
	FBlueprintHelperNodeFragment& Fragment,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Fragment.OwnershipTags.Add(Key, CleanValue);
	}
}
