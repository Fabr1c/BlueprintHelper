// BlueprintHelper Review adapter registry.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewEvidenceAdapter.h"
#include "Systems/Review/BlueprintHelperReviewRestoreAdapter.h"

struct FBlueprintHelperReviewAdapterRegistryDiagnostic
{
	FString Code;
	FString Message;
};

struct FBlueprintHelperReviewRestoreAdapterLookup
{
	bool bAvailable = false;
	FString TargetKind;
	FString Message;
	TSharedPtr<IBlueprintHelperReviewRestoreAdapter> Adapter;
	TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic> Diagnostics;
};

struct FBlueprintHelperReviewEvidenceAdapterLookup
{
	bool bAvailable = false;
	FString TargetKind;
	FString Message;
	TSharedPtr<IBlueprintHelperReviewEvidenceAdapter> Adapter;
	TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewAdapterRegistry
{
public:
	static TSharedRef<FBlueprintHelperReviewAdapterRegistry> CreateDefault();

	bool RegisterEvidenceAdapter(
		const TSharedRef<IBlueprintHelperReviewEvidenceAdapter>& Adapter,
		TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic>& OutDiagnostics);

	bool RegisterRestoreAdapter(
		const TSharedRef<IBlueprintHelperReviewRestoreAdapter>& Adapter,
		TArray<FBlueprintHelperReviewAdapterRegistryDiagnostic>& OutDiagnostics);

	FBlueprintHelperReviewEvidenceAdapterLookup FindEvidenceAdapter(const FString& TargetKind) const;
	FBlueprintHelperReviewRestoreAdapterLookup FindRestoreAdapter(const FString& TargetKind) const;

	void RegisterBuiltInAdapters();

private:
	static FString NormalizeTargetKind(const FString& TargetKind);
	static FBlueprintHelperReviewAdapterRegistryDiagnostic MakeDiagnostic(
		const FString& Code,
		const FString& Message);

	TMap<FString, TSharedPtr<IBlueprintHelperReviewEvidenceAdapter>> EvidenceAdaptersByTargetKind;
	TMap<FString, TSharedPtr<IBlueprintHelperReviewRestoreAdapter>> RestoreAdaptersByTargetKind;
};
