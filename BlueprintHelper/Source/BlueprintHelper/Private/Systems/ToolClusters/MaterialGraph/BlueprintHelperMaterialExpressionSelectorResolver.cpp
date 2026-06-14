// BlueprintHelper MaterialGraph selector resolver.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialExpressionSelectorResolver.h"

#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialExpressionCandidateCacheService.h"

class FBlueprintHelperMaterialExpressionSelectorResolverPrivate
{
public:
	static void FillError(
		FBlueprintHelperMaterialSelectorResolution& Result,
		const FString& Code,
		const FString& Message)
	{
		Result.bResolved = false;
		Result.ErrorCode = Code;
		Result.ErrorMessage = Message;
	}
};

FBlueprintHelperMaterialSelectorResolution FBlueprintHelperMaterialExpressionSelectorResolver::ResolveSelector(
	const TSharedPtr<FJsonValue>& SelectorValue)
{
	return ResolveSelector(SelectorValue, FString());
}

FBlueprintHelperMaterialSelectorResolution FBlueprintHelperMaterialExpressionSelectorResolver::ResolveSelector(
	const TSharedPtr<FJsonValue>& SelectorValue,
	const FString& AssetPath)
{
	FBlueprintHelperMaterialSelectorResolution Result;
	if (!SelectorValue.IsValid())
	{
		FBlueprintHelperMaterialExpressionSelectorResolverPrivate::FillError(
			Result,
			TEXT("material_expression_selector_missing"),
			TEXT("Material expression selector is required."));
		return Result;
	}

	FString SelectorString;
	if (SelectorValue->TryGetString(SelectorString))
	{
		const FString ClassName = ResolveCommonSelectorClassName(SelectorString);
		if (ClassName.IsEmpty())
		{
			FBlueprintHelperMaterialExpressionSelectorResolverPrivate::FillError(
				Result,
				TEXT("material_expression_selector_unknown"),
				FString::Printf(TEXT("Unsupported material common selector: %s."), *SelectorString));
			return Result;
		}
		Result.bResolved = true;
		Result.SelectorId = SelectorString;
		Result.ClassName = ClassName;
		return Result;
	}

	const TSharedPtr<FJsonObject> SelectorObject = SelectorValue->AsObject();
	if (SelectorObject.IsValid())
	{
		FString Query;
		if (SelectorObject->TryGetStringField(TEXT("query"), Query) && !Query.IsEmpty())
		{
			Result.bResolved = true;
			Result.bRequiresCandidateSearch = true;
			Result.SelectorId = Query;
			return Result;
		}

		FString CandidateId;
		if (SelectorObject->TryGetStringField(TEXT("candidate_id"), CandidateId) && !CandidateId.IsEmpty())
		{
			return FBlueprintHelperMaterialExpressionCandidateCacheService::ResolveCandidateId(
				CandidateId,
				AssetPath);
		}
	}

	FBlueprintHelperMaterialExpressionSelectorResolverPrivate::FillError(
		Result,
		TEXT("material_expression_selector_unknown"),
		TEXT("Material expression selector must be a common selector string, query selector, or candidate_id selector."));
	return Result;
}

bool FBlueprintHelperMaterialExpressionSelectorResolver::IsCommonSelector(const FString& Selector)
{
	return FBlueprintHelperMaterialExpressionCandidateCacheService::IsCommonSelector(Selector);
}

FString FBlueprintHelperMaterialExpressionSelectorResolver::ResolveCommonSelectorClassName(const FString& Selector)
{
	return FBlueprintHelperMaterialExpressionCandidateCacheService::ResolveCommonSelectorClassName(Selector);
}
