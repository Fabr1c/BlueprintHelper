#include "Systems/Editor/BlueprintHelperEditorCloseSafetyGate.h"

#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"

TSharedRef<FJsonObject> FBlueprintHelperEditorCloseSafetyGateResult::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("can_proceed"), bCanProceed);
	Json->SetStringField(TEXT("code"), Code);
	Json->SetStringField(TEXT("message"), Message);
	if (SourceControlJson.IsValid())
	{
		Json->SetObjectField(TEXT("source_control_gate"), SourceControlJson.ToSharedRef());
	}
	return Json;
}

bool FBlueprintHelperEditorCloseSafetyGate::ShouldAttemptAutoCheckout(
	const FBlueprintHelperSourceControlResult& SourceControlResult) const
{
	if (SourceControlResult.Status == TEXT("checkout_required"))
	{
		return true;
	}

	for (const FBlueprintHelperSourceControlFileState& File : SourceControlResult.Files)
	{
		if (File.Status == TEXT("checkout_required") && File.bCanCheckOut)
		{
			return true;
		}
	}
	return false;
}

FBlueprintHelperEditorCloseSafetyGateResult FBlueprintHelperEditorCloseSafetyGate::EvaluateDirtyPackageStatus(
	const FBlueprintHelperSourceControlResult& SourceControlResult) const
{
	FBlueprintHelperEditorCloseSafetyGateResult Result;
	Result.SourceControlJson = SourceControlResult.ToJson();

	if (!SourceControlResult.BlocksAgentEdit())
	{
		Result.bCanProceed = true;
		Result.Code = TEXT("editable");
		Result.Message = TEXT("Dirty packages are editable for close_editor(save_all=true).");
		return Result;
	}

	Result.bCanProceed = false;
	Result.Code = !SourceControlResult.ErrorCode.IsEmpty()
		? SourceControlResult.ErrorCode
		: (!SourceControlResult.Status.IsEmpty() ? SourceControlResult.Status : TEXT("source_control_gate_failed"));
	Result.Message = SourceControlResult.RecommendedAction.IsEmpty()
		? TEXT("Run blueprinthelper_source_control_checkout for dirty assets before close_editor(save_all=true).")
		: SourceControlResult.RecommendedAction;
	return Result;
}
