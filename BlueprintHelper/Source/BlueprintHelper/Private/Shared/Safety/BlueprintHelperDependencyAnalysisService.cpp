// BlueprintHelper Service Layer - internal dependency analysis service.

#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

class FBlueprintHelperDependencyAnalysisServiceLocalUtils
{
public:
	static constexpr int32 DefaultMaxResults = 50;
	static constexpr int32 MaxAllowedResults = 500;

	static FString NormalizeScope(const FString& Scope)
	{
		const FString LowerScope = Scope.IsEmpty() ? TEXT("safety_context") : Scope.ToLower();
		if (LowerScope == TEXT("dependencies") ||
			LowerScope == TEXT("referencers") ||
			LowerScope == TEXT("external_dependents") ||
			LowerScope == TEXT("all"))
		{
			return LowerScope;
		}
		return TEXT("safety_context");
	}

	static bool ShouldReadDependencies(const FString& Scope)
	{
		return Scope == TEXT("safety_context") || Scope == TEXT("dependencies") || Scope == TEXT("all");
	}

	static bool ShouldReadReferencers(const FString& Scope)
	{
		return Scope == TEXT("safety_context") || Scope == TEXT("referencers") || Scope == TEXT("all");
	}

	static bool ShouldReadExternalDependents(const FString& Scope)
	{
		return Scope == TEXT("safety_context") || Scope == TEXT("external_dependents") || Scope == TEXT("all");
	}

	static int32 ClampMaxResults(int32 MaxResults)
	{
		if (MaxResults <= 0)
		{
			return DefaultMaxResults;
		}
		return FMath::Clamp(MaxResults, 1, MaxAllowedResults);
	}

