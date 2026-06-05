// BlueprintHelper source-control service.

#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"

#include "SourceControlHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

class FBlueprintHelperSourceControlServiceLocalUtils
{
public:
	static FString LastSourceControlError()
	{
		return USourceControlHelpers::LastErrorMsg().ToString();
	}

	static TArray<FString> NormalizeInputs(TConstArrayView<FString> Inputs)
	{
		TArray<FString> Result;
		for (const FString& Input : Inputs)
		{
			FString Trimmed = Input;
			Trimmed.TrimStartAndEndInline();
			if (!Trimmed.IsEmpty())
			{
				Result.AddUnique(Trimmed);
			}
		}
		return Result;
	}

	static FBlueprintHelperSourceControlFileState MakeDisabledFileState(const FString& Input)
	{
		FBlueprintHelperSourceControlFileState State;
		State.Input = Input;
		State.Status = TEXT("source_control_disabled");
		State.AgentHint = TEXT("Source control is not enabled; checkout is not required for this file.");
		return State;
	}

	static FBlueprintHelperSourceControlFileState MakeUnavailableFileState(const FString& Input)
	{
		FBlueprintHelperSourceControlFileState State;
		State.Input = Input;
		State.Status = TEXT("source_control_unavailable");
		State.AgentHint = TEXT("Source control is enabled but unavailable; do not edit until the provider is reachable.");
		return State;
	}

	static FBlueprintHelperSourceControlFileState FromSourceControlState(
		const FString& Input,
		const FSourceControlState& SourceState)
	{
		FBlueprintHelperSourceControlFileState State;
		State.Input = Input;
		State.Filename = SourceState.Filename;
		State.bValid = SourceState.bIsValid;
		State.bUnknown = SourceState.bIsUnknown;
		State.bSourceControlled = SourceState.bIsSourceControlled;
		State.bCanCheckOut = SourceState.bCanCheckOut;
		State.bCheckedOut = SourceState.bIsCheckedOut;
		State.bCurrent = SourceState.bIsCurrent;
		State.bAdded = SourceState.bIsAdded;
		State.bDeleted = SourceState.bIsDeleted;
		State.bIgnored = SourceState.bIsIgnored;
		State.bCanEdit = SourceState.bCanEdit;
		State.bModified = SourceState.bIsModified;
		State.bCanAdd = SourceState.bCanAdd;
		State.bConflicted = SourceState.bIsConflicted;
		State.bCanRevert = SourceState.bCanRevert;
		State.bCheckedOutOther = SourceState.bIsCheckedOutOther;
		State.CheckedOutOther = SourceState.CheckedOutOther;
		State.bCheckedOutInOtherBranch = SourceState.bIsCheckedOutInOtherBranch;
		State.bModifiedInOtherBranch = SourceState.bIsModifiedInOtherBranch;
		State.DisplayName = SourceState.bIsValid ? TEXT("source_control_state") : TEXT("invalid_source_control_state");
		FBlueprintHelperSourceControlService::ClassifyFileStateForAgent(State);
		return State;
	}

	static void AddInputFileStates(
		FBlueprintHelperSourceControlResult& Result,
		TConstArrayView<FString> Inputs,
		const FString& Status)
	{
		for (const FString& Input : Inputs)
		{
			if (Status == TEXT("source_control_disabled"))
			{
				Result.Files.Add(MakeDisabledFileState(Input));
			}
			else
			{
				Result.Files.Add(MakeUnavailableFileState(Input));
			}
		}
	}

