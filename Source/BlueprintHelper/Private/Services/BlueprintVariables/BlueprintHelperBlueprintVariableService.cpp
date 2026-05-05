// BlueprintHelper Service Layer 。BlueprintVariableService 实现

#include "Services/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "OperationHandlers/BlueprintVariables/BlueprintLocalVariableMutationHandler.h"
#include "OperationHandlers/BlueprintVariables/BlueprintMemberVariableMutationHandler.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Structure/BlueprintHelperServiceTypes.h"

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

namespace
{
FBlueprintHelperToolResultBase MakeBlueprintVariableFailure(
	const FString& Operation,
	const FString& TraceId,
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field = TEXT(""))
{
	FBlueprintHelperToolError Error;
	Error.Code = Code;
	Error.Stage = Stage;
	Error.Message = Message;
	Error.bRetryable = false;
	Error.Field = Field;
	return FBlueprintHelperToolResultBuilder::Failure(Operation, TraceId, Error);
}

bool TryResolveBlueprintForVariableWrite(
	const FBlueprintHelperGraphResolver& Resolver,
	const FString& AssetPath,
	UBlueprint*& OutBlueprint,
	FString& OutError)
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diagnostics;
	OutBlueprint = Resolver.ResolveBlueprint(Target, Diagnostics);
	if (!OutBlueprint)
	{
		OutError = Diagnostics.Items.Num() > 0
			? Diagnostics.Items[0].Message
			: TEXT("Blueprint asset could not be resolved.");
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> MakeBlueprintVariableTarget(
	const FString& AssetPath,
	const FString& VariableName = TEXT(""))
{
	TSharedRef<FJsonObject> TargetJson = MakeShared<FJsonObject>();
	TargetJson->SetStringField(TEXT("asset_path"), AssetPath);
	TargetJson->SetStringField(TEXT("variable_scope"), TEXT("member"));
	if (!VariableName.IsEmpty())
	{
		TargetJson->SetStringField(TEXT("variable_name"), VariableName);
	}
	return TargetJson;
}

TSharedRef<FJsonObject> MakeBlueprintLocalVariableTarget(
	const FString& AssetPath,
	const FString& FunctionName,
	const FString& VariableName = TEXT(""))
{
	TSharedRef<FJsonObject> TargetJson = MakeShared<FJsonObject>();
	TargetJson->SetStringField(TEXT("asset_path"), AssetPath);
	TargetJson->SetStringField(TEXT("variable_scope"), TEXT("local"));
	TargetJson->SetStringField(TEXT("function_name"), FunctionName);
	if (!VariableName.IsEmpty())
	{
		TargetJson->SetStringField(TEXT("variable_name"), VariableName);
	}
	return TargetJson;
}

bool TryReadDryRun(const TSharedPtr<FJsonObject>& Payload)
{
	bool bDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}
	return bDryRun;
}

bool TryReadVariableNameField(const TSharedPtr<FJsonObject>& Payload, FString& OutVariableName)
{
	if (!Payload.IsValid())
	{
		return false;
	}

	Payload->TryGetStringField(TEXT("name"), OutVariableName);
	if (OutVariableName.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("variable_name"), OutVariableName);
	}
	return !OutVariableName.IsEmpty();
}

bool TryReadVariableNamesArray(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* ArrayField,
	TArray<FString>& OutNames,
	FString& OutError,
	FString& OutField)
{
	OutNames.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(ArrayField, Values) || !Values || Values->Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s array is required."), ArrayField);
		OutField = ArrayField;
		return false;
	}

	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		FString Name;
		const TSharedPtr<FJsonValue>& Value = (*Values)[Index];
		if (Value.IsValid() && Value->Type == EJson::String)
		{
			Name = Value->AsString();
		}
		else
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			TryReadVariableNameField(Object, Name);
		}

		if (Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s[%d] requires name or variable_name."), ArrayField, Index);
			OutField = FString::Printf(TEXT("%s[%d]"), ArrayField, Index);
			return false;
		}
		OutNames.Add(Name);
	}
	return true;
}

FBlueprintHelperVariableBatchResult ToBlueprintVariableBatchResult(
	const FBlueprintHelperLocalVariableMutationCounts& Counts)
{
	FBlueprintHelperVariableBatchResult Result;
	Result.RequestedCount = Counts.RequestedCount;
	Result.AddedCount = Counts.AddedCount;
	Result.RemovedCount = Counts.RemovedCount;
	Result.ChangedCount = Counts.ChangedCount;
	Result.NoOpCount = Counts.NoOpCount;
	return Result;
}

bool TryReadLocalAddRequests(
	const TSharedPtr<FJsonObject>& Payload,
	TArray<FBlueprintHelperLocalVariableAddRequest>& OutRequests,
	FString& OutError,
	FString& OutField)
{
	OutRequests.Reset();
	if (!Payload.IsValid())
	{
		OutError = TEXT("payload is required.");
		OutField = TEXT("payload");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("variables"), VariablesArray) && VariablesArray)
	{
		if (VariablesArray->Num() == 0)
		{
			OutError = TEXT("variables array must not be empty.");
			OutField = TEXT("variables");
			return false;
		}

		for (int32 Index = 0; Index < VariablesArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> VariableObject =
				(*VariablesArray)[Index].IsValid() ? (*VariablesArray)[Index]->AsObject() : nullptr;
			FBlueprintHelperLocalVariableAddRequest Request;
			FString Field;
			if (!FBlueprintHelperLocalVariableMutationHandler::TryReadAddRequest(
				VariableObject,
				Request,
				OutError,
				&Field))
			{
				OutField = Field.IsEmpty()
					? FString::Printf(TEXT("variables[%d]"), Index)
					: FString::Printf(TEXT("variables[%d].%s"), Index, *Field);
				return false;
			}
			OutRequests.Add(MoveTemp(Request));
		}
		return true;
	}

	FBlueprintHelperLocalVariableAddRequest Request;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadAddRequest(
		Payload,
		Request,
		OutError,
		&OutField))
	{
		return false;
	}
	OutRequests.Add(MoveTemp(Request));
	return true;
}

