#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.h"

struct FBlueprintHelperReadCachePolicyRule
{
	bool bPersistentAllowed = false;
	bool bRequestLocalOnly = false;
	const TCHAR* Description = TEXT("");
};

class FBlueprintHelperReadCachePolicyLocalUtils
{
public:
	static const TMap<EBlueprintHelperReadCacheDataKind, FBlueprintHelperReadCachePolicyRule>& Rules()
	{
		static const TMap<EBlueprintHelperReadCacheDataKind, FBlueprintHelperReadCachePolicyRule> Map = {
			{
				EBlueprintHelperReadCacheDataKind::CliSchemaMetadata,
				{true, false, TEXT("Pure CLI schema metadata may be cached by schema version.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::ReadCapabilityMatrix,
				{true, false, TEXT("Read capability matrix is pure process metadata.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::RuntimeProfileStaticInfo,
				{true, false, TEXT("Runtime profile static process info may be cached when it does not include editor object state.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::FormatterRegistryMetadata,
				{true, false, TEXT("Formatter registry metadata is pure and versioned by code.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::AssetGraphSnapshot,
				{false, true, TEXT("Asset graph snapshots may only live inside a single read request.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::UObjectPointer,
				{false, false, TEXT("UObject pointers must not be cached by read tooling.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::BlueprintObjectPointer,
				{false, false, TEXT("Blueprint object pointers must not be cached by read tooling.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::GraphObjectPointer,
				{false, false, TEXT("UEdGraph pointers must not be cached by read tooling.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::WidgetTreeObjectPointer,
				{false, false, TEXT("UWidgetTree pointers must not be cached by read tooling.")}
			},
			{
				EBlueprintHelperReadCacheDataKind::PropertyReflectionResult,
				{false, true, TEXT("Property reflection DTOs may only be request-local unless an explicit invalidation model exists.")}
			},
		};
		return Map;
	}

	static FBlueprintHelperReadCachePolicyRule FindRule(EBlueprintHelperReadCacheDataKind Kind)
	{
		if (const FBlueprintHelperReadCachePolicyRule* Rule = Rules().Find(Kind))
		{
			return *Rule;
		}
		return FBlueprintHelperReadCachePolicyRule();
	}
};

bool FBlueprintHelperReadCachePolicy::IsPersistentCacheAllowed(
	EBlueprintHelperReadCacheDataKind Kind)
{
	return FBlueprintHelperReadCachePolicyLocalUtils::FindRule(Kind).bPersistentAllowed;
}

bool FBlueprintHelperReadCachePolicy::IsRequestLocalOnly(
	EBlueprintHelperReadCacheDataKind Kind)
{
	return FBlueprintHelperReadCachePolicyLocalUtils::FindRule(Kind).bRequestLocalOnly;
}

FString FBlueprintHelperReadCachePolicy::Describe(
	EBlueprintHelperReadCacheDataKind Kind)
{
	return FBlueprintHelperReadCachePolicyLocalUtils::FindRule(Kind).Description;
}