	static FString ResolveAggregateStatus(const FBlueprintHelperSourceControlResult& Result)
	{
		if (Result.HasFileStatus(TEXT("checked_out_by_other")))
		{
			return TEXT("checked_out_by_other");
		}
		if (Result.HasFileStatus(TEXT("source_control_conflicted")))
		{
			return TEXT("source_control_conflicted");
		}
		if (Result.HasFileStatus(TEXT("not_editable")) || Result.HasFileStatus(TEXT("unknown")))
		{
			return TEXT("not_editable");
		}
		if (Result.HasFileStatus(TEXT("checkout_required")))
		{
			return TEXT("checkout_required");
		}
		if (Result.HasFileStatus(TEXT("source_control_unavailable")))
		{
			return TEXT("source_control_unavailable");
		}
		if (Result.HasFileStatus(TEXT("source_control_disabled")))
		{
			return TEXT("source_control_disabled");
		}
		return TEXT("editable");
	}

	static void FinalizeQueryMessages(FBlueprintHelperSourceControlResult& Result)
	{
		Result.Status = ResolveAggregateStatus(Result);
		if (Result.Status == TEXT("checked_out_by_other"))
		{
			Result.AgentMessage = TEXT("At least one target is checked out by another user. Stop and ask the user to resolve source control ownership before editing.");
			Result.RecommendedAction = TEXT("Report checked_out_by_other with the file owner; do not write this asset.");
		}
		else if (Result.Status == TEXT("source_control_conflicted"))
		{
			Result.AgentMessage = TEXT("At least one target is conflicted in source control. Stop before editing.");
			Result.RecommendedAction = TEXT("Ask the user to resolve source-control conflicts first.");
		}
		else if (Result.Status == TEXT("checkout_required"))
		{
			Result.AgentMessage = TEXT("At least one target must be checked out before editing.");
			Result.RecommendedAction = TEXT("Run blueprinthelper_source_control_checkout for the same targets, then retry the write.");
		}
		else if (Result.Status == TEXT("not_editable"))
		{
			Result.AgentMessage = TEXT("At least one target is not editable and cannot currently be checked out.");
			Result.RecommendedAction = TEXT("Stop and report source-control not_editable details to the user.");
		}
		else if (Result.Status == TEXT("source_control_unavailable"))
		{
			Result.AgentMessage = TEXT("Source control is enabled but unavailable.");
			Result.RecommendedAction = TEXT("Ask the user to reconnect source control before editing source-controlled assets.");
		}
		else if (Result.Status == TEXT("source_control_disabled"))
		{
			Result.AgentMessage = TEXT("Source control is not enabled; checkout is not required.");
			Result.RecommendedAction = TEXT("Continue only if project policy allows editing without source control.");
		}
		else
		{
			Result.AgentMessage = TEXT("All targets are editable.");
			Result.RecommendedAction = TEXT("Proceed with the requested edit.");
		}
	}

	static bool FileNeedsCheckout(const FBlueprintHelperSourceControlFileState& File)
	{
		return File.Status == TEXT("checkout_required") && !File.Filename.IsEmpty();
	}
};

TSharedRef<FJsonObject> FBlueprintHelperSourceControlFileState::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("input"), Input);
	if (!Filename.IsEmpty())
	{
		Json->SetStringField(TEXT("filename"), Filename);
	}
	if (!PackageName.IsEmpty())
	{
		Json->SetStringField(TEXT("package_name"), PackageName);
	}
	Json->SetStringField(TEXT("status"), Status);
	if (!AgentHint.IsEmpty())
	{
		Json->SetStringField(TEXT("agent_hint"), AgentHint);
	}
	if (!CheckedOutOther.IsEmpty())
	{
		Json->SetStringField(TEXT("checked_out_other"), CheckedOutOther);
	}
	if (!DisplayName.IsEmpty())
	{
		Json->SetStringField(TEXT("display_name"), DisplayName);
	}
	if (!DisplayTooltip.IsEmpty())
	{
		Json->SetStringField(TEXT("display_tooltip"), DisplayTooltip);
	}
	Json->SetBoolField(TEXT("valid"), bValid);
	Json->SetBoolField(TEXT("unknown"), bUnknown);
	Json->SetBoolField(TEXT("source_controlled"), bSourceControlled);
	Json->SetBoolField(TEXT("can_checkout"), bCanCheckOut);
	Json->SetBoolField(TEXT("checked_out"), bCheckedOut);
	Json->SetBoolField(TEXT("current"), bCurrent);
	Json->SetBoolField(TEXT("added"), bAdded);
	Json->SetBoolField(TEXT("deleted"), bDeleted);
	Json->SetBoolField(TEXT("ignored"), bIgnored);
	Json->SetBoolField(TEXT("can_edit"), bCanEdit);
	Json->SetBoolField(TEXT("modified"), bModified);
	Json->SetBoolField(TEXT("can_add"), bCanAdd);
	Json->SetBoolField(TEXT("conflicted"), bConflicted);
	Json->SetBoolField(TEXT("can_revert"), bCanRevert);
	Json->SetBoolField(TEXT("checked_out_by_other"), bCheckedOutOther);
	Json->SetBoolField(TEXT("checked_out_in_other_branch"), bCheckedOutInOtherBranch);
	Json->SetBoolField(TEXT("modified_in_other_branch"), bModifiedInOtherBranch);
	return Json;
}

