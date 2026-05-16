// BlueprintHelper GraphStatement FBlueprintHelperGraphFragmentEvidenceUtils declarations.

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperGraphFragmentEvidenceUtils
{
public:
	static FString NormalizeEvidenceId(const FString& Value);
	static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values);
	static TArray<TSharedPtr<FJsonValue>> StringMapToJsonArray(const TMap<FString, FString>& Values);
	static FString FragmentDiagnosticSeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity);
	static bool IsEmptyOrWhitespace(const FString& Value);
	static void AddUniqueTrimmed(TArray<FString>& Values, TSet<FString>& SeenValues, const FString& Value);
	static void AppendMetadata(TMap<FString, FString>& Target, const TMap<FString, FString>& Source);
	static bool TryReadMetadata(
			const TMap<FString, FString>& Metadata,
			const FString& Key,
			FString& OutValue);
	static FString ReadFirstMetadata(
			const TMap<FString, FString>& Metadata,
			const TArray<FString>& Keys);
	static bool IsDebugMetadataKey(const FString& Key);
	static void AppendDebugMetadataFromDag(
			TMap<FString, FString>& OutDebugMetadata,
			const TMap<FString, FString>& DagMetadata);
	static FString ChooseReviewScopeName(
			EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind,
			const TMap<FString, FString>& Metadata);
	static EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ChooseReviewScopeKind(
			const TMap<FString, FString>& Metadata);
	static FBlueprintHelperGraphFragmentEvidenceReviewScope MakeReviewScopeFromDagMetadata(
			const FBlueprintHelperGraphFragmentDag& Dag);
	static FBlueprintHelperGraphFragmentEvidenceReviewScope MakeReviewScope(
			const FBlueprintHelperGraphFragmentDag& Dag,
			const FBlueprintHelperGraphFragmentEvidenceBuildOptions& Options);
	static const FBlueprintHelperGraphFragmentRef* FindFragmentByPath(
			const FBlueprintHelperGraphFragmentDag& Dag,
			const FString& Path);
	static FString MakeBundleId(
			const FBlueprintHelperGraphFragmentDag& Dag,
			const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope);
	static void FillScopeCoverage(
			FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope,
			const FBlueprintHelperGraphFragmentEvidenceBundle& Bundle);
};
