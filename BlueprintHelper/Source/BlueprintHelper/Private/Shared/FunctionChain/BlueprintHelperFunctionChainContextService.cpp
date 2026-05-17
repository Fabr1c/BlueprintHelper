#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextService.h"

#include "Engine/Blueprint.h"
#include "Shared/FunctionChain/Utils/BlueprintHelperFunctionChainTraversalUtils.h"

bool FBlueprintHelperFunctionChainContextService::TryBuildFunctionChainContext(
	const FBlueprintHelperFunctionChainContextRequest& Request,
	FBlueprintHelperFunctionChainContextPack& OutContext,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	OutContext = FBlueprintHelperFunctionChainContextPack();
	OutErrorCode.Empty();
	OutErrorMessage.Empty();

	if (Request.AssetPath.IsEmpty())
	{
		OutErrorCode = TEXT("asset_path_required");
		OutErrorMessage = TEXT("asset_path is required.");
		return false;
	}
	if (Request.TargetName.IsEmpty())
	{
		OutErrorCode = TEXT("target_name_required");
		OutErrorMessage = TEXT("target_name is required.");
		return false;
	}
	if (!Request.TargetType.Equals(TEXT("function"), ESearchCase::IgnoreCase) &&
		!Request.TargetType.Equals(TEXT("event"), ESearchCase::IgnoreCase) &&
		!Request.TargetType.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		OutErrorCode = TEXT("unsupported_target_type");
		OutErrorMessage = TEXT("target_type must be function, event, or custom_event.");
		return false;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Request.AssetPath);
	if (!Blueprint)
	{
		OutErrorCode = TEXT("asset_not_found");
		OutErrorMessage = FString::Printf(TEXT("Blueprint asset not found: %s."), *Request.AssetPath);
		return false;
	}

	return FBlueprintHelperFunctionChainTraversalUtils::BuildContext(Blueprint, Request, OutContext, OutErrorCode, OutErrorMessage);
}
