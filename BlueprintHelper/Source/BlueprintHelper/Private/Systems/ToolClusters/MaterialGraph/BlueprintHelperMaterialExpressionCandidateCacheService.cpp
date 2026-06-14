// BlueprintHelper MaterialGraph expression candidate cache service.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialExpressionCandidateCacheService.h"

#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadingBase.h"
#include "Materials/MaterialExpression.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

class FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate
{
public:
	struct FCommonSelectorInfo
	{
		FString Selector;
		FString DisplayName;
		FString ClassName;
		FString Category;
	};

	struct FClassActionInfo
	{
		FString ClassPath;
		FString ClassName;
		FString DisplayName;
		FString Category;
		FString SearchText;
		FString CommonSelector;
		bool bP0ExecuteSupported = false;
	};

	struct FClassActionSnapshot
	{
		TArray<FClassActionInfo> Actions;
		FString CatalogFingerprint;
		FString Revision;
		FDateTime LastRefreshedUtc;
	};

	struct FScoredClassActionInfo
	{
		FClassActionInfo Action;
		int32 Score = 0;
	};

	struct FCachedCandidateInfo
	{
		FString CandidateId;
		FString AssetPath;
		FString Query;
		FString Fingerprint;
		FString SchemaFingerprint;
		FString CatalogFingerprint;
		FString ClassCatalogRevision;
		FString ClassPath;
		FString Selector;
		FString DisplayName;
		FString ClassName;
		FString Category;
		FDateTime ExpiresAtUtc;
	};

	static const TArray<FCommonSelectorInfo>& CommonSelectors()
	{
		static const TArray<FCommonSelectorInfo> Selectors = {
			{ TEXT("constant"), TEXT("Constant"), TEXT("UMaterialExpressionConstant"), TEXT("Math") },
			{ TEXT("scalar_parameter"), TEXT("Scalar Parameter"), TEXT("UMaterialExpressionScalarParameter"), TEXT("Parameter") },
			{ TEXT("vector_parameter"), TEXT("Vector Parameter"), TEXT("UMaterialExpressionVectorParameter"), TEXT("Parameter") },
			{ TEXT("texture_object_parameter"), TEXT("Texture Object Parameter"), TEXT("UMaterialExpressionTextureObjectParameter"), TEXT("Texture") },
			{ TEXT("texture_sample"), TEXT("Texture Sample"), TEXT("UMaterialExpressionTextureSample"), TEXT("Texture") },
			{ TEXT("add"), TEXT("Add"), TEXT("UMaterialExpressionAdd"), TEXT("Math") },
			{ TEXT("multiply"), TEXT("Multiply"), TEXT("UMaterialExpressionMultiply"), TEXT("Math") },
			{ TEXT("static_switch_parameter"), TEXT("Static Switch Parameter"), TEXT("UMaterialExpressionStaticSwitchParameter"), TEXT("Parameter") },
		};
		return Selectors;
	}

	static FString CandidateSchemaFingerprint()
	{
		return TEXT("BlueprintHelper.MaterialExpressionCandidates.v1");
	}

	static FString ClassActionSnapshotSchema()
	{
		return TEXT("BlueprintHelper.MaterialExpressionClassActionSnapshot.v1");
	}

	static FString CandidatePrefix()
	{
		return TEXT("mat_expr_");
	}

	static FTimespan CandidateTtl()
	{
		return FTimespan::FromMinutes(10.0);
	}

	static FString NormalizeCacheText(const FString& Value)
	{
		FString Result = Value.TrimStartAndEnd().ToLower();
		Result.ReplaceInline(TEXT("_"), TEXT(" "));
		Result.ReplaceInline(TEXT("-"), TEXT(" "));
		Result.ReplaceInline(TEXT("."), TEXT(" "));
		return Result;
	}

	static FString NormalizeAssetPath(const FString& AssetPath)
	{
		return AssetPath.TrimStartAndEnd().ToLower();
	}