bool TryReadLocalRemoveRequests(
	const TSharedPtr<FJsonObject>& Payload,
	TArray<FBlueprintHelperLocalVariableRemoveRequest>& OutRequests,
	FString& OutError,
	FString& OutField)
{
	OutRequests.Reset();
	if (!Payload.IsValid())
	{
		OutError = TEXT("payload is required.");
		OutField = TEXT("payload");
		return false;
	}

	bool bRootDryRun = false;
	Payload->TryGetBoolField(TEXT("dry_run"), bRootDryRun);

	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("variables"), VariablesArray) && VariablesArray)
	{
		if (VariablesArray->Num() == 0)
		{
			OutError = TEXT("variables array must not be empty.");
			OutField = TEXT("variables");
			return false;
		}

		for (int32 Index = 0; Index < VariablesArray->Num(); ++Index)
		{
			FBlueprintHelperLocalVariableRemoveRequest Request;
			Request.bDryRun = bRootDryRun;
			const TSharedPtr<FJsonValue>& Value = (*VariablesArray)[Index];
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				Request.VariableName = Value->AsString();
			}
			else
			{
				const TSharedPtr<FJsonObject> VariableObject = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Field;
				if (!FBlueprintHelperLocalVariableMutationHandler::TryReadRemoveRequest(
					VariableObject,
					Request,
					OutError,
					&Field))
				{
					OutField = Field.IsEmpty()
						? FString::Printf(TEXT("variables[%d]"), Index)
						: FString::Printf(TEXT("variables[%d].%s"), Index, *Field);
					return false;
				}
			}

			if (Request.VariableName.IsEmpty())
			{
				OutError = TEXT("remove local variable entries require name or variable_name.");
				OutField = FString::Printf(TEXT("variables[%d].name"), Index);
				return false;
			}
			OutRequests.Add(MoveTemp(Request));
		}
		return true;
	}

	FBlueprintHelperLocalVariableRemoveRequest Request;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadRemoveRequest(
		Payload,
		Request,
		OutError,
		&OutField))
	{
		return false;
	}
	OutRequests.Add(MoveTemp(Request));
	return true;
}

bool LocalVariableExists(
	const TArray<FBlueprintHelperLocalVariableItem>& LocalVariables,
	const FString& VariableName)
{
	for (const FBlueprintHelperLocalVariableItem& LocalVariable : LocalVariables)
	{
		if (LocalVariable.VariableName.Equals(VariableName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool ValidateLocalAddDryRun(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const TArray<FBlueprintHelperLocalVariableAddRequest>& Requests,
	FBlueprintHelperLocalVariableMutationCounts& OutCounts,
	FString& OutError,
	FString& OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Requests.Num();
	TArray<FBlueprintHelperLocalVariableItem> ExistingVariables;
	if (!FBlueprintHelperLocalVariableMutationHandler::ReadLocalVariables(
		Blueprint,
		FunctionName,
		ExistingVariables,
		OutError,
		&OutField))
	{
		return false;
	}

	TSet<FString> PlannedNames;
	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariableAddRequest& Request = Requests[Index];
		if (Request.VariableName.IsEmpty())
		{
			OutError = TEXT("local variable add request requires name or variable_name.");
			OutField = FString::Printf(TEXT("variables[%d].name"), Index);
			return false;
		}

		FEdGraphPinType PinType;
		if (!FBlueprintHelperLocalVariableMutationHandler::TryBuildPinType(Request.VariableType, PinType, OutError))
		{
			OutField = FString::Printf(TEXT("variables[%d].variable_type"), Index);
			return false;
		}

		const FString LowerName = Request.VariableName.ToLower();
		const bool bExists = LocalVariableExists(ExistingVariables, Request.VariableName);
		const bool bPlanned = PlannedNames.Contains(LowerName);
		if ((bExists || bPlanned) &&
			Request.NameCollisionPolicy == EBlueprintHelperVariableNameCollisionPolicy::FailIfExists)
		{
			OutError = FString::Printf(TEXT("Local variable '%s' already exists."), *Request.VariableName);
			OutField = FString::Printf(TEXT("variables[%d].name"), Index);
			return false;
		}

		if (bExists || bPlanned)
		{
			++OutCounts.NoOpCount;
		}
		else
		{
			PlannedNames.Add(LowerName);
			++OutCounts.AddedCount;
		}
	}

	return true;
}

void AddVariableWriteValidation(FBlueprintHelperToolResultBase& Result, bool bShouldCompile = true, bool bShouldSave = true)
{
	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bShouldCompile;
	Validation.bShouldSave = bShouldSave;
	Result.Validation = Validation;
}
}

FBlueprintHelperBlueprintVariableService::FBlueprintHelperBlueprintVariableService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperBlueprintStructureService& InStructureService)
	: Resolver(InResolver), StructureService(InStructureService)
{
}

FBlueprintHelperMemberVariableItem FBlueprintHelperBlueprintVariableService::ConvertToMemberItem(
	const FBlueprintHelperVariableInfo& Info) const
{
	FBlueprintHelperMemberVariableItem Item;
	Item.VariableName = Info.Name;
	Item.VariableType.Category = Info.TypeCategory;
	if (!Info.SubCategoryObject.IsEmpty())
		Item.VariableType.Subtype = Info.SubCategoryObject;
	Item.VariableType.Container = Info.ContainerType.IsEmpty() ? TEXT("single") : Info.ContainerType;
	if (!Info.Category.IsEmpty()) Item.Category = Info.Category;
	return Item;
}

// ─── ReadMemberVariables ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::ReadMemberVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FString AssetPath;
	if (Payload.IsValid()) Payload->TryGetStringField(TEXT("asset_path"), AssetPath);

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = AssetPath;
	const FBlueprintHelperListVariablesResult ListResult = StructureService.ListVariables(Tgt);

	if (!ListResult.bSuccess)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_member_variables"), TraceId,
			{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ListResult.ErrorMessage, false});

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_member_variables"), TraceId);

	TSharedRef<FJsonObject> TgtJson = MakeShared<FJsonObject>();
	TgtJson->SetStringField(TEXT("asset_path"), AssetPath.IsEmpty() ? TEXT("focused") : AssetPath);
	TgtJson->SetStringField(TEXT("read_scope"), TEXT("member_variables"));
	Result.CustomTargetJson = TgtJson;

	FBlueprintHelperReadMemberVariablesResultData Data;
	for (const auto& Info : ListResult.Variables)
		Data.MemberVariables.Add(ConvertToMemberItem(Info));
	Result.Data = Data.ToJson();
	return Result;
}

