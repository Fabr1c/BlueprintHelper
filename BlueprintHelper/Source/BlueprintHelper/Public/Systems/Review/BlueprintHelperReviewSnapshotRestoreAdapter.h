// BlueprintHelper Review snapshot restore adapter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewRestoreAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewSnapshotRestoreAdapter : public IBlueprintHelperReviewRestoreAdapter
{
public:
	explicit FBlueprintHelperReviewSnapshotRestoreAdapter(const FString& InTargetKind);

	virtual FString GetTargetKind() const override;
	virtual FBlueprintHelperReviewRestoreResult RestoreBeforeSnapshot(
		const FBlueprintHelperReviewVisibleChange& Change) const override;

	static TArray<FString> GetSupportedTargetKinds();

private:
	FString TargetKind;
};
