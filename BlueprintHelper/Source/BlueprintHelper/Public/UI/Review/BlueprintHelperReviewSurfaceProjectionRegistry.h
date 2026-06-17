// BlueprintHelper Review surface projection registry.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionAdapter.h"

struct FBlueprintHelperReviewSurfaceProjectionLookup
{
	bool bAvailable = false;
	FString ProjectionKey;
	FString Message;
	TSharedPtr<IBlueprintHelperReviewSurfaceProjectionAdapter> Adapter;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceProjectionRegistry
{
public:
	static TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> CreateDefault();

	bool RegisterProjectionAdapter(
		const TSharedRef<IBlueprintHelperReviewSurfaceProjectionAdapter>& Adapter,
		TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics);

	FBlueprintHelperReviewSurfaceProjectionLookup FindProjectionAdapter(
		const FBlueprintHelperReviewTargetIdentity& Identity) const;

	TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> ProjectVisibleChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FString& AssetKind,
		const FString& SurfaceKind) const;

	void RegisterBuiltInAdapters();

	static FString MakeProjectionKey(
		const FString& AssetKind,
		const FString& SurfaceKind,
		const FString& TargetKind);

private:
	static FString NormalizeKeyPart(const FString& Value);
	static FBlueprintHelperDiagnosticItem MakeDiagnostic(
		const FString& Code,
		const FString& Message);

	TMap<FString, TSharedPtr<IBlueprintHelperReviewSurfaceProjectionAdapter>> AdaptersByProjectionKey;
};