// ─── AddMemberVariable ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddMemberVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variable"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload 缺失。"), false});

	FString AssetPath; Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variable"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 asset_path。"), false});

	if (TryReadDryRun(Payload))
	{
		UBlueprint* Blueprint = nullptr;
		FString ResolveError;
		if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
		{
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variable"), TraceId,
				{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ResolveError, false});
		}
		(void)Blueprint;

		FString VariableName;
		if (!TryReadVariableNameField(Payload, VariableName))
		{
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variable"), TraceId,
				{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("name or variable_name is required."), false});
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("add_blueprint_member_variable"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, VariableName);
		FBlueprintHelperAddMemberVariableResultData Data;
		Data.AddResult.bSuccess = true;
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = AssetPath;
	FString Error;
	if (!StructureService.AddVariable(Tgt, Payload, Error))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variable"), TraceId,
			{TEXT("variable_add_failed"), EBlueprintHelperToolStage::Execute, Error, false});

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("add_blueprint_member_variable"), TraceId);
	TSharedRef<FJsonObject> TgtJson = MakeShared<FJsonObject>();
	TgtJson->SetStringField(TEXT("asset_path"), AssetPath);
	TgtJson->SetStringField(TEXT("variable_scope"), TEXT("member"));
	Result.CustomTargetJson = TgtJson;

	FBlueprintHelperAddMemberVariableResultData Data;
	Data.AddResult.bSuccess = true;
	Result.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val; Val.bShouldCompile = true; Val.bShouldSave = true;
	Result.Validation = Val;
	return Result;
}

// ─── AddMemberVariables (batch) ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddMemberVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variables"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload 缺失。"), false});

	FString AssetPath; Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
	if (!Payload->TryGetArrayField(TEXT("variables"), Vars) || Vars->Num() == 0)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variables"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 variables 数组。"), false});

	if (TryReadDryRun(Payload))
	{
		UBlueprint* Blueprint = nullptr;
		FString ResolveError;
		if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
		{
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variables"), TraceId,
				{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ResolveError, false});
		}
		(void)Blueprint;

		for (int32 Index = 0; Index < Vars->Num(); ++Index)
		{
			FString VariableName;
			if (!TryReadVariableNameField((*Vars)[Index].IsValid() ? (*Vars)[Index]->AsObject() : nullptr, VariableName))
			{
				return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_member_variables"), TraceId,
					{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, FString::Printf(TEXT("variables[%d] requires name or variable_name."), Index), false});
			}
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("add_blueprint_member_variables"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath);
		FBlueprintHelperAddMemberVariablesResultData Data;
		Data.AddResult.RequestedCount = Vars->Num();
		Data.AddResult.AddedCount = Vars->Num();
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = AssetPath;
	FBlueprintHelperVariableBatchResult BatchResult;
	BatchResult.RequestedCount = Vars->Num();

	for (const auto& V : *Vars)
	{
		const TSharedPtr<FJsonObject> VarObj = V->AsObject();
		if (!VarObj.IsValid()) continue;
		FString Err;
		if (StructureService.AddVariable(Tgt, VarObj, Err))
			BatchResult.AddedCount++;
		else
			BatchResult.NoOpCount++;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("add_blueprint_member_variables"), TraceId);
	TSharedRef<FJsonObject> TgtJson = MakeShared<FJsonObject>();
	TgtJson->SetStringField(TEXT("asset_path"), AssetPath);
	Result.CustomTargetJson = TgtJson;

	FBlueprintHelperAddMemberVariablesResultData Data;
	Data.AddResult = BatchResult;
	Result.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val; Val.bShouldCompile = true; Val.bShouldSave = true;
	Result.Validation = Val;
	return Result;
}