	static FString NormalizeClassNameForComparison(const FString& ClassName)
	{
		FString Result = ClassName;
		if (Result.StartsWith(TEXT("U")) && Result.Mid(1).StartsWith(TEXT("MaterialExpression")))
		{
			Result.RightChopInline(1);
		}
		return Result;
	}

	static FString StripMaterialExpressionPrefix(const FString& ClassName)
	{
		FString Result = NormalizeClassNameForComparison(ClassName);
		if (Result.StartsWith(TEXT("MaterialExpression")))
		{
			Result.RightChopInline(18);
		}
		return Result;
	}

	static FString SplitCamelCase(const FString& Value)
	{
		FString Result;
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Current = Value[Index];
			const TCHAR Previous = Index > 0 ? Value[Index - 1] : 0;
			const TCHAR Next = Index + 1 < Value.Len() ? Value[Index + 1] : 0;
			const bool bNeedsSpace =
				Index > 0 &&
				FChar::IsUpper(Current) &&
				(FChar::IsLower(Previous) || (Next != 0 && FChar::IsLower(Next)));
			if (bNeedsSpace)
			{
				Result.AppendChar(TCHAR(' '));
			}
			Result.AppendChar(Current);
		}
		return Result.TrimStartAndEnd();
	}

	static FString BuildDisplayName(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}

		const FString MetadataDisplayName = Class->GetMetaData(TEXT("DisplayName"));
		if (!MetadataDisplayName.IsEmpty())
		{
			return MetadataDisplayName;
		}

		const FString CompactName = StripMaterialExpressionPrefix(Class->GetName());
		const FString DisplayName = SplitCamelCase(CompactName);
		return DisplayName.IsEmpty() ? Class->GetName() : DisplayName;
	}

	static FString BuildCategory(const UClass* Class)
	{
		if (!Class)
		{
			return TEXT("Material Expression");
		}

		const FString ExplicitMaterialCategory = Class->GetMetaData(TEXT("MaterialExpressionCategory"));
		if (!ExplicitMaterialCategory.IsEmpty())
		{
			return ExplicitMaterialCategory;
		}

		const FString ExplicitCategory = Class->GetMetaData(TEXT("Category"));
		if (!ExplicitCategory.IsEmpty())
		{
			return ExplicitCategory;
		}

		const FString ClassName = Class->GetName();
		if (ClassName.Contains(TEXT("Parameter")))
		{
			return TEXT("Parameter");
		}
		if (ClassName.Contains(TEXT("Texture")))
		{
			return TEXT("Texture");
		}
		if (ClassName.Contains(TEXT("Noise")) || ClassName.Contains(TEXT("Voronoi")))
		{
			return TEXT("Procedural");
		}
		if (ClassName.Contains(TEXT("Add")) ||
			ClassName.Contains(TEXT("Subtract")) ||
			ClassName.Contains(TEXT("Multiply")) ||
			ClassName.Contains(TEXT("Divide")) ||
			ClassName.Contains(TEXT("Clamp")) ||
			ClassName.Contains(TEXT("Power")) ||
			ClassName.Contains(TEXT("Constant")))
		{
			return TEXT("Math");
		}
		if (ClassName.Contains(TEXT("Coordinate")) || ClassName.Contains(TEXT("Position")))
		{
			return TEXT("Coordinates");
		}
		return TEXT("Material Expression");
	}

	static FString FindCommonSelectorForClassName(const FString& ClassName)
	{
		const FString NormalizedClassName = NormalizeClassNameForComparison(ClassName);
		for (const FCommonSelectorInfo& Info : CommonSelectors())
		{
			if (NormalizeClassNameForComparison(Info.ClassName) == NormalizedClassName)
			{
				return Info.Selector;
			}
		}
		return FString();
	}

	static FString BuildSearchText(const FClassActionInfo& Info)
	{
		return NormalizeCacheText(FString::Printf(
			TEXT("%s %s U%s %s %s material expression %s"),
			*Info.DisplayName,
			*Info.ClassName,
			*Info.ClassName,
			*Info.Category,
			*Info.CommonSelector,
			*StripMaterialExpressionPrefix(Info.ClassName)));
	}

	static FClassActionSnapshot BuildClassActionSnapshotOnGameThread()
	{
		FClassActionSnapshot Snapshot;
		Snapshot.LastRefreshedUtc = FDateTime::UtcNow();

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class ||
				Class == UMaterialExpression::StaticClass() ||
				!Class->IsChildOf(UMaterialExpression::StaticClass()) ||
				Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			FClassActionInfo Info;
			Info.ClassPath = Class->GetPathName();
			Info.ClassName = Class->GetName();
			Info.DisplayName = BuildDisplayName(Class);
			Info.Category = BuildCategory(Class);
			Info.CommonSelector = FindCommonSelectorForClassName(Info.ClassName);
			Info.bP0ExecuteSupported = !Info.CommonSelector.IsEmpty();
			Info.SearchText = BuildSearchText(Info);
			Snapshot.Actions.Add(Info);
		}

		Snapshot.Actions.Sort([](const FClassActionInfo& Left, const FClassActionInfo& Right)
		{
			if (Left.DisplayName == Right.DisplayName)
			{
				return Left.ClassName < Right.ClassName;
			}
			return Left.DisplayName < Right.DisplayName;
		});

		FString FingerprintSource = ClassActionSnapshotSchema();
		for (const FClassActionInfo& Info : Snapshot.Actions)
		{
			FingerprintSource += FString::Printf(
				TEXT("|%s|%s|%s|%s"),
				*Info.ClassPath,
				*Info.ClassName,
				*Info.Category,
				*Info.CommonSelector);
		}

		const uint32 FingerprintCrc = FCrc::StrCrc32(*FingerprintSource);
		Snapshot.CatalogFingerprint = FString::Printf(
			TEXT("%s_%08x"),
			*ClassActionSnapshotSchema(),
			FingerprintCrc);
		Snapshot.Revision = FString::Printf(
			TEXT("class_catalog_revision_%08x_%d"),
			FingerprintCrc,
			Snapshot.Actions.Num());
		return Snapshot;
	}

	static FCriticalSection& ClassActionSnapshotCriticalSection()
	{
		static FCriticalSection CriticalSection;
		return CriticalSection;
	}

	static FClassActionSnapshot& MutableClassActionSnapshot()
	{
		static FClassActionSnapshot Snapshot;
		return Snapshot;
	}

	static FClassActionSnapshot GetClassActionSnapshotCopy()
	{
		if (IsInGameThread())
		{
			FBlueprintHelperMaterialExpressionCandidateCacheService::RefreshMaterialExpressionClassActionSnapshotOnGameThread();
		}

		FScopeLock Lock(&ClassActionSnapshotCriticalSection());
		return MutableClassActionSnapshot();
	}

	static FCriticalSection& CandidateCacheCriticalSection()
	{
		static FCriticalSection CriticalSection;
		return CriticalSection;
	}

	static TMap<FString, FCachedCandidateInfo>& CandidateCache()
	{
		static TMap<FString, FCachedCandidateInfo> Cache;
		return Cache;
	}

	static void FillError(
		FBlueprintHelperMaterialSelectorResolution& Result,
		const FString& Code,
		const FString& Message)
	{
		Result.bResolved = false;
		Result.ErrorCode = Code;
		Result.ErrorMessage = Message;
	}

	static void FillExpiredError(
		FBlueprintHelperMaterialSelectorResolution& Result,
		const FString& Message)
	{
		FillError(Result, TEXT("material_expression_candidate_expired"), Message);
		Result.bCandidateExpired = true;
	}

	static void PruneExpiredCandidatesLocked(const FDateTime& NowUtc)
	{
		TArray<FString> ExpiredIds;
		for (const TPair<FString, FCachedCandidateInfo>& Pair : CandidateCache())
		{
			if (Pair.Value.ExpiresAtUtc <= NowUtc)
			{
				ExpiredIds.Add(Pair.Key);
			}
		}
		for (const FString& CandidateId : ExpiredIds)
		{
			CandidateCache().Remove(CandidateId);
		}
	}

	static FString BuildCandidateId(const FString& Fingerprint, const FString& ClassPath)
	{
		const FString Source = Fingerprint + TEXT("|class=") + NormalizeCacheText(ClassPath);
		return FString::Printf(TEXT("%s%08x"), *CandidatePrefix(), FCrc::StrCrc32(*Source));
	}

	static TSharedPtr<FJsonValue> BuildCandidateJson(
		const FCachedCandidateInfo& Info,
		const FString& Reason)
	{
		TSharedRef<FJsonObject> Candidate = MakeShared<FJsonObject>();
		Candidate->SetStringField(TEXT("candidate_id"), Info.CandidateId);
		Candidate->SetStringField(TEXT("display_name"), Info.DisplayName);
		Candidate->SetStringField(TEXT("class_name"), Info.ClassName);
		Candidate->SetStringField(TEXT("category"), Info.Category);
		Candidate->SetStringField(TEXT("reason"), Reason);
		return MakeShared<FJsonValueObject>(Candidate);
	}

	static bool CandidateMatchesQuery(
		const FClassActionInfo& Info,
		const FString& NormalizedQuery,
		int32& OutScore)
	{
		if (NormalizedQuery.IsEmpty())
		{
			OutScore = Info.bP0ExecuteSupported ? 20 : 0;
			return true;
		}

		TArray<FString> Tokens;
		NormalizedQuery.ParseIntoArrayWS(Tokens);
		for (const FString& Token : Tokens)
		{
			if (!Token.IsEmpty() && !Info.SearchText.Contains(Token))
			{
				return false;
			}
		}

		const FString DisplayName = NormalizeCacheText(Info.DisplayName);
		const FString ClassName = NormalizeCacheText(Info.ClassName);
		const FString Category = NormalizeCacheText(Info.Category);
		const FString CommonSelector = NormalizeCacheText(Info.CommonSelector);
		int32 Score = Tokens.Num();
		if (DisplayName == NormalizedQuery)
		{
			Score += 100;
		}
		if (DisplayName.Contains(NormalizedQuery))
		{
			Score += 60;
		}
		if (ClassName.Contains(NormalizedQuery))
		{
			Score += 50;
		}
		if (CommonSelector.Contains(NormalizedQuery))
		{
			Score += 40;
		}
		if (Category.Contains(NormalizedQuery))
		{
			Score += 20;
		}
		if (Info.bP0ExecuteSupported)
		{
			Score += 5;
		}
		OutScore = Score;
		return true;
	}
};