bool FBlueprintHelperSourceControlResult::HasFileStatus(const FString& InStatus) const
{
	for (const FBlueprintHelperSourceControlFileState& File : Files)
	{
		if (File.Status == InStatus)
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperSourceControlResult::BlocksAgentEdit() const
{
	return Status == TEXT("checked_out_by_other")
		|| Status == TEXT("source_control_conflicted")
		|| Status == TEXT("checkout_required")
		|| Status == TEXT("not_editable")
		|| Status == TEXT("source_control_unavailable");
}

TSharedRef<FJsonObject> FBlueprintHelperSourceControlResult::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.SourceControlResult.v1"));
	Json->SetBoolField(TEXT("ok"), bSuccess);
	Json->SetStringField(TEXT("status"), Status);
	Json->SetBoolField(TEXT("modified"), bModified);
	Json->SetBoolField(TEXT("enabled"), bEnabled);
	Json->SetBoolField(TEXT("available"), bAvailable);
	Json->SetStringField(TEXT("provider"), Provider);
	if (!AgentMessage.IsEmpty())
	{
		Json->SetStringField(TEXT("agent_message"), AgentMessage);
	}
	if (!RecommendedAction.IsEmpty())
	{
		Json->SetStringField(TEXT("recommended_action"), RecommendedAction);
	}
	if (!ErrorCode.IsEmpty())
	{
		Json->SetStringField(TEXT("error_code"), ErrorCode);
	}
	if (!ErrorMessage.IsEmpty())
	{
		Json->SetStringField(TEXT("error_message"), ErrorMessage);
	}

	TArray<TSharedPtr<FJsonValue>> FileValues;
	for (const FBlueprintHelperSourceControlFileState& File : Files)
	{
		FileValues.Add(MakeShared<FJsonValueObject>(File.ToJson()));
	}
	Json->SetArrayField(TEXT("files"), FileValues);
	return Json;
}

FBlueprintHelperSourceControlResult FBlueprintHelperSourceControlService::QueryStatus(
	TConstArrayView<FString> Inputs,
	bool bUpdateStatus) const
{
	FBlueprintHelperSourceControlResult Result;
	Result.Provider = USourceControlHelpers::CurrentProvider();
	Result.bEnabled = USourceControlHelpers::IsEnabled();
	Result.bAvailable = USourceControlHelpers::IsAvailable();

	const TArray<FString> NormalizedInputs = FBlueprintHelperSourceControlServiceLocalUtils::NormalizeInputs(Inputs);
	if (NormalizedInputs.IsEmpty())
	{
		Result.Status = TEXT("invalid_request");
		Result.ErrorCode = TEXT("invalid_request");
		Result.ErrorMessage = TEXT("At least one asset path, package name, or file path is required.");
		Result.AgentMessage = Result.ErrorMessage;
		Result.RecommendedAction = TEXT("Pass asset_paths, package_names, or file_paths before querying source control.");
		return Result;
	}

	if (!Result.bEnabled)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("source_control_disabled");
		FBlueprintHelperSourceControlServiceLocalUtils::AddInputFileStates(Result, NormalizedInputs, TEXT("source_control_disabled"));
		FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(Result);
		return Result;
	}

	if (!Result.bAvailable)
	{
		Result.Status = TEXT("source_control_unavailable");
		Result.ErrorCode = TEXT("source_control_unavailable");
		Result.ErrorMessage = FBlueprintHelperSourceControlServiceLocalUtils::LastSourceControlError();
		FBlueprintHelperSourceControlServiceLocalUtils::AddInputFileStates(Result, NormalizedInputs, TEXT("source_control_unavailable"));
		FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(Result);
		return Result;
	}

	const TArray<FSourceControlState> SourceStates = USourceControlHelpers::QueryFileStates(NormalizedInputs, /*bSilent=*/ true);
	for (int32 Index = 0; Index < NormalizedInputs.Num(); ++Index)
	{
		if (SourceStates.IsValidIndex(Index))
		{
			Result.Files.Add(FBlueprintHelperSourceControlServiceLocalUtils::FromSourceControlState(NormalizedInputs[Index], SourceStates[Index]));
		}
		else
		{
			FBlueprintHelperSourceControlFileState State;
			State.Input = NormalizedInputs[Index];
			State.Status = TEXT("unknown");
			State.AgentHint = TEXT("Source-control state could not be read for this input.");
			Result.Files.Add(State);
		}
	}

	Result.bSuccess = true;
	FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(Result);
	return Result;
}