	static FString PackageNameFromAssetPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (FPackageName::IsValidLongPackageName(AssetPath))
		{
			return AssetPath;
		}
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return FPackageName::ObjectPathToPackageName(AssetPath);
		}
		return AssetPath.Contains(TEXT(".")) ? FPackageName::ObjectPathToPackageName(AssetPath) : AssetPath;
	}

	static FString ObjectPathFromPackageName(const FString& PackageName)
	{
		if (PackageName.IsEmpty())
		{
			return FString();
		}
		return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
	}

	static FString AssetPathFromDataOrPackage(const FAssetData& AssetData, const FName& PackageName)
	{
		if (AssetData.IsValid())
		{
			return AssetData.GetObjectPathString();
		}
		return PackageName.ToString();
	}

	static FString AssetTypeFromData(const FAssetData& AssetData)
	{
		if (!AssetData.IsValid())
		{
			return TEXT("unknown");
		}
		return AssetData.AssetClassPath.ToString();
	}

	static bool TryFindAssetData(IAssetRegistry& Registry, const FName PackageName, FAssetData& OutAssetData)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(PackageName, Assets, false);
		if (Assets.Num() > 0)
		{
			OutAssetData = Assets[0];
			return true;
		}
		return false;
	}

	static bool TryResolveTargetAsset(
		IAssetRegistry& Registry,
		const FString& AssetPath,
		FName& OutPackageName,
		FAssetData& OutAssetData,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		if (AssetPath.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_request");
			OutErrorMessage = TEXT("asset_path is required.");
			return false;
		}

		const FString PackageName = PackageNameFromAssetPath(AssetPath);
		OutPackageName = FName(*PackageName);
		if (TryFindAssetData(Registry, OutPackageName, OutAssetData))
		{
			return true;
		}

		UObject* LoadedAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
		if (!LoadedAsset && FPackageName::IsValidLongPackageName(PackageName))
		{
			const FString ObjectPath = ObjectPathFromPackageName(PackageName);
			LoadedAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
		}
		if (LoadedAsset)
		{
			const FString LoadedPackageName = LoadedAsset->GetOutermost()->GetName();
			OutPackageName = FName(*LoadedPackageName);
			TryFindAssetData(Registry, OutPackageName, OutAssetData);
			return true;
		}

		OutErrorCode = TEXT("asset_not_found");
		OutErrorMessage = FString::Printf(TEXT("Target asset was not found: %s"), *AssetPath);
		return false;
	}

	static FBlueprintHelperAssetRefSummary MakeAssetRefSummary(
		IAssetRegistry& Registry,
		const FName PackageName,
		const FString& EvidencePath = FString())
	{
		FAssetData AssetData;
		TryFindAssetData(Registry, PackageName, AssetData);

		FBlueprintHelperAssetRefSummary Summary;
		Summary.AssetPath = AssetPathFromDataOrPackage(AssetData, PackageName);
		Summary.AssetType = AssetTypeFromData(AssetData);
		Summary.ReferenceKind = TEXT("package");
		Summary.Source = TEXT("asset_registry");
		Summary.EvidencePath = EvidencePath.IsEmpty() ? PackageName.ToString() : EvidencePath;
		Summary.Confidence = TEXT("high");
		return Summary;
	}

	static void AddUnsupportedCheck(TArray<FString>& UnsupportedChecks, const FString& Check)
	{
		UnsupportedChecks.AddUnique(Check);
	}

	static void AddUnsupportedChecksForOptions(
		const FBlueprintHelperDependencyAnalysisOptions& Options,
		TArray<FString>& UnsupportedChecks)
	{
		if (Options.bAnalyzeBlueprintCalls)
		{
			AddUnsupportedCheck(UnsupportedChecks, TEXT("blueprint_calls"));
		}
		if (Options.bAnalyzeWidgetBindings)
		{
			AddUnsupportedCheck(UnsupportedChecks, TEXT("widget_bindings"));
		}
		if (Options.bAnalyzeDataTableRows)
		{
			AddUnsupportedCheck(UnsupportedChecks, TEXT("data_table_rows"));
		}
		if (Options.bAnalyzeRuntimeStringLookup)
		{
			AddUnsupportedCheck(UnsupportedChecks, TEXT("runtime_string_lookup"));
		}
		if (Options.bAnalyzeDynamicSoftReferences)
		{
			AddUnsupportedCheck(UnsupportedChecks, TEXT("dynamic_soft_references"));
		}
	}

	static void AppendPackageRefs(
		IAssetRegistry& Registry,
		const TArray<FName>& PackageNames,
		const FString& ReferenceKind,
		int32 MaxResults,
		bool bIncludeSamples,
		int32& InOutFullCount,
		TArray<FBlueprintHelperAssetRefSummary>& OutSamples,
		bool& bOutTruncated)
	{
		InOutFullCount += PackageNames.Num();
		if (!bIncludeSamples)
		{
			return;
		}

		for (const FName PackageName : PackageNames)
		{
			if (OutSamples.Num() >= MaxResults)
			{
				bOutTruncated = true;
				continue;
			}

			FBlueprintHelperAssetRefSummary Summary = MakeAssetRefSummary(
				Registry,
				PackageName,
				PackageName.ToString());
			Summary.ReferenceKind = ReferenceKind;
			OutSamples.Add(Summary);
		}
	}

};

