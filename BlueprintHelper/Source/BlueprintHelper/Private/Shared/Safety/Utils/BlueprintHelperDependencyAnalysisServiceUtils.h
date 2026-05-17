// BlueprintHelper dependency analysis service utilities.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Shared/BlueprintHelperDependencyAnalysisTypes.h"

class IAssetRegistry;
class UBlueprint;
class UClass;
class UEdGraph;
class UWidgetBlueprint;
class UK2Node_BaseMCDelegate;
class UK2Node_Variable;
struct FDelegateEditorBinding;

class FBlueprintHelperDependencyAnalysisServiceUtils
{
public:
	static constexpr int32 DefaultMaxResults = 50;
	static constexpr int32 MaxAllowedResults = 500;
	static constexpr int32 MaxSamplesPerAsset = 3;

	static FString NormalizeLegacyScope(const FString& Scope);
	static FString NormalizeSearchScope(const FString& SearchScope);
	static FString NormalizeResolutionPolicy(const FString& ResolutionPolicy);
	static FString NormalizeDetail(const FString& Detail);
	static FString NormalizeTargetType(const FString& TargetType);

	static bool ShouldReadDependencies(const FString& Scope);
	static bool ShouldReadReferencers(const FString& Scope);
	static bool IsMemberTarget(const FString& TargetType);

	static int32 ClampMaxResults(int32 MaxResults);

	static bool ShouldIncludeSamples(const FString& Detail);

	static FString PackageNameFromAssetPath(const FString& AssetPath);

	static FString ObjectPathFromPackageName(const FString& PackageName);

	static FString AssetPathFromDataOrPackage(const FAssetData& AssetData,
		const FName& PackageName);

	static FString AssetTypeFromData(const FAssetData& AssetData);

	static bool TryFindAssetData(IAssetRegistry& Registry,
		const FName PackageName,
		FAssetData& OutAssetData);

	static bool TryResolveTargetAsset(IAssetRegistry& Registry,
		const FString& AssetPath,
		FName& OutPackageName,
		FAssetData& OutAssetData,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static UBlueprint* LoadBlueprintFromAssetData(const FAssetData& AssetData);
	static UBlueprint* LoadBlueprintFromPath(const FString& AssetPath);

	static UClass* BlueprintReferenceClass(UBlueprint* Blueprint);
	static UClass* ResolveClassPath(const FString& ClassPath);

	static UClass* ResolveTargetClass(
		const FBlueprintHelperDependencyAnalysisTarget& Target);

	static bool ClassesMatch(UClass* CandidateClass, UClass* TargetClass);

	static bool MatchesResolution(const FString& ResolutionPolicy,
		UClass* CandidateClass,
		UClass* TargetClass,
		bool& bOutUsedFallback);

	static void AddUnsupportedCheck(TArray<FString>& UnsupportedChecks,
		const FString& Check);

	static FBlueprintHelperReferenceAssetSummary& FindOrAddSummary(
		TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
		const FString& AssetPath, const FString& AssetType);

	static void AddAggregateReference(
		TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
		const FString& AssetPath, const FString& AssetType,
		const FString& ReferenceKind, const FString& GraphName,
		const FString& Safety, bool bIncludeSamples);

	static FBlueprintHelperReferenceAssetSummary
	MakePackageSummary(IAssetRegistry& Registry, const FName PackageName,
		const FString& ReferenceKind, const FString& Safety);

	static void AppendPackageRefs(IAssetRegistry& Registry,
		const TArray<FName>& PackageNames,
		const FString& ReferenceKind,
		const FString& Safety,
		int32 MaxResults,
		TArray<FBlueprintHelperReferenceAssetSummary>& OutSamples,
		bool& bOutTruncated);

	static void GatherBlueprintCandidates(IAssetRegistry& Registry,
		const FString& SearchScope,
		const FName TargetPackageName,
		const FAssetData& TargetAssetData,
		TArray<FAssetData>& OutCandidates);

	static FString DelegateReferenceKind(const UK2Node_BaseMCDelegate* DelegateNode);

	static FString EventReferenceKind(const FString& TargetType);

	static FString VariableReferenceKind(const FString& TargetType,
		const UK2Node_Variable* VariableNode);

	static bool MatchesVariableScope(
		const FBlueprintHelperDependencyAnalysisTarget& Target,
		const UK2Node_Variable* VariableNode,
		const UEdGraph* Graph);

	static FName ResolveBindingFunctionName(
		const FDelegateEditorBinding& Binding,
		const UWidgetBlueprint* WidgetBlueprint);

	static FName ResolveBindingSourcePropertyName(
		const FDelegateEditorBinding& Binding,
		const UWidgetBlueprint* WidgetBlueprint);

	static bool BindingSourcePathContainsMember(
		const FDelegateEditorBinding& Binding,
		const FName TargetName);

	static bool BindingMatchesMemberVariable(
		const FDelegateEditorBinding& Binding,
		const UWidgetBlueprint* WidgetBlueprint,
		const FName TargetName);

	static void ScanWidgetBlueprintBindingsForMemberReferences(
		const FBlueprintHelperDependencyAnalysisTarget& Target,
		const FString& TargetType, UWidgetBlueprint* WidgetBlueprint,
		const FString& AssetPath, const FString& AssetType, bool bIncludeSamples,
		TMap<FString, FBlueprintHelperReferenceAssetSummary>& OutReferencers);

	static void ScanBlueprintForMemberReferences(
		const FBlueprintHelperDependencyAnalysisTarget& Target,
		const FString& TargetType, const FString& ResolutionPolicy,
		UClass* TargetClass, UBlueprint* Blueprint, const FString& AssetPath,
		const FString& AssetType, bool bIncludeSamples,
		bool bAnalyzeWidgetBindings,
		TMap<FString, FBlueprintHelperReferenceAssetSummary>& OutReferencers,
		TArray<FString>& UnsupportedChecks, bool& bOutUsedFallback);

	static void EmitAggregatedSummaries(
		const TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
		int32 MaxResults,
		TArray<FBlueprintHelperReferenceAssetSummary>& OutSummaries,
		bool& bOutTruncated);

	static void FillSummaryAndHints(FBlueprintHelperReferenceContextPack& Context);
};
