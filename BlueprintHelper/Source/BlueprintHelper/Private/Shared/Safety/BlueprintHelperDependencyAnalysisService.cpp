// BlueprintHelper Service Layer - internal dependency analysis service.

#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "Shared/Safety/Utils/BlueprintHelperDependencyAnalysisServiceUtils.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "FindInBlueprintManager.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

bool FBlueprintHelperDependencyAnalysisService::TryBuildReferenceContext(
	const FBlueprintHelperDependencyAnalysisTarget& Target,
	const FBlueprintHelperDependencyAnalysisOptions& Options,
	FBlueprintHelperReferenceContextPack& OutContext, FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	OutContext = FBlueprintHelperReferenceContextPack();
	OutErrorCode.Empty();
	OutErrorMessage.Empty();

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FName TargetPackageName;
	FAssetData TargetAssetData;
	if (!FBlueprintHelperDependencyAnalysisServiceUtils::TryResolveTargetAsset(
			AssetRegistry, Target.AssetPath, TargetPackageName, TargetAssetData,
			OutErrorCode, OutErrorMessage))
	{
		return false;
	}

	const FString TargetType =
		FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeTargetType(
			Target.TargetType);
	const FString EffectiveScope =
		FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeLegacyScope(
			Options.LegacyScope);
	const FString SearchScope =
		FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeSearchScope(
			Options.SearchScope);
	const FString ResolutionPolicy =
		FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeResolutionPolicy(
			Options.ResolutionPolicy);
	const FString Detail =
		FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeDetail(
			Options.Detail);
	const int32 EffectiveMaxResults =
		FBlueprintHelperDependencyAnalysisServiceUtils::ClampMaxResults(
			Options.MaxResultCount);
	const bool bIncludeSamples =
		FBlueprintHelperDependencyAnalysisServiceUtils::ShouldIncludeSamples(
			Detail);
	bool bTruncated = false;

	OutContext.ContextId = FString::Printf(
		TEXT("refctx_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	OutContext.IndexStatus.UnindexedCount =
		FFindInBlueprintSearchManager::Get().GetNumberUnindexedAssets();
	OutContext.IndexStatus.OutOfDateCount = 0;
	OutContext.IndexStatus.FailedCount =
		FFindInBlueprintSearchManager::Get().GetFailedToCacheCount();

	if ((TargetType == TEXT("asset") ||
		 !FBlueprintHelperDependencyAnalysisServiceUtils::IsMemberTarget(
			 TargetType)) &&
		FBlueprintHelperDependencyAnalysisServiceUtils::ShouldReadDependencies(
			EffectiveScope))
	{
		TArray<FName> HardDependencies;
		TArray<FName> SoftDependencies;
		if (Options.bIncludeHardReferences)
		{
			AssetRegistry.GetDependencies(
				TargetPackageName, HardDependencies,
				UE::AssetRegistry::EDependencyCategory::Package,
				UE::AssetRegistry::FDependencyQuery(
					UE::AssetRegistry::EDependencyQuery::Hard));
		}
		if (Options.bIncludeSoftReferences)
		{
			AssetRegistry.GetDependencies(
				TargetPackageName, SoftDependencies,
				UE::AssetRegistry::EDependencyCategory::Package,
				UE::AssetRegistry::FDependencyQuery(
					UE::AssetRegistry::EDependencyQuery::Soft));
		}

		FBlueprintHelperDependencyAnalysisServiceUtils::AppendPackageRefs(
			AssetRegistry, HardDependencies, TEXT("asset_reference"), TEXT("info"),
			EffectiveMaxResults, OutContext.Dependencies, bTruncated);
		FBlueprintHelperDependencyAnalysisServiceUtils::AppendPackageRefs(
			AssetRegistry, SoftDependencies, TEXT("asset_reference"), TEXT("info"),
			EffectiveMaxResults, OutContext.Dependencies, bTruncated);
	}

	if ((TargetType == TEXT("asset") ||
		 !FBlueprintHelperDependencyAnalysisServiceUtils::IsMemberTarget(
			 TargetType)) &&
		FBlueprintHelperDependencyAnalysisServiceUtils::ShouldReadReferencers(
			EffectiveScope))
	{
		TArray<FName> HardReferencers;
		TArray<FName> SoftReferencers;
		AssetRegistry.GetReferencers(
			TargetPackageName, HardReferencers,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(
				UE::AssetRegistry::EDependencyQuery::Hard));
		AssetRegistry.GetReferencers(
			TargetPackageName, SoftReferencers,
			UE::AssetRegistry::EDependencyCategory::Package,
			UE::AssetRegistry::FDependencyQuery(
				UE::AssetRegistry::EDependencyQuery::Soft));

		FBlueprintHelperDependencyAnalysisServiceUtils::AppendPackageRefs(
			AssetRegistry, HardReferencers, TEXT("asset_reference"),
			TEXT("blocking"), EffectiveMaxResults, OutContext.Referencers,
			bTruncated);
		FBlueprintHelperDependencyAnalysisServiceUtils::AppendPackageRefs(
			AssetRegistry, SoftReferencers, TEXT("asset_reference"),
			TEXT("warning"), EffectiveMaxResults, OutContext.Referencers,
			bTruncated);
	}

	if (FBlueprintHelperDependencyAnalysisServiceUtils::IsMemberTarget(
			TargetType))
	{
		if (Target.TargetName.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_request");
			OutErrorMessage =
				TEXT("target_name is required for member-level reference context.");
			return false;
		}
		if (TargetType == TEXT("local_variable") && Target.GraphName.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_request");
			OutErrorMessage =
				TEXT("graph_name is required for local_variable reference context.");
			return false;
		}

		UClass* TargetClass =
			FBlueprintHelperDependencyAnalysisServiceUtils::ResolveTargetClass(
				Target);
		if (ResolutionPolicy == TEXT("ue_only") && !TargetClass)
		{
			FBlueprintHelperDependencyAnalysisServiceUtils::AddUnsupportedCheck(
				OutContext.UnsupportedChecks, TEXT("target_resolution_failed"));
		}

		TArray<FAssetData> CandidateAssets;
		FBlueprintHelperDependencyAnalysisServiceUtils::GatherBlueprintCandidates(
			AssetRegistry, SearchScope, TargetPackageName, TargetAssetData,
			CandidateAssets);

		TMap<FString, FBlueprintHelperReferenceAssetSummary> AggregatedReferencers;
		bool bUsedFallback = false;
		int32 FailedLoadCount = 0;
		for (const FAssetData& CandidateAsset : CandidateAssets)
		{
			UBlueprint* CandidateBlueprint =
				FBlueprintHelperDependencyAnalysisServiceUtils::
					LoadBlueprintFromAssetData(CandidateAsset);
			if (!CandidateBlueprint)
			{
				++FailedLoadCount;
				continue;
			}
			FBlueprintHelperDependencyAnalysisServiceUtils::
				ScanBlueprintForMemberReferences(
					Target, TargetType, ResolutionPolicy, TargetClass,
					CandidateBlueprint, CandidateAsset.GetObjectPathString(),
					FBlueprintHelperDependencyAnalysisServiceUtils::AssetTypeFromData(
						CandidateAsset),
					bIncludeSamples, Options.bAnalyzeWidgetBindings,
					AggregatedReferencers, OutContext.UnsupportedChecks,
					bUsedFallback);
		}
		if (CandidateAssets.Num() == 0 && SearchScope == TEXT("asset"))
		{
			if (UBlueprint* TargetBlueprint =
					FBlueprintHelperDependencyAnalysisServiceUtils::
						LoadBlueprintFromPath(Target.AssetPath))
			{
				FBlueprintHelperDependencyAnalysisServiceUtils::
					ScanBlueprintForMemberReferences(
						Target, TargetType, ResolutionPolicy, TargetClass,
						TargetBlueprint, Target.AssetPath, TEXT("Blueprint"),
						bIncludeSamples, Options.bAnalyzeWidgetBindings,
						AggregatedReferencers, OutContext.UnsupportedChecks,
						bUsedFallback);
			}
		}

		if (FailedLoadCount > 0)
		{
			FBlueprintHelperDependencyAnalysisServiceUtils::AddUnsupportedCheck(
				OutContext.UnsupportedChecks,
				FString::Printf(TEXT("blueprint_load_failed:%d"), FailedLoadCount));
		}
		if (bUsedFallback)
		{
			FBlueprintHelperDependencyAnalysisServiceUtils::AddUnsupportedCheck(
				OutContext.UnsupportedChecks, TEXT("name_fallback_used"));
		}

		FBlueprintHelperDependencyAnalysisServiceUtils::EmitAggregatedSummaries(
			AggregatedReferencers, EffectiveMaxResults, OutContext.Referencers,
			bTruncated);
	}

	OutContext.Summary.bTruncated = bTruncated;
	OutContext.Summary.bPartial =
		OutContext.UnsupportedChecks.Num() > 0 ||
		OutContext.IndexStatus.FailedCount > 0 ||
		(ResolutionPolicy == TEXT("ue_only") &&
		 FBlueprintHelperDependencyAnalysisServiceUtils::IsMemberTarget(
			 TargetType) &&
		 !FBlueprintHelperDependencyAnalysisServiceUtils::ResolveTargetClass(
			 Target));
	FBlueprintHelperDependencyAnalysisServiceUtils::FillSummaryAndHints(
		OutContext);
	return true;
}
