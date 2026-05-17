// BlueprintHelper Service Layer - internal dependency analysis types.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperReferenceContextProtocol
{
public:
	static constexpr const TCHAR* Schema = TEXT("ReferenceContextPack.v1");
};

class FBlueprintHelperDependencyAnalysisJson
{
public:
	static TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Items);
};

struct FBlueprintHelperDependencyAnalysisTarget
{
	FString AssetPath;
	FString TargetType = TEXT("asset");
	FString TargetName;
	FString GraphName;
	FString DeclaringClassPath;

	// Legacy target selectors kept for older callers. They are not echoed in ReferenceContextPack.
	FString BlockId;
	FString RowName;
	FString WidgetName;
	FString InterfacePath;
};

struct FBlueprintHelperDependencyAnalysisOptions
{
	bool bIncludeHardReferences = true;
	bool bIncludeSoftReferences = true;
	bool bAnalyzeBlueprintCalls = true;
	bool bAnalyzeWidgetBindings = true;
	bool bAnalyzeDataTableRows = true;
	bool bScanCppSource = false;
	bool bAnalyzeRuntimeStringLookup = false;
	bool bAnalyzeDynamicSoftReferences = false;
	int32 MaxResultCount = 50;

	FString LegacyScope = TEXT("safety_context");
	FString SearchScope = TEXT("project");
	FString ResolutionPolicy = TEXT("ue_then_name");
	FString Detail = TEXT("samples");
};

struct FBlueprintHelperReferenceSampleSummary
{
	FString GraphName;
	FString ReferenceKind = TEXT("unknown");

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReferenceAssetSummary
{
	FString AssetPath;
	FString AssetType = TEXT("unknown");
	int32 MatchCount = 0;
	TArray<FString> ReferenceKinds;
	FString Safety = TEXT("info");
	TArray<FBlueprintHelperReferenceSampleSummary> Samples;

	void AddReference(
		const FString& ReferenceKind,
		const FString& GraphName,
		const FString& InSafety,
		bool bIncludeSample,
		int32 MaxSamples);

	TSharedRef<FJsonObject> ToJson(bool bIncludeSamples) const;

private:
	void EscalateSafety(const FString& InSafety);
};

struct FBlueprintHelperReferenceIndexStatus
{
	int32 UnindexedCount = 0;
	int32 OutOfDateCount = 0;
	int32 FailedCount = 0;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReferenceContextSummary
{
	int32 AssetCount = 0;
	int32 ReferenceCount = 0;
	int32 BlockingCount = 0;
	int32 WarningCount = 0;
	bool bPartial = false;
	bool bTruncated = false;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReferenceContextAgentHints
{
	bool bCanEditSafely = true;
	bool bRequiresPreview = true;
	FString RecommendedTaskStrategy = TEXT("preview_before_write");
	TArray<FString> Blockers;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReferenceContextPack
{
	FString ContextId;
	FBlueprintHelperReferenceContextSummary Summary;
	FBlueprintHelperReferenceIndexStatus IndexStatus;
	TArray<FBlueprintHelperReferenceAssetSummary> Dependencies;
	TArray<FBlueprintHelperReferenceAssetSummary> Referencers;
	FBlueprintHelperReferenceContextAgentHints AgentHints;
	TArray<FString> UnsupportedChecks;

	TSharedRef<FJsonObject> ToJson() const;
};