FBlueprintHelperSourceControlResult FBlueprintHelperSourceControlService::Checkout(
	TConstArrayView<FString> Inputs,
	bool bUpdateStatus) const
{
	FBlueprintHelperSourceControlResult Result = QueryStatus(Inputs, bUpdateStatus);
	if (!Result.bSuccess)
	{
		return Result;
	}

	if (!Result.bEnabled)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("source_control_disabled");
		Result.AgentMessage = TEXT("Source control is not enabled; checkout is not required.");
		Result.RecommendedAction = TEXT("Continue only if project policy allows editing without source control.");
		return Result;
	}

	if (Result.HasFileStatus(TEXT("checked_out_by_other")))
	{
		Result.bSuccess = false;
		Result.ErrorCode = TEXT("checked_out_by_other");
		Result.ErrorMessage = TEXT("At least one target is checked out by another user.");
		FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(Result);
		return Result;
	}

	if (Result.HasFileStatus(TEXT("source_control_conflicted")))
	{
		Result.bSuccess = false;
		Result.ErrorCode = TEXT("source_control_conflicted");
		Result.ErrorMessage = TEXT("At least one target is conflicted in source control.");
		FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(Result);
		return Result;
	}

	TArray<FString> FilesToCheckout;
	for (const FBlueprintHelperSourceControlFileState& File : Result.Files)
	{
		if (FBlueprintHelperSourceControlServiceLocalUtils::FileNeedsCheckout(File))
		{
			FilesToCheckout.AddUnique(File.Filename);
		}
	}

	if (FilesToCheckout.IsEmpty())
	{
		Result.bSuccess = true;
		Result.Status = TEXT("editable");
		Result.AgentMessage = TEXT("All targets are already editable; checkout is not required.");
		Result.RecommendedAction = TEXT("Proceed with the requested edit.");
		return Result;
	}

	const bool bCheckoutSucceeded = USourceControlHelpers::CheckOutFiles(FilesToCheckout, /*bSilent=*/ true);
	FBlueprintHelperSourceControlResult UpdatedResult = QueryStatus(Inputs, bUpdateStatus);
	UpdatedResult.bModified = bCheckoutSucceeded;

	if (!bCheckoutSucceeded || UpdatedResult.BlocksAgentEdit())
	{
		UpdatedResult.bSuccess = false;
		UpdatedResult.ErrorCode = UpdatedResult.HasFileStatus(TEXT("checked_out_by_other"))
			? TEXT("checked_out_by_other")
			: TEXT("checkout_failed");
		UpdatedResult.ErrorMessage = FBlueprintHelperSourceControlServiceLocalUtils::LastSourceControlError();
		if (UpdatedResult.ErrorMessage.IsEmpty())
		{
			UpdatedResult.ErrorMessage = TEXT("Source-control checkout failed or did not make every target editable.");
		}
		FBlueprintHelperSourceControlServiceLocalUtils::FinalizeQueryMessages(UpdatedResult);
		if (UpdatedResult.ErrorCode == TEXT("checkout_failed"))
		{
			UpdatedResult.Status = TEXT("checkout_failed");
			UpdatedResult.AgentMessage = TEXT("Checkout failed or the targets are still not editable.");
			UpdatedResult.RecommendedAction = TEXT("Stop and report checkout_failed plus source-control file states to the user.");
		}
		return UpdatedResult;
	}

	UpdatedResult.bSuccess = true;
	UpdatedResult.bModified = true;
	UpdatedResult.Status = TEXT("checked_out");
	UpdatedResult.AgentMessage = TEXT("Source-control checkout succeeded.");
	UpdatedResult.RecommendedAction = TEXT("Proceed with the requested edit.");
	return UpdatedResult;
}