bool FBlueprintHelperMaterialExpressionCandidateCacheService::IsCommonSelector(const FString& Selector)
{
	return !ResolveCommonSelectorClassName(Selector).IsEmpty();
}

FString FBlueprintHelperMaterialExpressionCandidateCacheService::ResolveCommonSelectorClassName(
	const FString& Selector)
{
	for (const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FCommonSelectorInfo& Info :
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CommonSelectors())
	{
		if (Selector.Equals(Info.Selector, ESearchCase::IgnoreCase))
		{
			return Info.ClassName;
		}
	}
	return FString();
}

FString FBlueprintHelperMaterialExpressionCandidateCacheService::GetCandidateSchemaFingerprint()
{
	return FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateSchemaFingerprint();
}

FString FBlueprintHelperMaterialExpressionCandidateCacheService::GetMaterialExpressionClassActionSnapshotRevision()
{
	const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot Snapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::GetClassActionSnapshotCopy();
	return Snapshot.Revision;
}

FString FBlueprintHelperMaterialExpressionCandidateCacheService::GetMaterialExpressionClassActionSnapshotFingerprint()
{
	const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot Snapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::GetClassActionSnapshotCopy();
	return Snapshot.CatalogFingerprint;
}

void FBlueprintHelperMaterialExpressionCandidateCacheService::RefreshMaterialExpressionClassActionSnapshotOnGameThread()
{
	if (!IsInGameThread())
	{
		return;
	}

	FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot Snapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::BuildClassActionSnapshotOnGameThread();
	FScopeLock Lock(
		&FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::ClassActionSnapshotCriticalSection());
	FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::MutableClassActionSnapshot() = MoveTemp(Snapshot);
}

