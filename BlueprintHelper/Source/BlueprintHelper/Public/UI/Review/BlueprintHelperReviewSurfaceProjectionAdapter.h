// BlueprintHelper Review surface projection adapter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewTargetIdentity.h"
#include "UI/Review/BlueprintHelperReviewSurfaceDiffModel.h"

struct FBlueprintHelperReviewSurfaceProjectionResult
{
	bool bProjected = false;
	FString Message;
	TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> DiffModels;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
};

class BLUEPRINTHELPER_API IBlueprintHelperReviewSurfaceProjectionAdapter
{
public:
	virtual ~IBlueprintHelperReviewSurfaceProjectionAdapter();

	virtual FString GetAssetKind() const = 0;
	virtual FString GetSurfaceKind() const = 0;
	virtual FString GetTargetKind() const = 0;
	virtual bool CanProject(const FBlueprintHelperReviewTargetIdentity& Identity) const = 0;
	virtual FBlueprintHelperReviewSurfaceProjectionResult Project(
		const FBlueprintHelperReviewVisibleChange& Change) const = 0;
};