// ─── SetMemberVariableProperties ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetMemberVariableProperties(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString VariableName;
	if (!FBlueprintHelperMemberVariableMutationHandler::TryReadVariableName(Payload, VariableName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("name or variable_name is required."),
			TEXT("name"));
	}

	const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
	if (!Payload->TryGetArrayField(TEXT("settings"), SettingsArray) || !SettingsArray || SettingsArray->Num() == 0)
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			TEXT("invalid_member_variable_settings"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("settings array is required."),
			TEXT("settings"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	TArray<FBlueprintHelperMemberPropertyMutation> Settings;
	for (int32 Index = 0; Index < SettingsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> SettingObject =
			(*SettingsArray)[Index].IsValid()
				? (*SettingsArray)[Index]->AsObject()
				: nullptr;

		FBlueprintHelperMemberPropertyMutation Setting;
		if (!FBlueprintHelperMemberVariableMutationHandler::TryReadPropertySetting(SettingObject, Setting))
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_member_variable_properties"),
				TraceId,
				TEXT("invalid_member_variable_settings"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("settings entries require property_path and value."),
				FString::Printf(TEXT("settings[%d]"), Index));
		}
		Settings.Add(MoveTemp(Setting));
	}

	if (TryReadDryRun(Payload))
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VariableName));
		if (VariableIndex == INDEX_NONE)
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_member_variable_properties"),
				TraceId,
				TEXT("variable_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Member variable '%s' was not found."), *VariableName),
				TEXT("name"));
		}

		for (int32 Index = 0; Index < Settings.Num(); ++Index)
		{
			const FString Path = Settings[Index].PropertyPath.ToLower();
			if (Path != TEXT("category") &&
				Path != TEXT("tooltip") &&
				Path != TEXT("instance_editable") &&
				Path != TEXT("expose_on_spawn"))
			{
				return MakeBlueprintVariableFailure(
					TEXT("set_blueprint_member_variable_properties"),
					TraceId,
					TEXT("invalid_member_variable_settings"),
					EBlueprintHelperToolStage::ParseInput,
					FString::Printf(TEXT("Unsupported member variable property: %s."), *Settings[Index].PropertyPath),
					FString::Printf(TEXT("settings[%d].property_path"), Index));
			}
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("set_blueprint_member_variable_properties"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, VariableName);
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("SetMemberVariableProperties.v1"));
		FBlueprintHelperVariableBatchResult PropertiesResult;
		PropertiesResult.RequestedCount = Settings.Num();
		PropertiesResult.ChangedCount = Settings.Num();
		Data->SetObjectField(TEXT("properties_result"), PropertiesResult.ToJson());
		Result.Data = Data;
		return Result;
	}

	FBlueprintHelperVariableMutationCounts Counts;
	FString SettingError;
	FString SettingField;
	if (!FBlueprintHelperMemberVariableMutationHandler::ApplyPropertySettings(
		Blueprint,
		VariableName,
		Settings,
		Counts,
		SettingError,
		&SettingField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_variable_properties"),
			TraceId,
			SettingField == TEXT("name") ? TEXT("variable_not_found") : TEXT("invalid_member_variable_settings"),
			SettingField == TEXT("name") ? EBlueprintHelperToolStage::ResolveTarget : EBlueprintHelperToolStage::ParseInput,
			SettingError,
			SettingField);
	}

	FBlueprintHelperToolResultBase Result = Counts.ChangedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("set_blueprint_member_variable_properties"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("set_blueprint_member_variable_properties"), TraceId);
	Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, VariableName);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("SetMemberVariableProperties.v1"));
	FBlueprintHelperVariableBatchResult PropertiesResult;
	PropertiesResult.RequestedCount = Counts.RequestedCount;
	PropertiesResult.ChangedCount = Counts.ChangedCount;
	PropertiesResult.NoOpCount = Counts.NoOpCount;
	Data->SetObjectField(TEXT("properties_result"), PropertiesResult.ToJson());
	Result.Data = Data;

	if (Counts.ChangedCount > 0)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = true;
		Validation.bShouldSave = true;
		Result.Validation = Validation;
	}
	return Result;
}