FBlueprintHelperSourceControlResult FBlueprintHelperSourceControlService::QueryDirtyPackages(
	TConstArrayView<UPackage*> Packages,
	bool bUpdateStatus) const
{
	return QueryStatus(InputsFromDirtyPackages(Packages), bUpdateStatus);
}

TArray<FString> FBlueprintHelperSourceControlService::InputsFromDirtyPackages(TConstArrayView<UPackage*> Packages)
{
	TArray<FString> Inputs;
	for (UPackage* Package : Packages)
	{
		if (!IsValid(Package) || !Package->IsDirty())
		{
			continue;
		}
		const FString Filename = USourceControlHelpers::PackageFilename(Package);
		if (!Filename.IsEmpty())
		{
			Inputs.AddUnique(Filename);
		}
	}
	return Inputs;
}

void FBlueprintHelperSourceControlService::ClassifyFileStateForAgent(FBlueprintHelperSourceControlFileState& State)
{
	if (!State.bValid || State.bUnknown)
	{
		State.Status = TEXT("unknown");
		State.AgentHint = TEXT("Could not determine source-control state; stop before writing this target.");
		return;
	}
	if (State.bConflicted)
	{
		State.Status = TEXT("source_control_conflicted");
		State.AgentHint = TEXT("This target has a source-control conflict; ask the user to resolve it before editing.");
		return;
	}
	if (State.bCheckedOutOther)
	{
		State.Status = TEXT("checked_out_by_other");
		State.AgentHint = State.CheckedOutOther.IsEmpty()
			? TEXT("This target is checked out by another user; stop and report the occupied state.")
			: FString::Printf(TEXT("This target is checked out by %s; stop and report the occupied state."), *State.CheckedOutOther);
		return;
	}
	if (State.bCheckedOut || State.bAdded || State.bCanEdit)
	{
		State.Status = TEXT("editable");
		State.AgentHint = TEXT("This target is editable for the current workspace.");
		return;
	}
	if (!State.bSourceControlled || State.bIgnored || State.bCanAdd)
	{
		State.Status = TEXT("not_source_controlled");
		State.AgentHint = TEXT("This target is not currently source controlled; checkout is not required.");
		return;
	}
	if (State.bCanCheckOut)
	{
		State.Status = TEXT("checkout_required");
		State.AgentHint = TEXT("Run blueprinthelper_source_control_checkout for this target before editing.");
		return;
	}

	State.Status = TEXT("not_editable");
	State.AgentHint = TEXT("This target is not editable and cannot currently be checked out; stop before writing.");
}