FString FBlueprintHelperMaterialExpressionCandidateCacheService::BuildCandidateCacheFingerprint(
	const FString& AssetPath,
	const FString& Query)
{
	const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot Snapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::GetClassActionSnapshotCopy();
	const FString Source = FString::Printf(
		TEXT("%s|snapshot=%s|revision=%s|asset=%s|query=%s|count=%d"),
		*GetCandidateSchemaFingerprint(),
		*Snapshot.CatalogFingerprint,
		*Snapshot.Revision,
		*FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::NormalizeAssetPath(AssetPath),
		*FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::NormalizeCacheText(Query),
		Snapshot.Actions.Num());
	return FString::Printf(
		TEXT("material_expression_candidate_cache_%08x"),
		FCrc::StrCrc32(*Source));
}

double FBlueprintHelperMaterialExpressionCandidateCacheService::GetCandidateTtlSeconds()
{
	return FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateTtl().GetTotalSeconds();
}

TArray<TSharedPtr<FJsonValue>> FBlueprintHelperMaterialExpressionCandidateCacheService::BuildAndCacheCandidates(
	const FString& Query,
	const FString& AssetPath)
{
	TArray<TSharedPtr<FJsonValue>> Candidates;
	const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot Snapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::GetClassActionSnapshotCopy();
	const FString NormalizedQuery =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::NormalizeCacheText(Query);
	const FString NormalizedAssetPath =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::NormalizeAssetPath(AssetPath);
	const FString Fingerprint = BuildCandidateCacheFingerprint(AssetPath, Query);
	const FString Reason = NormalizedQuery.IsEmpty() ? TEXT("catalog_match") : TEXT("query_match");
	const FDateTime ExpiresAtUtc =
		FDateTime::UtcNow() + FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateTtl();

	TArray<FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FScoredClassActionInfo> Matches;
	for (const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionInfo& Info : Snapshot.Actions)
	{
		if (!Info.bP0ExecuteSupported)
		{
			continue;
		}

		int32 Score = 0;
		if (FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateMatchesQuery(
			Info,
			NormalizedQuery,
			Score))
		{
			FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FScoredClassActionInfo Match;
			Match.Action = Info;
			Match.Score = Score;
			Matches.Add(Match);
		}
	}

	Matches.Sort([](
		const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FScoredClassActionInfo& Left,
		const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FScoredClassActionInfo& Right)
	{
		if (Left.Score == Right.Score)
		{
			if (Left.Action.bP0ExecuteSupported != Right.Action.bP0ExecuteSupported)
			{
				return Left.Action.bP0ExecuteSupported;
			}
			if (Left.Action.DisplayName == Right.Action.DisplayName)
			{
				return Left.Action.ClassName < Right.Action.ClassName;
			}
			return Left.Action.DisplayName < Right.Action.DisplayName;
		}
		return Left.Score > Right.Score;
	});

	FScopeLock Lock(&FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateCacheCriticalSection());
	FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::PruneExpiredCandidatesLocked(FDateTime::UtcNow());
	for (const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FScoredClassActionInfo& Match : Matches)
	{
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FCachedCandidateInfo Cached;
		Cached.CandidateId = FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::BuildCandidateId(
			Fingerprint,
			Match.Action.ClassPath);
		Cached.AssetPath = NormalizedAssetPath;
		Cached.Query = NormalizedQuery;
		Cached.Fingerprint = Fingerprint;
		Cached.SchemaFingerprint = GetCandidateSchemaFingerprint();
		Cached.CatalogFingerprint = Snapshot.CatalogFingerprint;
		Cached.ClassCatalogRevision = Snapshot.Revision;
		Cached.ClassPath = Match.Action.ClassPath;
		Cached.Selector = Match.Action.CommonSelector;
		Cached.DisplayName = Match.Action.DisplayName;
		Cached.ClassName = Match.Action.ClassName;
		Cached.Category = Match.Action.Category;
		Cached.ExpiresAtUtc = ExpiresAtUtc;

		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateCache().Add(
			Cached.CandidateId,
			Cached);
		Candidates.Add(FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::BuildCandidateJson(Cached, Reason));
	}
	return Candidates;
}

