#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

const TArray<FBlueprintHelperOpCallableSpec>& FBlueprintHelperOpCallableCatalog::GetSupportedCallableSpecs()
{
	return UGraphWriteActionClusterUtils::SupportedSpecs();
}

const TArray<FBlueprintHelperOpCallableSpec>& FBlueprintHelperOpCallableCatalog::GetExcludedSpecs()
{
	return UGraphWriteActionClusterUtils::ExcludedSpecs();
}

const TArray<FString>& FBlueprintHelperOpCallableCatalog::GetTypePromotionOperationIds()
{
	static const TArray<FString> OperationIds = {
		TEXT("add"),
		TEXT("subtract"),
		TEXT("multiply"),
		TEXT("divide"),
		TEXT("greater"),
		TEXT("greater_equal"),
		TEXT("less"),
		TEXT("less_equal"),
		TEXT("equal"),
		TEXT("not_equal")
	};
	return OperationIds;
}

const FBlueprintHelperOpCallableSpec* FBlueprintHelperOpCallableCatalog::FindSupportedSpec(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return UGraphWriteActionClusterUtils::SupportedSpecs().FindByPredicate(
		[&Normalized](const FBlueprintHelperOpCallableSpec& Spec)
		{
			return Spec.OperationId.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

const FBlueprintHelperOpCallableSpec* FBlueprintHelperOpCallableCatalog::FindExcludedSpec(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return UGraphWriteActionClusterUtils::ExcludedSpecs().FindByPredicate(
		[&Normalized](const FBlueprintHelperOpCallableSpec& Spec)
		{
			return Spec.OperationId.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

bool FBlueprintHelperOpCallableCatalog::IsTypePromotionOperation(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return GetTypePromotionOperationIds().ContainsByPredicate(
		[&Normalized](const FString& Candidate)
		{
			return Candidate.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

FString FBlueprintHelperOpCallableCatalog::NormalizeOperationId(const FString& OperationId)
{
	FString Normalized = OperationId.TrimStartAndEnd().ToLower();
	if (Normalized.StartsWith(TEXT("op.")))
	{
		Normalized.RightChopInline(3);
	}
	return Normalized;
}
