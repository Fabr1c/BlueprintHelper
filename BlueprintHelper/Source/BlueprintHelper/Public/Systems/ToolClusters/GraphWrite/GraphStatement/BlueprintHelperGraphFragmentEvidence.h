#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"

class FJsonObject;

enum class EBlueprintHelperGraphFragmentEvidenceReviewScopeKind : uint8
{
	Unknown,
	Function,
	Event,
	Macro
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceDiagnostic
{
	FString Code;
	FString Path;
	FString Message;
	EBlueprintHelperGraphFragmentDiagnosticSeverity Severity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;
	FString FragmentId;
	FString SourceStatementId;
	FString ReviewScopeId;
	TMap<FString, FString> DebugMetadata;

	bool IsError() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceRef
{
	FString FragmentId;
	FString SourceStatementId;
	FString Path;
	FString Kind;
	FString ReviewScopeId;
	TMap<FString, FString> DebugMetadata;

	bool IsValid() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceReviewScope
{
	FString ScopeId;
	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind = EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown;
	FString ScopeName;
	FString GraphName;
	FString FunctionName;
	FString EventName;
	FString MacroName;
	TArray<FString> SourceStatementIds;
	TArray<FString> FragmentIds;
	TArray<FBlueprintHelperGraphFragmentEvidenceDiagnostic> Diagnostics;
	TMap<FString, FString> Metadata;
	TMap<FString, FString> DebugMetadata;

	bool IsValid() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceBundle
{
	FString Schema = TEXT("blueprint_helper.graph_fragment_evidence.v1");
	FString BundleId;
	FString DagSchema;
	TArray<FBlueprintHelperGraphFragmentEvidenceReviewScope> ReviewScopes;
	TArray<FBlueprintHelperGraphFragmentEvidenceRef> Fragments;
	TArray<FString> SourceStatementIds;
	TArray<FString> FragmentIds;
	TArray<FBlueprintHelperGraphFragmentEvidenceDiagnostic> Diagnostics;
	TMap<FString, FString> Metadata;
	TMap<FString, FString> DebugMetadata;

	void Reset();
	bool IsEmpty() const;
	bool HasErrors() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceBuildOptions
{
	FString BundleId;
	bool bUseReviewScopeHint = false;
	FBlueprintHelperGraphFragmentEvidenceReviewScope ReviewScopeHint;
	TMap<FString, FString> Metadata;
	TMap<FString, FString> DebugMetadata;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentEvidenceBuilder
{
public:
	static FBlueprintHelperGraphFragmentEvidenceBundle BuildFromDag(
		const FBlueprintHelperGraphFragmentDag& Dag);

	static FBlueprintHelperGraphFragmentEvidenceBundle BuildFromDag(
		const FBlueprintHelperGraphFragmentDag& Dag,
		const FBlueprintHelperGraphFragmentEvidenceBuildOptions& Options);

	static FString ReviewScopeKindToString(
		EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind);

	static EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ParseReviewScopeKind(
		const FString& ScopeKind);

	static FString MakeReviewScopeId(
		EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind,
		const FString& ScopeName,
		const FString& GraphName = TEXT(""));
};
