#include "Systems/Debug/Utils/BlueprintHelperDebugUtils.h"

FString UBlueprintHelperDebugUtils::NormalizeAssetObjectPathForRegistry(const FString& InAssetPath)
{
	FString AssetPath = InAssetPath.TrimStartAndEnd();
	if (AssetPath.IsEmpty())
	{
		return AssetPath;
	}

	int32 LastSlashIndex = INDEX_NONE;
	AssetPath.FindLastChar(TEXT('/'), LastSlashIndex);
	if (LastSlashIndex == INDEX_NONE)
	{
		return AssetPath;
	}

	int32 LastDotIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex > LastSlashIndex)
	{
		return AssetPath;
	}

	const FString AssetName = AssetPath.RightChop(LastSlashIndex + 1);
	return AssetName.IsEmpty()
		? AssetPath
		: FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
}

bool UBlueprintHelperDebugUtils::TryReadFragmentArtifactsObject(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperDebugFragmentArtifactRefs& OutRefs)
{
	if (!Json.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FragmentArtifactsObject = nullptr;
	if (Json->TryGetObjectField(TEXT("fragment_artifacts"), FragmentArtifactsObject) && FragmentArtifactsObject)
	{
		OutRefs = FBlueprintHelperDebugFragmentArtifactRefs::FromJson(*FragmentArtifactsObject);
		return OutRefs.IsValid();
	}

	return false;
}

FBlueprintHelperDebugFragmentArtifactRefs UBlueprintHelperDebugUtils::ExtractFragmentArtifactsFromToolResultSummary(
	const TSharedPtr<FJsonObject>& ToolResultSummary)
{
	FBlueprintHelperDebugFragmentArtifactRefs Refs;
	if (UBlueprintHelperDebugUtils::TryReadFragmentArtifactsObject(ToolResultSummary, Refs))
	{
		return Refs;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (ToolResultSummary.IsValid()
		&& ToolResultSummary->TryGetObjectField(TEXT("data"), DataObject)
		&& DataObject)
	{
		if (UBlueprintHelperDebugUtils::TryReadFragmentArtifactsObject(*DataObject, Refs))
		{
			return Refs;
		}

		const TSharedPtr<FJsonObject>* FragmentDebugObject = nullptr;
		if ((*DataObject)->TryGetObjectField(TEXT("fragment_debug"), FragmentDebugObject) && FragmentDebugObject)
		{
			UBlueprintHelperDebugUtils::TryReadFragmentArtifactsObject(*FragmentDebugObject, Refs);
		}
	}

	return Refs;
}
