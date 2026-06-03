// BlueprintHelper TaskRuntime CallFunction resolution cache implementation.

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

FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::FBlueprintHelperTaskRuntimeCallFunctionResolutionCache(
	const FBlueprintHelperTaskRuntimeCacheConfig& InConfig)
	: Config(InConfig)
{
}

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::TryGet(
	const FString& Key,
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue)
{
	return TryGet(Key, TEXT(""), TEXT(""), FDateTime::UtcNow(), OutValue);
}

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::TryGet(
	const FString& Key,
	const FString& AssetStateHash,
	const FDateTime& NowUtc,
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue)
{
	return TryGet(Key, AssetStateHash, TEXT(""), NowUtc, OutValue);
}

bool FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::TryGet(
	const FString& Key,
	const FString& AssetStateHash,
	const FString& ContextRevisionManifestHash,
	const FDateTime& NowUtc,
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue)
{
	if (FBlueprintHelperTaskRuntimeCachedCallFunctionResolution* Found = ValuesByKey.Find(Key))
	{
		if (Found->ExpiresAtUtc.GetTicks() > 0 && Found->ExpiresAtUtc <= NowUtc)
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Found->EstimatedBytes);
			ValuesByKey.Remove(Key);
			++RequestStats.PrunedExpiredEntries;
			++RequestStats.Misses;
			return false;
		}
		if (!AssetStateHash.IsEmpty() && Found->AssetStateHash != AssetStateHash)
		{
			++RequestStats.Misses;
			return false;
		}
		if (!ContextRevisionManifestHash.IsEmpty() &&
			Found->ContextRevisionManifestHash != ContextRevisionManifestHash)
		{
			++RequestStats.Misses;
			return false;
		}
		if (!Found->ResolverVersion.IsEmpty() && Found->ResolverVersion != CurrentResolverVersion())
		{
			++RequestStats.Misses;
			return false;
		}
		Found->LastAccessedAtUtc = NowUtc;
		OutValue = *Found;
		++RequestStats.Hits;
		return true;
	}

	++RequestStats.Misses;
	return false;
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::Store(
	const FString& Key,
	const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value)
{
	Store(Key, Value, FDateTime::UtcNow());
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::Store(
	const FString& Key,
	const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value,
	const FDateTime& NowUtc)
{
	if (Key.IsEmpty())
	{
		return;
	}

	PruneIfNeeded(NowUtc);
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Stored = CloneForStore(Value, NowUtc);
	if (const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution* Existing = ValuesByKey.Find(Key))
	{
		CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Existing->EstimatedBytes);
	}
	CurrentBytes += Stored.EstimatedBytes;
	ValuesByKey.Add(Key, MoveTemp(Stored));
	TrimToBounds();
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::ResetRequestStats()
{
	RequestStats = FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats();
}

FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::GetStats() const
{
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats Stats = RequestStats;
	Stats.Entries = ValuesByKey.Num();
	Stats.CurrentBytes = CurrentBytes;
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

FString FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::CurrentResolverVersion()
{
	return TEXT("BlueprintHelper.CallFunctionResolver.v2");
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::PruneIfNeeded(const FDateTime& NowUtc)
{
	if (LastPruneAtUtc.GetTicks() > 0 &&
		NowUtc - LastPruneAtUtc < Config.PruneOnAccessMinInterval)
	{
		return;
	}

	PruneExpired(NowUtc);
	TrimToBounds();
	LastPruneAtUtc = NowUtc;
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::PruneExpired(const FDateTime& NowUtc)
{
	TArray<FString> ExpiredKeys;
	for (const TPair<FString, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution>& Pair : ValuesByKey)
	{
		if (Pair.Value.ExpiresAtUtc.GetTicks() > 0 && Pair.Value.ExpiresAtUtc <= NowUtc)
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}
	for (const FString& Key : ExpiredKeys)
	{
		if (const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution* Entry = ValuesByKey.Find(Key))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		ValuesByKey.Remove(Key);
		++RequestStats.PrunedExpiredEntries;
	}
}

void FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::TrimToBounds()
{
	auto RemoveOldest = [&]()
	{
		FString OldestKey;
		FDateTime OldestAccess = FDateTime::MaxValue();
		for (const TPair<FString, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution>& Pair : ValuesByKey)
		{
			if (Pair.Value.LastAccessedAtUtc < OldestAccess)
			{
				OldestAccess = Pair.Value.LastAccessedAtUtc;
				OldestKey = Pair.Key;
			}
		}
		if (OldestKey.IsEmpty())
		{
			return false;
		}
		if (const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution* Entry = ValuesByKey.Find(OldestKey))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		ValuesByKey.Remove(OldestKey);
		++RequestStats.PrunedCapacityEntries;
		return true;
	};

	while (ValuesByKey.Num() > Config.CallFunctionFactMaxEntries && RemoveOldest())
	{
	}

	while (CurrentBytes > Config.CallFunctionFactMaxBytes && RemoveOldest())
	{
	}
}

FBlueprintHelperTaskRuntimeCachedCallFunctionResolution FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::CloneForStore(
	const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value,
	const FDateTime& NowUtc) const
{
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution Stored = Value;
	Stored.ResolverVersion = Stored.ResolverVersion.IsEmpty()
		? CurrentResolverVersion()
		: Stored.ResolverVersion;
	Stored.CreatedAtUtc = Stored.CreatedAtUtc.GetTicks() > 0 ? Stored.CreatedAtUtc : NowUtc;
	Stored.ExpiresAtUtc = Stored.ExpiresAtUtc.GetTicks() > 0
		? Stored.ExpiresAtUtc
		: Stored.CreatedAtUtc + Config.CallFunctionFactTtl;
	Stored.LastAccessedAtUtc = NowUtc;
	Stored.EstimatedBytes = Stored.EstimatedBytes > 0 ? Stored.EstimatedBytes : EstimateBytes(Stored);
	return Stored;
}

int64 FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::EstimateBytes(
	const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value) const
{
	int64 Bytes = 0;
	const FString Scalars =
		Value.ErrorCode +
		Value.Message +
		Value.StableId +
		Value.NativeName +
		Value.DisplayName +
		Value.OwnerClassPath +
		Value.AssetStateHash +
		Value.ContextRevisionManifestHash +
		Value.ResolverVersion;
	Bytes += FTCHARToUTF8(*Scalars).Length();
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Value.CandidateFunctions)
	{
		Bytes += FTCHARToUTF8(*Candidate.StableId).Length();
		Bytes += FTCHARToUTF8(*Candidate.DisplayName).Length();
		Bytes += FTCHARToUTF8(*Candidate.OwnerClassPath).Length();
		Bytes += FTCHARToUTF8(*Candidate.NativeFunctionName).Length();
	}
	return Bytes;
}
