#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionModel.h"

namespace BlueprintHelper::GraphLayout
{
enum class EGraphLayoutPreviewInteractionApplyStatus : uint8
{
	Applied,
	NoPendingChanges,
	Failed
};

struct FGraphLayoutPreviewInteractionApplyResult
{
	EGraphLayoutPreviewInteractionApplyStatus Status = EGraphLayoutPreviewInteractionApplyStatus::Failed;
	FString UpdatedRuleSetJson;
	FString Message;

	bool WasApplied() const
	{
		return Status == EGraphLayoutPreviewInteractionApplyStatus::Applied;
	}
};

class BLUEPRINTHELPER_API FGraphLayoutPreviewInteractionCommitCoordinator
{
public:
	void Reset();
	bool HasPendingChanges() const;
	void Append(const FGraphLayoutPreviewInteractionCommit& Commit);
	FGraphLayoutPreviewInteractionApplyResult ConsumePendingRuleSetJson(
		const FString& CurrentRuleSetJson,
		ESemanticScene CurrentScene);

private:
	FGraphLayoutPreviewInteractionCommitAccumulator PendingCommit;
};
}
