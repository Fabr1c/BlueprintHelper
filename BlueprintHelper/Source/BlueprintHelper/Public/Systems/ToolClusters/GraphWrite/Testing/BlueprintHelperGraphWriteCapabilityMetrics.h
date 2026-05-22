#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperGraphWriteCapabilityErrorKind : uint8
{
	None,
	SetupFailure,
	MissingRequiredEvidence,
	CandidateThresholdExceeded,
	AmbiguousCandidates,
	NotFound,
	UnsupportedIntent,
	SpawnOrLinkFailure,
	SilentWrongGraph,
	NotRun
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteCapabilityCaseResult
{
	FString CaseName;
	FString Phase;
	FString Capability;
	EBlueprintHelperGraphWriteCapabilityErrorKind ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun;
	bool bCallCorrect = false;
	bool bGraphWriteCorrect = false;
	FString EvidenceSummary;
	FString DebugBundlePath;
	FString GapUpdate;
	FString SemanticKind;
	FString ClusterKind;
	FString ResolverStatus;
	FString SelectedStableId;
	FString SelectedSpawnerClass;
	int32 CandidateCount = 0;
	TArray<FString> MissingEvidenceFields;
	FString SpawnedNodeClass;
	FString PinDefaultLinkReadbackSummary;
	bool bHasResolverEvidence = false;
	bool bHasSpawnEvidence = false;
	bool bReadbackComplete = false;
	int32 CapabilityItemCount = 1;
	int32 CoveredCapabilityItemCount = 0;
	int32 GraphWriteCheckCount = 0;
	int32 CorrectGraphWriteCheckCount = 0;
	bool bUseExplicitCallSampleCounts = false;
	int32 CallSampleCount = 0;
	int32 CorrectCallSampleCount = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteCapabilitySummary
{
	int32 CapabilityItemsPlanned = 0;
	int32 CapabilityItemsCovered = 0;
	int32 GraphWriteCasesRun = 0;
	int32 GraphWriteCasesCorrect = 0;
	int32 CallSamplesRun = 0;
	int32 CallSamplesCorrect = 0;
	int32 SilentWrongGraphCount = 0;

	double CapabilityCoverageRate() const;
	double GraphWriteCorrectRate() const;
	double CallCorrectRate() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteCapabilityMetrics
{
public:
	static const TCHAR* ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind Kind);
	static FBlueprintHelperGraphWriteCapabilityCaseResult NormalizeReadbackClassification(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result);
	static FBlueprintHelperGraphWriteCapabilitySummary Summarize(const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult>& Results);
	static FString ToMarkdownRow(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result);
	static TSharedRef<FJsonObject> ToDebugBundleFailureSummary(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result);
};
