// Built-in DebugCase export projection adapters.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Debug/BlueprintHelperDebugCaseProjectionAdapter.h"

enum class EBlueprintHelperDebugCaseBuiltinProjectionKind : uint8
{
	DebugCaseSummary,
	EvidenceSummary,
	FragmentSummary,
	ReviewSummary
};

class BLUEPRINTHELPER_API FBlueprintHelperDebugCaseBuiltinProjectionAdapter final
	: public IBlueprintHelperDebugCaseProjectionAdapter
{
public:
	explicit FBlueprintHelperDebugCaseBuiltinProjectionAdapter(
		EBlueprintHelperDebugCaseBuiltinProjectionKind InKind);

	virtual FString GetProjectionId() const override;
	virtual bool BuildArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		const FBlueprintHelperDebugCaseProjectionContext& Context,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const override;

private:
	EBlueprintHelperDebugCaseBuiltinProjectionKind Kind;

	static void SetError(FString* OutError, const FString& Error);
	static FString MakeSafeArtifactFileName(const FString& RawId);
	static FBlueprintHelperDebugCaseSummary BuildSummary(const FBlueprintHelperDebugCase& DebugCase);
	static FString BuildMarkdownSummary(
		const FBlueprintHelperDebugCaseSummary& Summary,
		const FBlueprintHelperDebugCaseProjectionResult& ProjectionResult);
	static void BuildSkippedArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		FBlueprintHelperDebugCaseProjectionResult& OutResult);
	bool BuildDebugCaseSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const;
	bool BuildEvidenceSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const;
	bool BuildFragmentSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const;
	bool BuildReviewSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		const FBlueprintHelperDebugCaseProjectionContext& Context,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const;
};
