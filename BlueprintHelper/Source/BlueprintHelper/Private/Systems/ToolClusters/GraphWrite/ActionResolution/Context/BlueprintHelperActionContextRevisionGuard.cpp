#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.h"

bool FBlueprintHelperActionContextRevisionGuard::Validate(
	const FBlueprintHelperActionContextRevisionToken& Expected,
	const FBlueprintHelperActionContextRevisionToken& Current,
	FString& OutError)
{
	if (Expected.IsCompatibleWith(Current))
	{
		OutError.Reset();
		return true;
	}

	OutError = FString::Printf(
		TEXT("action_context_stale:asset=%s graph=%s"),
		*Expected.AssetPath,
		*Expected.GraphName);
	return false;
}

bool FBlueprintHelperActionContextRevisionGuard::ValidateBundle(
	const FBlueprintHelperResolvedActionContextBundle& Bundle,
	const FBlueprintHelperActionContextRevisionToken& Current,
	FString& OutError)
{
	return Validate(Bundle.Revision, Current, OutError);
}
