// DebugCase export projection adapter contract.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"

class FBlueprintHelperReviewStoreService;

enum class EBlueprintHelperDebugCaseArtifactRole : uint8
{
	Generic,
	SummaryMarkdown,
	ReviewSummary,
	FragmentDag,
	FragmentEvidence,
	FragmentSummary
};

struct FBlueprintHelperDebugCaseArtifactModel
{
	FString ArtifactId;
	FString RelativePath;
	FString Schema;
	FString DisplayName;
	EBlueprintHelperDebugCaseArtifactRole Role = EBlueprintHelperDebugCaseArtifactRole::Generic;
	TSharedPtr<FJsonObject> Json;
	FString Markdown;
};

struct FBlueprintHelperDebugCaseProjectionContext
{
	const FBlueprintHelperReviewStoreService* ReviewStore = nullptr;
};

struct FBlueprintHelperDebugCaseProjectionResult
{
	TArray<FBlueprintHelperDebugCaseArtifactModel> Artifacts;
	TArray<FBlueprintHelperDebugSkippedArtifact> SkippedArtifacts;
	FBlueprintHelperDebugFragmentArtifactRefs FragmentArtifacts;
};

class BLUEPRINTHELPER_API IBlueprintHelperDebugCaseProjectionAdapter
{
public:
	virtual ~IBlueprintHelperDebugCaseProjectionAdapter();

	virtual FString GetProjectionId() const = 0;
	virtual bool BuildArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		const FBlueprintHelperDebugCaseProjectionContext& Context,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const = 0;
};