// ─── RemoveMemberVariable ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveMemberVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variable"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload 缺失。"), false});

	FString AssetPath; Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	FString VarName; Payload->TryGetStringField(TEXT("name"), VarName);
	if (VarName.IsEmpty()) Payload->TryGetStringField(TEXT("variable_name"), VarName);
	if (AssetPath.IsEmpty() || VarName.IsEmpty())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variable"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 asset_path 或 variable_name。"), false});

	if (TryReadDryRun(Payload))
	{
		UBlueprint* Blueprint = nullptr;
		FString ResolveError;
		if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
		{
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variable"), TraceId,
				{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ResolveError, false});
		}

		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VarName));
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_blueprint_member_variable"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, VarName);
		FBlueprintHelperRemoveMemberVariableResultData Data;
		Data.RemoveResult.bSuccess = VariableIndex != INDEX_NONE;
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = AssetPath;
	FString Err;
	if (!StructureService.RemoveVariable(Tgt, VarName, Err))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variable"), TraceId,
			{TEXT("variable_remove_failed"), EBlueprintHelperToolStage::Execute, Err, false});

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("remove_blueprint_member_variable"), TraceId);
	TSharedRef<FJsonObject> TgtJson = MakeShared<FJsonObject>();
	TgtJson->SetStringField(TEXT("asset_path"), AssetPath);
	Result.CustomTargetJson = TgtJson;

	FBlueprintHelperRemoveMemberVariableResultData Data;
	Data.RemoveResult.bSuccess = true;
	Result.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val; Val.bShouldCompile = true; Val.bShouldSave = true;
	Result.Validation = Val;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveMemberVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload is required."), false});
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("asset_path is required."), false});
	}

	TArray<FString> VariableNames;
	FString ParseError;
	FString ParseField;
	if (!TryReadVariableNamesArray(Payload, TEXT("variables"), VariableNames, ParseError, ParseField) &&
		!TryReadVariableNamesArray(Payload, TEXT("variable_names"), VariableNames, ParseError, ParseField))
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, ParseError, false});
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
			{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ResolveError, false});
	}

	FBlueprintHelperVariableBatchResult BatchResult;
	BatchResult.RequestedCount = VariableNames.Num();
	for (const FString& VariableName : VariableNames)
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VariableName));
		if (VariableIndex == INDEX_NONE)
		{
			++BatchResult.NoOpCount;
			continue;
		}
		++BatchResult.RemovedCount;
	}

	if (TryReadDryRun(Payload))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_blueprint_member_variables"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath);
		FBlueprintHelperRemoveMemberVariablesResultData Data;
		Data.RemoveResult = BatchResult;
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;
	for (const FString& VariableName : VariableNames)
	{
		if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VariableName)) == INDEX_NONE)
		{
			continue;
		}

		FString RemoveError;
		if (!StructureService.RemoveVariable(Target, VariableName, RemoveError))
		{
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
				{TEXT("variable_remove_failed"), EBlueprintHelperToolStage::Execute, RemoveError, false});
		}
	}

	FBlueprintHelperToolResultBase Result = BatchResult.RemovedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("remove_blueprint_member_variables"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("remove_blueprint_member_variables"), TraceId);
	Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath);
	FBlueprintHelperRemoveMemberVariablesResultData Data;
	Data.RemoveResult = BatchResult;
	Result.Data = Data.ToJson();
	if (BatchResult.RemovedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
#if 0
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量删除暂未实现。"), false});
}

// ─── Member Defaults ───

