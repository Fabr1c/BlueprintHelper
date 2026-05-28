#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"
#include "BlueprintHelperDebugUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperDebugUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Normalize an asset object path for AssetRegistry queries. */
	static FString NormalizeAssetObjectPathForRegistry(const FString& InAssetPath);

	/** Try to read fragment artifact refs from a JSON object's "fragment_artifacts" field. */
	static bool TryReadFragmentArtifactsObject(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperDebugFragmentArtifactRefs& OutRefs);

	/** Extract fragment artifacts from a tool result summary JSON object. */
	static FBlueprintHelperDebugFragmentArtifactRefs ExtractFragmentArtifactsFromToolResultSummary(
		const TSharedPtr<FJsonObject>& ToolResultSummary);
};
