// BlueprintHelper Service Layer — BlueprintVariableService 实现

#include "Services/BlueprintHelperBlueprintVariableService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Structure/BlueprintHelperServiceTypes.h"

#include "Dom/JsonObject.h"

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
	// 第一版委托给 StructureService 的能力
	(void)Payload;
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("set_blueprint_member_variable_properties"), TraceId,
		{TEXT("unsupported_variable_type"), EBlueprintHelperToolStage::Execute, TEXT("第一版不支持修改变量属性。"), false});
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
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_member_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量删除暂未实现。"), false});
}

// ─── Member Defaults ───

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
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("set_blueprint_member_default"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("CDO 默认值写入暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetMemberDefaults(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("set_blueprint_member_defaults"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("CDO 默认值批量写入暂未实现。"), false});
}

// ─── Local Variables ───

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::ReadLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("read_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 读取暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddLocalVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_local_variable"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 创建暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::AddLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("add_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量 Local Variable 创建暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::SetLocalVariableProperties(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("set_blueprint_local_variable_properties"), TraceId,
		{TEXT("unsupported_variable_type"), EBlueprintHelperToolStage::Execute, TEXT("Local Variable 属性修改暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveLocalVariable(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_local_variable"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("Local Variable 删除暂未实现。"), false});
}

FBlueprintHelperToolResultBase FBlueprintHelperBlueprintVariableService::RemoveLocalVariables(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	(void)Payload;
	return FBlueprintHelperToolResultBuilder::Failure(TEXT("remove_blueprint_local_variables"), TraceId,
		{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("批量 Local Variable 删除暂未实现。"), false});
}