#endif
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::ReadMemberDefaults(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_member_defaults"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("payload 缺失。"), false});

	FString AssetPath; Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_member_defaults"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 asset_path。"), false});

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = AssetPath;
	const FBlueprintHelperListVariablesResult ListResult = StructureService.ListVariables(Tgt);
	if (!ListResult.bSuccess)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_member_defaults"), TraceId,
			{TEXT("asset_not_found"), EBlueprintHelperToolStage::ResolveTarget, ListResult.ErrorMessage, false});

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_member_defaults"), TraceId);
	TSharedRef<FJsonObject> TgtJson = MakeShared<FJsonObject>();
	TgtJson->SetStringField(TEXT("asset_path"), AssetPath);
	TgtJson->SetStringField(TEXT("read_scope"), TEXT("member_defaults"));
	Result.CustomTargetJson = TgtJson;

	FBlueprintHelperReadMemberDefaultsResultData Data;
	TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
	for (const auto& Info : ListResult.Variables)
		if (!Info.DefaultValue.IsEmpty())
			Values->SetStringField(Info.Name, Info.DefaultValue);
	Data.Values = Values;
	Result.Data = Data.ToJson();
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetMemberDefault(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FBlueprintHelperMemberDefaultMutation Change;
	if (!FBlueprintHelperMemberVariableMutationHandler::TryReadVariableName(Payload, Change.VariableName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("name or variable_name is required."),
			TEXT("name"));
	}

	if (!FBlueprintHelperMemberVariableMutationHandler::TryReadDefaultValue(Payload, Change.DefaultValue))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("invalid_member_default_settings"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("value or default_value must be a scalar JSON value."),
			TEXT("value"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	TArray<FBlueprintHelperMemberDefaultMutation> Changes;
	Changes.Add(Change);

	if (TryReadDryRun(Payload))
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Change.VariableName));
		if (VariableIndex == INDEX_NONE)
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_member_default"),
				TraceId,
				TEXT("variable_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Member variable '%s' was not found."), *Change.VariableName),
				TEXT("name"));
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("set_blueprint_member_default"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, Change.VariableName);
		FBlueprintHelperSetMemberDefaultsResultData Data;
		Data.AppliedCount = 1;
		Data.ChangedCount = 1;
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (!FBlueprintHelperMemberVariableMutationHandler::ApplyDefaultChanges(
		Blueprint,
		Changes,
		Counts,
		ApplyError,
		&ApplyField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_default"),
			TraceId,
			TEXT("variable_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ApplyError,
			ApplyField);
	}

	FBlueprintHelperToolResultBase Result = Counts.ChangedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("set_blueprint_member_default"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("set_blueprint_member_default"), TraceId);
	Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath, Change.VariableName);

	FBlueprintHelperSetMemberDefaultsResultData Data;
	Data.AppliedCount = Counts.ChangedCount > 0 ? 1 : 0;
	Data.ChangedCount = Counts.ChangedCount;
	Data.NoOpCount = Counts.NoOpCount;
	Result.Data = Data.ToJson();

	if (Counts.ChangedCount > 0)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = true;
		Validation.bShouldSave = true;
		Result.Validation = Validation;
	}
	return Result;
}
FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetMemberDefaults(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_defaults"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_defaults"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	TArray<FBlueprintHelperMemberDefaultMutation> Changes;
	const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("defaults"), DefaultsArray) && DefaultsArray)
	{
		for (int32 Index = 0; Index < DefaultsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> DefaultObject =
				(*DefaultsArray)[Index].IsValid()
					? (*DefaultsArray)[Index]->AsObject()
					: nullptr;
			if (!DefaultObject.IsValid())
			{
				return MakeBlueprintVariableFailure(
					TEXT("set_blueprint_member_defaults"),
					TraceId,
					TEXT("invalid_member_default_settings"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("defaults entries must be objects."),
					FString::Printf(TEXT("defaults[%d]"), Index));
			}

			FBlueprintHelperMemberDefaultMutation Change;
			if (!FBlueprintHelperMemberVariableMutationHandler::TryReadVariableName(DefaultObject, Change.VariableName))
			{
				return MakeBlueprintVariableFailure(
					TEXT("set_blueprint_member_defaults"),
					TraceId,
					TEXT("invalid_request"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("defaults entry requires name or variable_name."),
					FString::Printf(TEXT("defaults[%d].name"), Index));
			}

			if (!FBlueprintHelperMemberVariableMutationHandler::TryReadDefaultValue(DefaultObject, Change.DefaultValue))
			{
				return MakeBlueprintVariableFailure(
					TEXT("set_blueprint_member_defaults"),
					TraceId,
					TEXT("invalid_member_default_settings"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("defaults entry requires a scalar value or default_value."),
					FString::Printf(TEXT("defaults[%d].value"), Index));
			}
			Changes.Add(MoveTemp(Change));
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* ValuesObject = nullptr;
		if (Payload->TryGetObjectField(TEXT("values"), ValuesObject) && ValuesObject && ValuesObject->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ValuesObject)->Values)
			{
				FBlueprintHelperMemberDefaultMutation Change;
				Change.VariableName = Pair.Key;
				if (!FBlueprintHelperMemberVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(
					Pair.Value,
					Change.DefaultValue))
				{
					return MakeBlueprintVariableFailure(
						TEXT("set_blueprint_member_defaults"),
						TraceId,
						TEXT("invalid_member_default_settings"),
						EBlueprintHelperToolStage::ParseInput,
						TEXT("values entries must be scalar JSON values."),
						FString::Printf(TEXT("values.%s"), *Pair.Key));
				}
				Changes.Add(MoveTemp(Change));
			}
		}
	}

	if (Changes.Num() == 0)
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_defaults"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("defaults array or values object is required."),
			TEXT("defaults"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_defaults"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	if (TryReadDryRun(Payload))
	{
		for (int32 Index = 0; Index < Changes.Num(); ++Index)
		{
			if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Changes[Index].VariableName)) == INDEX_NONE)
			{
				return MakeBlueprintVariableFailure(
					TEXT("set_blueprint_member_defaults"),
					TraceId,
					TEXT("variable_not_found"),
					EBlueprintHelperToolStage::ResolveTarget,
					FString::Printf(TEXT("Member variable '%s' was not found."), *Changes[Index].VariableName),
					FString::Printf(TEXT("defaults[%d].name"), Index));
			}
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("set_blueprint_member_defaults"), TraceId);
		Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath);
		FBlueprintHelperSetMemberDefaultsBatchResultData Data;
		Data.DefaultsResult.RequestedCount = Changes.Num();
		Data.DefaultsResult.ChangedCount = Changes.Num();
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (!FBlueprintHelperMemberVariableMutationHandler::ApplyDefaultChanges(
		Blueprint,
		Changes,
		Counts,
		ApplyError,
		&ApplyField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_member_defaults"),
			TraceId,
			TEXT("variable_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ApplyError,
			ApplyField);
	}

	FBlueprintHelperToolResultBase Result = Counts.ChangedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("set_blueprint_member_defaults"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("set_blueprint_member_defaults"), TraceId);
	Result.CustomTargetJson = MakeBlueprintVariableTarget(AssetPath);

	FBlueprintHelperSetMemberDefaultsBatchResultData Data;
	Data.DefaultsResult.RequestedCount = Counts.RequestedCount;
	Data.DefaultsResult.ChangedCount = Counts.ChangedCount;
	Data.DefaultsResult.NoOpCount = Counts.NoOpCount;
	Result.Data = Data.ToJson();

	if (Counts.ChangedCount > 0)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = true;
		Validation.bShouldSave = true;
		Result.Validation = Validation;
	}
	return Result;
}
// ─── Local Variables ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::ReadLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("read_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("read_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("read_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("read_blueprint_local_variables"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	TArray<FBlueprintHelperLocalVariableItem> LocalVariables;
	FString ReadError;
	FString ReadField;
	if (!FBlueprintHelperLocalVariableMutationHandler::ReadLocalVariables(
		Blueprint,
		FunctionName,
		LocalVariables,
		ReadError,
		&ReadField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("read_blueprint_local_variables"),
			TraceId,
			TEXT("function_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ReadError,
			ReadField);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_local_variables"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName);
	FBlueprintHelperReadLocalVariablesResultData Data;
	Data.FunctionName = FunctionName;
	Data.LocalVariables = MoveTemp(LocalVariables);
	Result.Data = Data.ToJson();
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 读取暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddLocalVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	TArray<FBlueprintHelperLocalVariableAddRequest> Requests;
	FString ParseError;
	FString ParseField;
	if (!TryReadLocalAddRequests(Payload, Requests, ParseError, ParseField) || Requests.Num() != 1)
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_local_variable_settings"),
			EBlueprintHelperToolStage::ParseInput,
			ParseError.IsEmpty() ? TEXT("single local variable add request is required.") : ParseError,
			ParseField.IsEmpty() ? TEXT("name") : ParseField);
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variable"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	FBlueprintHelperLocalVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (TryReadDryRun(Payload))
	{
		if (!ValidateLocalAddDryRun(Blueprint, FunctionName, Requests, Counts, ApplyError, ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("add_blueprint_local_variable"),
				TraceId,
				TEXT("variable_add_failed"),
				EBlueprintHelperToolStage::DryRun,
				ApplyError,
				ApplyField);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("add_blueprint_local_variable"), TraceId);
		Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, Requests[0].VariableName);
		FBlueprintHelperAddLocalVariableResultData Data;
		Data.AddResult.bSuccess = Counts.AddedCount > 0 || Counts.NoOpCount > 0;
		Result.Data = Data.ToJson();
		return Result;
	}

	{
		FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Local Variable")));
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyAddLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("add_blueprint_local_variable"),
				TraceId,
				TEXT("variable_add_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError,
				ApplyField);
		}
	}

	FBlueprintHelperToolResultBase Result = Counts.AddedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("add_blueprint_local_variable"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("add_blueprint_local_variable"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, Requests[0].VariableName);
	FBlueprintHelperAddLocalVariableResultData Data;
	Data.AddResult.bSuccess = true;
	Result.Data = Data.ToJson();
	if (Counts.AddedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_local_variable"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 创建暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	TArray<FBlueprintHelperLocalVariableAddRequest> Requests;
	FString ParseError;
	FString ParseField;
	if (!TryReadLocalAddRequests(Payload, Requests, ParseError, ParseField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_local_variable_settings"),
			EBlueprintHelperToolStage::ParseInput,
			ParseError,
			ParseField);
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("add_blueprint_local_variables"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	FBlueprintHelperLocalVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (TryReadDryRun(Payload))
	{
		if (!ValidateLocalAddDryRun(Blueprint, FunctionName, Requests, Counts, ApplyError, ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("add_blueprint_local_variables"),
				TraceId,
				TEXT("variable_add_failed"),
				EBlueprintHelperToolStage::DryRun,
				ApplyError,
				ApplyField);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("add_blueprint_local_variables"), TraceId);
		Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName);
		FBlueprintHelperAddLocalVariablesResultData Data;
		Data.AddResult = ToBlueprintVariableBatchResult(Counts);
		Result.Data = Data.ToJson();
		return Result;
	}

	{
		FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Local Variables")));
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyAddLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("add_blueprint_local_variables"),
				TraceId,
				TEXT("variable_add_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError,
				ApplyField);
		}
	}

	FBlueprintHelperToolResultBase Result = Counts.AddedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("add_blueprint_local_variables"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("add_blueprint_local_variables"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName);
	FBlueprintHelperAddLocalVariablesResultData Data;
	Data.AddResult = ToBlueprintVariableBatchResult(Counts);
	Result.Data = Data.ToJson();
	if (Counts.AddedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量 Local Variable 创建暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetLocalVariableProperties(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	FString VariableName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadVariableName(Payload, VariableName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("name or variable_name is required."),
			TEXT("name"));
	}

	TArray<FBlueprintHelperLocalVariablePropertyMutation> Settings;
	FString ParseError;
	FString ParseField;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadPropertySettings(
		Payload,
		Settings,
		ParseError,
		&ParseField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("invalid_local_variable_settings"),
			EBlueprintHelperToolStage::ParseInput,
			ParseError,
			ParseField);
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("set_blueprint_local_variable_properties"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	if (TryReadDryRun(Payload))
	{
		TArray<FBlueprintHelperLocalVariableItem> LocalVariables;
		FString ReadError;
		FString ReadField;
		if (!FBlueprintHelperLocalVariableMutationHandler::ReadLocalVariables(
			Blueprint,
			FunctionName,
			LocalVariables,
			ReadError,
			&ReadField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_local_variable_properties"),
				TraceId,
				TEXT("function_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				ReadError,
				ReadField);
		}

		if (!LocalVariableExists(LocalVariables, VariableName))
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_local_variable_properties"),
				TraceId,
				TEXT("variable_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Local variable '%s' was not found."), *VariableName),
				TEXT("name"));
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("set_blueprint_local_variable_properties"), TraceId);
		Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, VariableName);
		FBlueprintHelperSetLocalVariablePropertiesResultData Data;
		Data.PropertiesResult.RequestedCount = Settings.Num();
		Data.PropertiesResult.ChangedCount = Settings.Num();
		Result.Data = Data.ToJson();
		return Result;
	}

	FBlueprintHelperLocalVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Set Local Variable Properties")));
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyPropertySettings(
			Blueprint,
			FunctionName,
			VariableName,
			Settings,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("set_blueprint_local_variable_properties"),
				TraceId,
				TEXT("variable_property_set_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError,
				ApplyField);
		}
	}

	FBlueprintHelperToolResultBase Result = Counts.ChangedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("set_blueprint_local_variable_properties"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("set_blueprint_local_variable_properties"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, VariableName);
	FBlueprintHelperSetLocalVariablePropertiesResultData Data;
	Data.PropertiesResult = ToBlueprintVariableBatchResult(Counts);
	Result.Data = Data.ToJson();
	if (Counts.ChangedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("set_blueprint_local_variable_properties"), TraceId,
		{TEXT("unsupported_variable_type"), EBlueprintHelperToolStage::Execute, TEXT("Local Variable 属性修改暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveLocalVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	TArray<FBlueprintHelperLocalVariableRemoveRequest> Requests;
	FString ParseError;
	FString ParseField;
	if (!TryReadLocalRemoveRequests(Payload, Requests, ParseError, ParseField) || Requests.Num() != 1)
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variable"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			ParseError.IsEmpty() ? TEXT("single local variable remove request is required.") : ParseError,
			ParseField.IsEmpty() ? TEXT("name") : ParseField);
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variable"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	FBlueprintHelperLocalVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (TryReadDryRun(Payload))
	{
		Requests[0].bDryRun = true;
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyRemoveLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("remove_blueprint_local_variable"),
				TraceId,
				TEXT("variable_remove_failed"),
				EBlueprintHelperToolStage::DryRun,
				ApplyError,
				ApplyField);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_blueprint_local_variable"), TraceId);
		Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, Requests[0].VariableName);
		FBlueprintHelperRemoveLocalVariableResultData Data;
		Data.RemoveResult.bSuccess = true;
		Result.Data = Data.ToJson();
		return Result;
	}

	Requests[0].bDryRun = false;
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Local Variable")));
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyRemoveLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("remove_blueprint_local_variable"),
				TraceId,
				TEXT("variable_remove_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError,
				ApplyField);
		}
	}

	FBlueprintHelperToolResultBase Result = Counts.RemovedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("remove_blueprint_local_variable"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("remove_blueprint_local_variable"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName, Requests[0].VariableName);
	FBlueprintHelperRemoveLocalVariableResultData Data;
	Data.RemoveResult.bSuccess = Counts.RemovedCount > 0;
	Result.Data = Data.ToJson();
	if (Counts.RemovedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_local_variable"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 删除暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	if (!Payload.IsValid())
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("asset_path is required."),
			TEXT("asset_path"));
	}

	FString FunctionName;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(Payload, FunctionName))
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("function_name is required."),
			TEXT("function_name"));
	}

	TArray<FBlueprintHelperLocalVariableRemoveRequest> Requests;
	FString ParseError;
	FString ParseField;
	if (!TryReadLocalRemoveRequests(Payload, Requests, ParseError, ParseField))
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variables"),
			TraceId,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			ParseError,
			ParseField);
	}

	UBlueprint* Blueprint = nullptr;
	FString ResolveError;
	if (!TryResolveBlueprintForVariableWrite(Resolver, AssetPath, Blueprint, ResolveError))
	{
		return MakeBlueprintVariableFailure(
			TEXT("remove_blueprint_local_variables"),
			TraceId,
			TEXT("asset_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ResolveError,
			TEXT("asset_path"));
	}

	const bool bDryRun = TryReadDryRun(Payload);
	for (FBlueprintHelperLocalVariableRemoveRequest& Request : Requests)
	{
		Request.bDryRun = bDryRun;
	}

	FBlueprintHelperLocalVariableMutationCounts Counts;
	FString ApplyError;
	FString ApplyField;
	if (bDryRun)
	{
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyRemoveLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("remove_blueprint_local_variables"),
				TraceId,
				TEXT("variable_remove_failed"),
				EBlueprintHelperToolStage::DryRun,
				ApplyError,
				ApplyField);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_blueprint_local_variables"), TraceId);
		Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName);
		FBlueprintHelperRemoveLocalVariablesResultData Data;
		Data.RemoveResult = ToBlueprintVariableBatchResult(Counts);
		Result.Data = Data.ToJson();
		return Result;
	}

	{
		FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Local Variables")));
		if (!FBlueprintHelperLocalVariableMutationHandler::ApplyRemoveLocalVariables(
			Blueprint,
			FunctionName,
			Requests,
			Counts,
			ApplyError,
			&ApplyField))
		{
			return MakeBlueprintVariableFailure(
				TEXT("remove_blueprint_local_variables"),
				TraceId,
				TEXT("variable_remove_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError,
				ApplyField);
		}
	}

	FBlueprintHelperToolResultBase Result = Counts.RemovedCount > 0
		? FBlueprintHelperToolResultBuilder::Applied(TEXT("remove_blueprint_local_variables"), TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(TEXT("remove_blueprint_local_variables"), TraceId);
	Result.CustomTargetJson = MakeBlueprintLocalVariableTarget(AssetPath, FunctionName);
	FBlueprintHelperRemoveLocalVariablesResultData Data;
	Data.RemoveResult = ToBlueprintVariableBatchResult(Counts);
	Result.Data = Data.ToJson();
	if (Counts.RemovedCount > 0)
	{
		AddVariableWriteValidation(Result);
	}
	return Result;
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量 Local Variable 删除暂未实现。"), false});
}
