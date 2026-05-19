// BlueprintHelper TaskRuntime request-level CallFunction resolution cache implementation.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.h"

static void AppendPinTypeKeyParts(
	TArray<FString>& InOutParts,
	const FString& Prefix,
	const FBlueprintHelperCallFunctionPinType& PinType)
{
	InOutParts.Add(FString::Printf(TEXT("%s.category=%s"), *Prefix, *PinType.Category));
	InOutParts.Add(FString::Printf(TEXT("%s.sub_category=%s"), *Prefix, *PinType.SubCategory));
	InOutParts.Add(FString::Printf(TEXT("%s.object=%s"), *Prefix, *PinType.ObjectPath));
	InOutParts.Add(FString::Printf(TEXT("%s.container=%s"), *Prefix, *PinType.ContainerType));
	InOutParts.Add(FString::Printf(TEXT("%s.ref=%d"), *Prefix, PinType.bIsReference ? 1 : 0));
	InOutParts.Add(FString::Printf(TEXT("%s.const=%d"), *Prefix, PinType.bIsConst ? 1 : 0));
}

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::TryGet(
	const FString& Key,
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue)
{
	if (const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution* Found = ValuesByKey.Find(Key))
	{
		OutValue = *Found;
		++HitCount;
		return true;
	}

	++MissCount;
	return false;
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::Store(
	const FString& Key,
	const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value)
{
	if (Key.IsEmpty())
	{
		return;
	}

	ValuesByKey.Add(Key, Value);
}

FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::GetStats() const
{
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats Stats;
	Stats.Hits = HitCount;
	Stats.Misses = MissCount;
	Stats.Entries = ValuesByKey.Num();
	return Stats;
}

FString FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	const FString& AssetPath,
	const FString& GraphName)
{
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("asset=%s"), *AssetPath));
	Parts.Add(FString::Printf(TEXT("graph=%s"), *GraphName));
	Parts.Add(FString::Printf(TEXT("query=%s"), *Request.Query));
	Parts.Add(FString::Printf(TEXT("search=%s"), *Request.SearchMode));
	Parts.Add(FString::Printf(TEXT("ambiguity=%s"), *Request.AmbiguityPolicy));
	Parts.Add(FString::Printf(TEXT("target_object=%s"), *Request.TargetObjectType));
	Parts.Add(FString::Printf(TEXT("expected_return=%s"), *Request.ExpectedReturnType));
	AppendPinTypeKeyParts(Parts, TEXT("target_object_pin"), Request.TargetObjectPinType);
	AppendPinTypeKeyParts(Parts, TEXT("expected_return_pin"), Request.ExpectedReturnPinType);

	TArray<FString> ArgumentNames = Request.ArgumentNames;
	ArgumentNames.Sort();
	for (const FString& ArgumentName : ArgumentNames)
	{
		const FString* ArgumentType = Request.ArgumentTypes.Find(ArgumentName);
		Parts.Add(FString::Printf(
			TEXT("arg=%s:%s"),
			*ArgumentName,
			ArgumentType ? **ArgumentType : TEXT("")));
	}

	TArray<FString> ArgumentPinNames;
	Request.ArgumentPinTypes.GetKeys(ArgumentPinNames);
	ArgumentPinNames.Sort();
	for (const FString& ArgumentPinName : ArgumentPinNames)
	{
		if (const FBlueprintHelperCallFunctionPinType* PinType = Request.ArgumentPinTypes.Find(ArgumentPinName))
		{
			AppendPinTypeKeyParts(Parts, FString::Printf(TEXT("arg_pin.%s"), *ArgumentPinName), *PinType);
		}
	}

	TArray<FString> CategoryPriority = Request.CategoryPriority;
	for (int32 CategoryIndex = 0; CategoryIndex < CategoryPriority.Num(); ++CategoryIndex)
	{
		Parts.Add(FString::Printf(TEXT("category[%d]=%s"), CategoryIndex, *CategoryPriority[CategoryIndex]));
	}

	return FString::Join(Parts, TEXT("|"));
}