bool FBlueprintHelperDependencyAnalysisService::TryBuildReferenceContext(
	const FBlueprintHelperDependencyAnalysisTarget& Target,
	const FBlueprintHelperDependencyAnalysisOptions& Options,
	const FString& Scope,
	bool bIncludeSamples,
	FBlueprintHelperReferenceContextPack& OutContext,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	OutContext = FBlueprintHelperReferenceContextPack();
	OutErrorCode.Empty();
	OutErrorMessage.Empty();

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FName TargetPackageName;
	FAssetData TargetAssetData;
	if (!FBlueprintHelperDependencyAnalysisServiceLocalUtils::TryResolveTargetAsset(
		AssetRegistry,
		Target.AssetPath,
		TargetPackageName,
		TargetAssetData,
		OutErrorCode,
		OutErrorMessage))
	{
		return false;
	}

	const FString EffectiveScope = FBlueprintHelperDependencyAnalysisServiceLocalUtils::NormalizeScope(Scope);
	const int32 EffectiveMaxResults = FBlueprintHelperDependencyAnalysisServiceLocalUtils::ClampMaxResults(Options.MaxResultCount);
	bool bTruncated = false;

	TArray<FName> HardDependencies;
	TArray<FName> SoftDependencies;
	TArray<FName> HardReferencers;
	TArray<FName> SoftReferencers;

	if (FBlueprintHelperDependencyAnalysisServiceLocalUtils::ShouldReadDependencies(EffectiveScope) && Options.bIncludeHardReferences)
	{
		AssetRegistry.GetDependencies(
			TargetPackageName,
			HardDependencies,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
	}
	if (FBlueprintHelperDependencyAnalysisServiceLocalUtils::ShouldReadDependencies(EffectiveScope) && Options.bIncludeSoftReferences)
	{
		AssetRegistry.GetDependencies(
			TargetPackageName,
			SoftDependencies,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Soft));
	}

	if (FBlueprintHelperDependencyAnalysisServiceLocalUtils::ShouldReadReferencers(EffectiveScope))
	{
		AssetRegistry.GetReferencers(
			TargetPackageName,
			HardReferencers,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
		AssetRegistry.GetReferencers(
			TargetPackageName,
			SoftReferencers,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Soft));
	}

	OutContext.ContextId = FString::Printf(
		TEXT("refctx_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	OutContext.Analysis.Scope = EffectiveScope;
	OutContext.Analysis.MaxResults = EffectiveMaxResults;
	OutContext.Analysis.UnsupportedChecks.Reset();

	if (FBlueprintHelperDependencyAnalysisServiceLocalUtils::ShouldReadExternalDependents(EffectiveScope))
	{
		FBlueprintHelperDependencyAnalysisServiceLocalUtils::AddUnsupportedChecksForOptions(Options, OutContext.Analysis.UnsupportedChecks);
	}

	int32 DependencyCount = 0;
	FBlueprintHelperDependencyAnalysisServiceLocalUtils::AppendPackageRefs(
		AssetRegistry,
		HardDependencies,
		TEXT("hard_package"),
		EffectiveMaxResults,
		bIncludeSamples,
		DependencyCount,
		OutContext.Dependencies,
		bTruncated);
	FBlueprintHelperDependencyAnalysisServiceLocalUtils::AppendPackageRefs(
		AssetRegistry,
		SoftDependencies,
		TEXT("soft_package"),
		EffectiveMaxResults,
		bIncludeSamples,
		DependencyCount,
		OutContext.Dependencies,
		bTruncated);

	int32 ReferencerCount = 0;
	FBlueprintHelperDependencyAnalysisServiceLocalUtils::AppendPackageRefs(
		AssetRegistry,
		HardReferencers,
		TEXT("hard_package"),
		EffectiveMaxResults,
		bIncludeSamples,
		ReferencerCount,
		OutContext.Referencers,
		bTruncated);
	FBlueprintHelperDependencyAnalysisServiceLocalUtils::AppendPackageRefs(
		AssetRegistry,
		SoftReferencers,
		TEXT("soft_package"),
		EffectiveMaxResults,
		bIncludeSamples,
		ReferencerCount,
		OutContext.Referencers,
		bTruncated);

	OutContext.Summary.DependencyCount = DependencyCount;
	OutContext.Summary.ReferencerCount = ReferencerCount;
	OutContext.Summary.ExternalDependentCount = 0;
	OutContext.Summary.BlockingDependentCount = 0;
	OutContext.Summary.WarningCount = OutContext.Analysis.UnsupportedChecks.Num() > 0 ? 1 : 0;
	OutContext.Analysis.bPartial = OutContext.Analysis.UnsupportedChecks.Num() > 0;
	OutContext.Analysis.bTruncated = bTruncated;
	OutContext.AgentHints.bCanEditSafely =
		OutContext.Summary.ReferencerCount == 0 &&
		OutContext.Summary.ExternalDependentCount == 0;
	OutContext.AgentHints.bRequiresPreview = true;
	OutContext.AgentHints.RecommendedTaskStrategy = TEXT("preview_before_write");
	if (OutContext.Summary.ReferencerCount > 0)
	{
		OutContext.AgentHints.Blockers.Add(TEXT("external_referencers_exist"));
	}
	if (OutContext.Summary.BlockingDependentCount > 0)
	{
		OutContext.AgentHints.Blockers.Add(TEXT("blocking_dependents_exist"));
	}

	return true;
}