TArray<TSharedPtr<FJsonValue>> FBlueprintHelperMaterialExpressionCandidateCacheService::BuildCommonSelectorCandidates(
	const FString& Query)
{
	return BuildAndCacheCandidates(Query, FString());
}

FBlueprintHelperMaterialSelectorResolution FBlueprintHelperMaterialExpressionCandidateCacheService::ResolveCandidateId(
	const FString& CandidateId,
	const FString& AssetPath)
{
	FBlueprintHelperMaterialSelectorResolution Result;
	if (!CandidateId.StartsWith(FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidatePrefix()))
	{
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FillExpiredError(
			Result,
			FString::Printf(TEXT("MaterialGraph candidate_id is not from the active candidate cache: %s."), *CandidateId));
		return Result;
	}

	FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FCachedCandidateInfo Cached;
	{
		const FDateTime NowUtc = FDateTime::UtcNow();
		FScopeLock Lock(&FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateCacheCriticalSection());
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::PruneExpiredCandidatesLocked(NowUtc);
		const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FCachedCandidateInfo* CachedPtr =
			FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::CandidateCache().Find(CandidateId);
		if (!CachedPtr)
		{
			FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FillExpiredError(
				Result,
				FString::Printf(TEXT("MaterialGraph candidate_id is expired or was not produced by the current preview cache: %s."), *CandidateId));
			return Result;
		}
		Cached = *CachedPtr;
	}

	const FString NormalizedAssetPath =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::NormalizeAssetPath(AssetPath);
	if (!NormalizedAssetPath.IsEmpty() && Cached.AssetPath != NormalizedAssetPath)
	{
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FillExpiredError(
			Result,
			TEXT("MaterialGraph candidate_id belongs to a different material asset preview cache."));
		return Result;
	}

	const FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FClassActionSnapshot CurrentSnapshot =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::GetClassActionSnapshotCopy();
	const FString CurrentFingerprint = BuildCandidateCacheFingerprint(Cached.AssetPath, Cached.Query);
	const FString ExpectedCandidateId =
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::BuildCandidateId(
			Cached.Fingerprint,
			Cached.ClassPath);
	if (Cached.SchemaFingerprint != GetCandidateSchemaFingerprint() ||
		Cached.CatalogFingerprint != CurrentSnapshot.CatalogFingerprint ||
		Cached.ClassCatalogRevision != CurrentSnapshot.Revision ||
		Cached.Fingerprint != CurrentFingerprint ||
		CandidateId != ExpectedCandidateId)
	{
		FBlueprintHelperMaterialExpressionCandidateCacheServicePrivate::FillExpiredError(
			Result,
			TEXT("MaterialGraph candidate_id fingerprint is stale for the current MaterialExpression class action snapshot."));
		return Result;
	}

	Result.bResolved = true;
	Result.SelectorId = Cached.Selector.IsEmpty() ? Cached.DisplayName : Cached.Selector;
	Result.CandidateId = Cached.CandidateId;
	Result.Fingerprint = Cached.Fingerprint;
	Result.ClassName = Cached.ClassName;
	return Result;
}
