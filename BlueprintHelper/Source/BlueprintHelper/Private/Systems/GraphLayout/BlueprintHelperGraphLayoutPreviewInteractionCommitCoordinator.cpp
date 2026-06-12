#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionCommitCoordinator.h"

namespace BlueprintHelper::GraphLayout
{
void FGraphLayoutPreviewInteractionCommitCoordinator::Reset()
{
	PendingCommit.Reset();
}

bool FGraphLayoutPreviewInteractionCommitCoordinator::HasPendingChanges() const
{
	return PendingCommit.HasPendingChanges();
}

void FGraphLayoutPreviewInteractionCommitCoordinator::Append(const FGraphLayoutPreviewInteractionCommit& Commit)
{
	PendingCommit.Append(Commit);
}

FGraphLayoutPreviewInteractionApplyResult FGraphLayoutPreviewInteractionCommitCoordinator::ConsumePendingRuleSetJson(
	const FString& CurrentRuleSetJson,
	const ESemanticScene CurrentScene)
{
	FGraphLayoutPreviewInteractionApplyResult Result;
	if (!PendingCommit.HasPendingChanges())
	{
		Result.Status = EGraphLayoutPreviewInteractionApplyStatus::NoPendingChanges;
		Result.Message = TEXT("没有待应用的预览修改。");
		return Result;
	}

	FString UpdatedJson;
	FString Error;
	if (!FGraphLayoutPreviewInteractionModel::BuildRuleSetJsonForCommit(
		CurrentRuleSetJson,
		CurrentScene,
		PendingCommit.GetCommit(),
		UpdatedJson,
		Error))
	{
		Result.Status = EGraphLayoutPreviewInteractionApplyStatus::Failed;
		Result.Message = Error.IsEmpty() ? TEXT("预览拖拽提交失败。") : Error;
		return Result;
	}

	PendingCommit.Reset();
	Result.Status = EGraphLayoutPreviewInteractionApplyStatus::Applied;
	Result.UpdatedRuleSetJson = MoveTemp(UpdatedJson);
	Result.Message = TEXT("已从预览更新并保存 RuleSet。");
	return Result;
}
}
