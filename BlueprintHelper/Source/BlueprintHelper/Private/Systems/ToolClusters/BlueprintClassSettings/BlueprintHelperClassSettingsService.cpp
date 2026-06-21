// BlueprintHelper Service Layer — Blueprint Class Settings 服务实现

#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/BlueprintClassSettings/Utils/BlueprintHelperClassSettingsUtils.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationPolicy.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationResolver.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultSetterMutationService.h"
#include "Shared/BlueprintHelperServiceTypes.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/Interface.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperClassSettingsServiceLocal
{
public:
	static TArray<FString> BuildDirectWriteBlockedBy(const FProperty* Property)
	{
		TArray<FString> BlockedBy;
		if (!Property)
		{
			BlockedBy.Add(TEXT("MissingProperty"));
			return BlockedBy;
		}
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			BlockedBy.Add(TEXT("NotEdit"));
		}
		if (Property->HasAnyPropertyFlags(CPF_EditConst))
		{
			BlockedBy.Add(TEXT("EditConst"));
		}
		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			BlockedBy.Add(TEXT("Transient"));
		}
		if (Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate))
		{
			BlockedBy.Add(TEXT("DisableEditOnTemplate"));
		}
		return BlockedBy;
	}

	static bool ResolveMutationDecision(
		UObject* CDO,
		const FBlueprintHelperClassDefaultPropertySetting& Setting,
		FBlueprintHelperClassDefaultResolvedMutationTarget& OutTarget,
		FBlueprintHelperClassDefaultMutationPolicyDecision& OutDecision,
		FBlueprintHelperInvalidClassDefaultSetting& OutInvalid)
	{
		OutInvalid.PropertyPath = Setting.PropertyPath;

		FString ResolveCode;
		FString ResolveMessage;
		const FBlueprintHelperClassDefaultPropertyMutationResolver Resolver;
		if (!Resolver.Resolve(CDO, Setting.PropertyPath, OutTarget, ResolveCode, ResolveMessage))
		{
			OutInvalid.Code = ResolveCode;
			OutInvalid.ValueSummary = ResolveMessage.Left(128);
			return false;
		}

		OutInvalid.ExpectedType = OutTarget.ExpectedType;
		const FBlueprintHelperClassDefaultPropertyMutationPolicy Policy;
		OutDecision = Policy.Decide(OutTarget, Setting.MutationStrategy);
		if (OutDecision.Strategy == EBlueprintHelperClassDefaultMutationStrategy::Blocked)
		{
			OutInvalid.Code = OutDecision.Code;
			OutInvalid.ValueSummary = OutDecision.Message.Left(128);
			OutInvalid.SafeNextAction = OutDecision.SafeNextAction;
			if (!OutDecision.SuggestedRoute.RouteId.IsEmpty())
			{
				OutInvalid.SuggestedRoute = OutDecision.SuggestedRoute;
			}
			return false;
		}

		return true;
	}

	static FBlueprintHelperClassDefaultSetterMutationResult ApplySetterMutation(
		const FString& AssetPath,
		UObject* CDO,
		const FBlueprintHelperClassDefaultResolvedMutationTarget& Target,
		const TSharedPtr<FJsonValue>& Value,
		const bool bDryRun)
	{
		FBlueprintHelperClassDefaultSetterMutationRequest Request;
		Request.AssetPath = AssetPath;
		Request.RootObject = CDO;
		Request.Target = Target;
		Request.Value = Value;
		Request.bDryRun = bDryRun;
		const FBlueprintHelperClassDefaultSetterMutationService Service;
		return Service.Apply(Request);
	}
};

// ─── 构造函数 ───

FBlueprintHelperClassSettingsService::FBlueprintHelperClassSettingsService(
	const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

// ─── ResolveBlueprint ───

UBlueprint* FBlueprintHelperClassSettingsService::ResolveBlueprint(
	const FString& AssetPath,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	if (AssetPath.IsEmpty())
	{
		OutErrorCode = TEXT("blueprint_not_found");
		OutErrorMessage = TEXT("asset_path 不能为空。");
		return nullptr;
	}

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());
	if (!Blueprint)
	{
		OutErrorCode = TEXT("blueprint_not_found");
		OutErrorMessage = Diag.Items.Num() > 0
			? Diag.Items[0].Message
			: FString::Printf(TEXT("无法解析蓝图资产: %s"), *AssetPath);
		return nullptr;
	}

	return Blueprint;
}

// ─── NormalizeObjectPath ───

FString FBlueprintHelperClassSettingsService::NormalizeObjectPath(const FString& Path)
{
	if (Path.IsEmpty() || Path.StartsWith(TEXT("/Script/")))
	{
		return Path;
	}

	// 已经是 /Game/A/B.AssetName 形式。
	if (Path.Contains(TEXT(".")))
	{
		return Path;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(Path);
	return FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
}

// ─── MakeError / MakeValidation ───

FBlueprintHelperToolError FBlueprintHelperClassSettingsService::MakeError(
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
{
	FBlueprintHelperToolError Error;
	Error.Code = Code;
	Error.Stage = Stage;
	Error.Message = Message;
	Error.bRetryable = false;
	Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
	Error.Field = Field;
	return Error;
}

FBlueprintHelperToolError FBlueprintHelperClassSettingsService::MakeClassDefaultReadError(
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
{
	FString PublicCode = Code;
	if (PublicCode == TEXT("blueprint_not_found"))
	{
		PublicCode = TEXT("asset_not_found");
	}
	else if (PublicCode == TEXT("interface_compile_required"))
	{
		PublicCode = TEXT("class_default_cdo_unavailable");
	}
	else if (PublicCode == TEXT("object_reference_not_found"))
	{
		PublicCode = TEXT("class_default_object_reference_not_found");
	}
	else if (PublicCode == TEXT("struct_field_invalid"))
	{
		PublicCode = TEXT("class_default_property_path_not_traversable");
	}

	FBlueprintHelperToolError Error = MakeError(PublicCode, Stage, Message, Field);
	if (PublicCode == TEXT("class_default_property_not_found") ||
		PublicCode == TEXT("class_default_object_reference_not_found") ||
		PublicCode == TEXT("class_default_property_path_not_traversable"))
	{
		Error.Category = TEXT("parameter_error");
		Error.SafeNextAction = TEXT("correct_property_path_then_retry");
	}
	else if (PublicCode == TEXT("class_default_cdo_unavailable"))
	{
		Error.Category = TEXT("runtime_state_error");
		Error.SafeNextAction = TEXT("compile_blueprint_then_retry");
	}
	else
	{
		Error.Category = TEXT("context_error");
		Error.SafeNextAction = TEXT("inspect_blueprint_class_settings_then_retry");
	}
	return Error;
}

FBlueprintHelperValidationSummary FBlueprintHelperClassSettingsService::MakeValidation(
	bool bShouldCompile,
	bool bShouldSave)
{
	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bShouldCompile;
	Validation.bShouldSave = bShouldSave;
	Validation.bCompiled = false;
	Validation.bSaved = false;
	Validation.bCompileSuccess = false;
	return Validation;
}

// ─── read_class_settings 工具函数 ───

FString FBlueprintHelperClassSettingsService::GetClassPath(const UClass* Class)
{
	return Class ? Class->GetPathName() : TEXT("");
}

FString FBlueprintHelperClassSettingsService::GetGeneratedClassShortName(const UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->GetName()
		: TEXT("");
}

FString FBlueprintHelperClassSettingsService::GetInterfaceAssetPath(const UClass* InterfaceClass)
{
	if (!InterfaceClass)
	{
		return TEXT("");
	}

	if (UObject* GeneratedBy = InterfaceClass->ClassGeneratedBy)
	{
		return GeneratedBy->GetPathName();
	}

	// Native interface fallback。
	return InterfaceClass->GetPathName();
}

int32 FBlueprintHelperClassSettingsService::CountEditableClassDefaults(UObject* CDO)
{
	if (!CDO)
	{
		return 0;
	}

	int32 Count = 0;
	for (TFieldIterator<FProperty> It(CDO->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (FBlueprintHelperEditablePropertyPolicy::AllowsClassDefaultWrite(Prop))
		{
			++Count;
		}
	}
	return Count;
}

// ─── read_class_settings ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::ReadClassSettings(
	const FString& AssetPath) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_class_settings"),
			TraceId,
			MakeError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage));
	}

	FBlueprintHelperClassSettingsSummary Summary;
	Summary.ParentClass = GetClassPath(Blueprint->ParentClass);
	Summary.GeneratedClass = GetGeneratedClassShortName(Blueprint);

	for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
	{
		Summary.ImplementedInterfaces.Add(GetInterfaceAssetPath(Desc.Interface));
	}

	UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
	Summary.ClassDefaultCount = CountEditableClassDefaults(CDO);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintClassSettings.v1"));
	Data->SetObjectField(TEXT("class_settings"), Summary.ToJson());

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_class_settings"), TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	Result.Data = Data;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::ReadClassDefaultProperty(
	const FString& AssetPath,
	const FString& PropertyPath) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_blueprint_class_default_property"),
			TraceId,
			MakeClassDefaultReadError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage, TEXT("asset_path")));
	}

	FString CdoErrorCode;
	FString CdoErrorMessage;
	UObject* CDO = ResolveClassDefaultObject(Blueprint, CdoErrorCode, CdoErrorMessage);
	if (!CDO)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_blueprint_class_default_property"),
			TraceId,
			MakeClassDefaultReadError(CdoErrorCode, EBlueprintHelperToolStage::ResolveTarget, CdoErrorMessage, TEXT("property_path")));
	}

	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;
	FString ExpectedType;
	FString ResolveCode;
	FString ResolveMessage;
	if (!ResolvePropertyPath(CDO, PropertyPath, Property, ValuePtr, ExpectedType, ResolveCode, ResolveMessage))
	{
		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_blueprint_class_default_property"),
			TraceId,
			MakeClassDefaultReadError(ResolveCode, EBlueprintHelperToolStage::ResolveTarget, ResolveMessage, TEXT("property_path")));
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Property;
		Failed.Target->PropertyPath = PropertyPath;
		return Failed;
	}

	FBlueprintHelperClassDefaultPropertyContext Context;
	Context.AssetPath = AssetPath;
	Context.PropertyPath = PropertyPath;
	Context.ClassName = CDO->GetClass()->GetName();
	Context.TypeName = ExpectedType;
	Context.bFound = true;
	Property->ExportTextItem_Direct(Context.Value, ValuePtr, nullptr, CDO, PPF_None);
	if (Property->HasMetaData(TEXT("Category")))
	{
		Context.Category = Property->GetMetaData(TEXT("Category"));
	}
	Context.Flags = FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(Property->PropertyFlags);
	Context.bDirectWriteWritable = FBlueprintHelperEditablePropertyPolicy::AllowsClassDefaultWrite(Property);
	if (!Context.bDirectWriteWritable)
	{
		Context.DirectWriteBlockedBy = FBlueprintHelperClassSettingsServiceLocal::BuildDirectWriteBlockedBy(Property);
	}

	FBlueprintHelperClassDefaultResolvedMutationTarget MutationTarget;
	FString MutationResolveCode;
	FString MutationResolveMessage;
	const FBlueprintHelperClassDefaultPropertyMutationResolver MutationResolver;
	if (MutationResolver.Resolve(CDO, PropertyPath, MutationTarget, MutationResolveCode, MutationResolveMessage))
	{
		Context.OwnerObjectPath = MutationTarget.OwnerObjectPath;
		Context.OwnerObjectClass = MutationTarget.OwnerObjectClass;
		Context.SetterFunction = MutationTarget.SetterFunctionName;
		Context.GetterFunction = MutationTarget.GetterFunctionName;

		const FBlueprintHelperClassDefaultPropertyMutationPolicy MutationPolicy;
		const FBlueprintHelperClassDefaultMutationPolicyDecision SetterDecision =
			MutationPolicy.Decide(MutationTarget, TEXT("setter_aware_property"));
		Context.bSetterAwareWriteSupported =
			SetterDecision.Strategy == EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty;
		if (Context.bSetterAwareWriteSupported)
		{
			const FBlueprintHelperToolSuggestedRoute Route =
				FBlueprintHelperClassDefaultPropertyMutationPolicy::MakeSetterAwareSuggestedRoute(PropertyPath);
			Context.SetterAwareRouteId = Route.RouteId;
			Context.SetterAwareTargetKind = TEXT("class_default_setter_property");
		}
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_class_default_property"),
		TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Property;
	Result.Target->PropertyPath = PropertyPath;
	Result.Data = Context.ToJson();
	return Result;
}

// ─── ResolveInterfaceClass ───

UClass* FBlueprintHelperClassSettingsService::ResolveInterfaceClass(
	const FString& InterfacePath,
	FString& OutCode,
	FString& OutMessage) const
{
	if (InterfacePath.IsEmpty())
	{
		OutCode = TEXT("interface_not_found");
		OutMessage = TEXT("interface_path 不能为空。");
		return nullptr;
	}

	const FString ObjectPath = NormalizeObjectPath(InterfacePath);
	UBlueprint* InterfaceBP = Cast<UBlueprint>(StaticLoadObject(
		UBlueprint::StaticClass(), nullptr, *ObjectPath));

	if (!InterfaceBP)
	{
		OutCode = TEXT("interface_load_failed");
		OutMessage = FString::Printf(TEXT("无法加载接口资产: %s"), *InterfacePath);
		return nullptr;
	}

	if (InterfaceBP->BlueprintType != BPTYPE_Interface)
	{
		OutCode = TEXT("not_blueprint_interface");
		OutMessage = FString::Printf(TEXT("资产不是 Blueprint Interface: %s"), *InterfacePath);
		return nullptr;
	}

	UClass* InterfaceClass = InterfaceBP->GeneratedClass;
	if (!InterfaceClass)
	{
		OutCode = TEXT("interface_compile_required");
		OutMessage = FString::Printf(TEXT("接口资产没有 GeneratedClass，可能需要先编译: %s"), *InterfacePath);
		return nullptr;
	}

	if (!InterfaceClass->IsChildOf(UInterface::StaticClass()))
	{
		OutCode = TEXT("invalid_blueprint_interface");
		OutMessage = FString::Printf(TEXT("GeneratedClass 不是 UInterface 子类: %s"), *InterfacePath);
		return nullptr;
	}

	return InterfaceClass;
}

// ─── IsInterfaceImplemented ───

UClass* FBlueprintHelperClassSettingsService::ResolveParentClass(
	const FString& ParentClassPath,
	FString& OutCode,
	FString& OutMessage) const
{
	const FString TrimmedPath = ParentClassPath.TrimStartAndEnd();
	if (TrimmedPath.IsEmpty())
	{
		OutCode = TEXT("parent_class_required");
		OutMessage = TEXT("new_parent_class cannot be empty.");
		return nullptr;
	}

	TArray<FString> CandidatePaths;
	CandidatePaths.Add(TrimmedPath);
	CandidatePaths.Add(NormalizeObjectPath(TrimmedPath));
	if (!TrimmedPath.StartsWith(TEXT("/")) && !TrimmedPath.Contains(TEXT(".")))
	{
		CandidatePaths.Add(FString::Printf(TEXT("/Script/Engine.%s"), *TrimmedPath));
	}

	UClass* ParentClass = nullptr;
	for (const FString& CandidatePath : CandidatePaths)
	{
		if (CandidatePath.IsEmpty())
		{
			continue;
		}
		ParentClass = FindObject<UClass>(nullptr, *CandidatePath);
		if (!ParentClass)
		{
			ParentClass = LoadObject<UClass>(nullptr, *CandidatePath);
		}
		if (ParentClass)
		{
			break;
		}
	}

	if (!ParentClass)
	{
		OutCode = TEXT("parent_class_not_found");
		OutMessage = FString::Printf(TEXT("Could not resolve Blueprint parent class: %s"), *ParentClassPath);
		return nullptr;
	}

	if (ParentClass->HasAnyClassFlags(CLASS_Interface) ||
		!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		OutCode = TEXT("invalid_blueprint_parent_class");
		OutMessage = FString::Printf(TEXT("Class is not a valid Blueprint parent: %s"), *ParentClass->GetPathName());
		return nullptr;
	}

	return ParentClass;
}

bool FBlueprintHelperClassSettingsService::IsInterfaceImplemented(
	UBlueprint* Blueprint,
	UClass* InterfaceClass)
{
	if (!Blueprint || !InterfaceClass)
	{
		return false;
	}

	for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
	{
		if (Desc.Interface == InterfaceClass)
		{
			return true;
		}
	}
	return false;
}

// ─── AddInterfaceToBlueprint ───

bool FBlueprintHelperClassSettingsService::AddInterfaceToBlueprint(
	UBlueprint* Blueprint,
	UClass* InterfaceClass)
{
	if (!Blueprint || !InterfaceClass || IsInterfaceImplemented(Blueprint, InterfaceClass))
	{
		return false;
	}

	// 优先走 UE 编辑器工具 API，保证内部 Class Settings 状态一致。
	FBlueprintEditorUtils::ImplementNewInterface(Blueprint, FTopLevelAssetPath(InterfaceClass));
	return IsInterfaceImplemented(Blueprint, InterfaceClass);
}

// ─── RemoveInterfaceFromBlueprint ───

bool FBlueprintHelperClassSettingsService::RemoveInterfaceFromBlueprint(
	UBlueprint* Blueprint,
	UClass* InterfaceClass)
{
	if (!Blueprint || !InterfaceClass || !IsInterfaceImplemented(Blueprint, InterfaceClass))
	{
		return false;
	}

	for (int32 Index = 0; Index < Blueprint->ImplementedInterfaces.Num(); ++Index)
	{
		if (Blueprint->ImplementedInterfaces[Index].Interface == InterfaceClass)
		{
			// 若目标 UE 版本有 FBlueprintEditorUtils::RemoveInterface(UBlueprint*, FBPInterfaceDescription&)，优先替换为该 API。
			Blueprint->Modify();
			Blueprint->ImplementedInterfaces.RemoveAt(Index);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			return true;
		}
	}

	return false;
}

// ─── AddImplementedInterfaces 事务式实现 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::AddImplementedInterfaces(
	const FString& AssetPath,
	const TArray<FString>& InterfacePaths,
	bool bDryRun) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FBlueprintHelperInterfaceResult InterfaceResult;
	InterfaceResult.Mode = EBlueprintHelperClassSettingsOperationMode::Batch;
	InterfaceResult.RequestedCount = InterfacePaths.Num();

	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("add_implemented_interfaces"),
			TraceId,
			MakeError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage));
	}

	// Preflight：解析所有接口。
	TArray<UClass*> InterfaceClasses;
	for (const FString& InterfacePath : InterfacePaths)
	{
		FString Code;
		FString Message;
		UClass* InterfaceClass = ResolveInterfaceClass(InterfacePath, Code, Message);
		if (!InterfaceClass)
		{
			FBlueprintHelperInvalidInterface Invalid;
			Invalid.InterfacePath = InterfacePath;
			Invalid.Code = Code;
			Invalid.Message = Message;
			InterfaceResult.InvalidInterfaces.Add(MoveTemp(Invalid));
			continue;
		}

		InterfaceClasses.Add(InterfaceClass);
	}

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintClassSettings.v1"));

	if (InterfaceResult.InvalidInterfaces.Num() > 0)
	{
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("add_implemented_interfaces"),
			TraceId,
			MakeError(TEXT("invalid_blueprint_interface"), EBlueprintHelperToolStage::Preflight,
				UBlueprintHelperClassSettingsUtils::BlueprintClassSettingsDescribeInvalidInterface(InterfaceResult.InvalidInterfaces[0]),
				TEXT("interface_paths")));
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Failed.Data = Data;
		return Failed;
	}

	// 统计已实现的接口。
	for (UClass* InterfaceClass : InterfaceClasses)
	{
		if (IsInterfaceImplemented(Blueprint, InterfaceClass))
		{
			++InterfaceResult.AlreadyImplementedCount;
		}
	}

	const int32 ToApplyCount = InterfaceClasses.Num() - InterfaceResult.AlreadyImplementedCount;
	if (ToApplyCount <= 0)
	{
		if (bDryRun)
		{
			Data->SetBoolField(TEXT("dry_run"), true);
		}
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase NoOp = bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(TEXT("add_implemented_interfaces"), TraceId)
			: FBlueprintHelperToolResultBuilder::NoOp(TEXT("add_implemented_interfaces"), TraceId);
		NoOp.Target = FBlueprintHelperTargetRef();
		NoOp.Target->AssetPath = AssetPath;
		NoOp.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		NoOp.Data = Data;
		NoOp.Validation = MakeValidation(false, false);
		return NoOp;
	}

	if (bDryRun)
	{
		InterfaceResult.AppliedCount = ToApplyCount;
		Data->SetBoolField(TEXT("dry_run"), true);
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("add_implemented_interfaces"), TraceId);
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Result.Data = Data;
		return Result;
	}

	// 执行添加。
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Add Implemented Interfaces")),
		Blueprint);

	for (UClass* InterfaceClass : InterfaceClasses)
	{
		if (!IsInterfaceImplemented(Blueprint, InterfaceClass))
		{
			if (AddInterfaceToBlueprint(Blueprint, InterfaceClass))
			{
				++InterfaceResult.AppliedCount;
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("add_implemented_interfaces"), TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	Result.Data = Data;
	Result.Validation = MakeValidation(true, true);
	return Result;
}

// ─── AddImplementedInterface 单接口包装 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::AddImplementedInterface(
	const FString& AssetPath,
	const FString& InterfacePath,
	bool bDryRun) const
{
	// 委托到批量内部实现，传入 Mode=Single。
	FBlueprintHelperToolResultBase Result = AddImplementedInterfaces(AssetPath, { InterfacePath }, bDryRun);
	Result.Operation = TEXT("add_implemented_interface");
	if (Result.Target.IsSet())
	{
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	}
	// 修正 mode 为 single。
	if (Result.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* InterfaceResultObj = nullptr;
		if (Result.Data->TryGetObjectField(TEXT("interface_result"), InterfaceResultObj) && InterfaceResultObj && InterfaceResultObj->IsValid())
		{
			(*InterfaceResultObj)->SetStringField(TEXT("mode"), TEXT("single"));
		}
	}
	return Result;
}

// ─── RemoveImplementedInterfaces 事务式实现 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::RemoveImplementedInterfaces(
	const FString& AssetPath,
	const TArray<FString>& InterfacePaths,
	bool bDryRun) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FBlueprintHelperInterfaceResult InterfaceResult;
	InterfaceResult.Mode = EBlueprintHelperClassSettingsOperationMode::Batch;
	InterfaceResult.RequestedCount = InterfacePaths.Num();

	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("remove_implemented_interfaces"),
			TraceId,
			MakeError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage));
	}

	// Preflight：解析所有接口。
	TArray<UClass*> InterfaceClasses;
	for (const FString& InterfacePath : InterfacePaths)
	{
		FString Code;
		FString Message;
		UClass* InterfaceClass = ResolveInterfaceClass(InterfacePath, Code, Message);
		if (!InterfaceClass)
		{
			FBlueprintHelperInvalidInterface Invalid;
			Invalid.InterfacePath = InterfacePath;
			Invalid.Code = Code;
			Invalid.Message = Message;
			InterfaceResult.InvalidInterfaces.Add(MoveTemp(Invalid));
			continue;
		}

		InterfaceClasses.Add(InterfaceClass);
	}

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintClassSettings.v1"));

	if (InterfaceResult.InvalidInterfaces.Num() > 0)
	{
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("remove_implemented_interfaces"),
			TraceId,
			MakeError(TEXT("invalid_blueprint_interface"), EBlueprintHelperToolStage::Preflight,
				UBlueprintHelperClassSettingsUtils::BlueprintClassSettingsDescribeInvalidInterface(InterfaceResult.InvalidInterfaces[0]),
				TEXT("interface_paths")));
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Failed.Data = Data;
		return Failed;
	}

	// 统计已实现的接口。
	for (UClass* InterfaceClass : InterfaceClasses)
	{
		if (IsInterfaceImplemented(Blueprint, InterfaceClass))
		{
			++InterfaceResult.AlreadyImplementedCount;
		}
	}

	if (InterfaceResult.AlreadyImplementedCount == 0)
	{
		if (bDryRun)
		{
			Data->SetBoolField(TEXT("dry_run"), true);
		}
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase NoOp = bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(TEXT("remove_implemented_interfaces"), TraceId)
			: FBlueprintHelperToolResultBuilder::NoOp(TEXT("remove_implemented_interfaces"), TraceId);
		NoOp.Target = FBlueprintHelperTargetRef();
		NoOp.Target->AssetPath = AssetPath;
		NoOp.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		NoOp.Data = Data;
		NoOp.Validation = MakeValidation(false, false);
		return NoOp;
	}

	if (bDryRun)
	{
		InterfaceResult.RemovedCount = InterfaceResult.AlreadyImplementedCount;
		Data->SetBoolField(TEXT("dry_run"), true);
		Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_implemented_interfaces"), TraceId);
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Result.Data = Data;
		return Result;
	}

	// 执行移除。
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Remove Implemented Interfaces")),
		Blueprint);

	for (UClass* InterfaceClass : InterfaceClasses)
	{
		if (IsInterfaceImplemented(Blueprint, InterfaceClass))
		{
			if (RemoveInterfaceFromBlueprint(Blueprint, InterfaceClass))
			{
				++InterfaceResult.RemovedCount;
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Data->SetObjectField(TEXT("interface_result"), InterfaceResult.ToJson());

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("remove_implemented_interfaces"), TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	Result.Data = Data;
	Result.Validation = MakeValidation(true, true);
	return Result;
}

// ─── RemoveImplementedInterface 单接口包装 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::RemoveImplementedInterface(
	const FString& AssetPath,
	const FString& InterfacePath,
	bool bDryRun) const
{
	FBlueprintHelperToolResultBase Result = RemoveImplementedInterfaces(AssetPath, { InterfacePath }, bDryRun);
	Result.Operation = TEXT("remove_implemented_interface");
	if (Result.Target.IsSet())
	{
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	}
	// 修正 mode 为 single。
	if (Result.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* InterfaceResultObj = nullptr;
		if (Result.Data->TryGetObjectField(TEXT("interface_result"), InterfaceResultObj) && InterfaceResultObj && InterfaceResultObj->IsValid())
		{
			(*InterfaceResultObj)->SetStringField(TEXT("mode"), TEXT("single"));
		}
	}
	return Result;
}

// ─── ResolveClassDefaultObject ───

UObject* FBlueprintHelperClassSettingsService::ResolveClassDefaultObject(
	UBlueprint* Blueprint,
	FString& OutCode,
	FString& OutMessage) const
{
	if (!Blueprint)
	{
		OutCode = TEXT("blueprint_not_found");
		OutMessage = TEXT("Blueprint 为空。");
		return nullptr;
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		OutCode = TEXT("interface_compile_required");
		OutMessage = TEXT("Blueprint 没有 GeneratedClass，可能需要先编译。");
		return nullptr;
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		OutCode = TEXT("class_default_cdo_unavailable");
		OutMessage = TEXT("无法获取 Blueprint CDO。");
		return nullptr;
	}

	return CDO;
}

// ─── ResolvePropertyPath ───

bool FBlueprintHelperClassSettingsService::ResolvePropertyPath(
	UObject* RootObject,
	const FString& PropertyPath,
	FProperty*& OutProperty,
	void*& OutValuePtr,
	FString& OutExpectedType,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutProperty = nullptr;
	OutValuePtr = nullptr;

	if (!RootObject)
	{
		OutErrorCode = TEXT("class_default_property_not_found");
		OutErrorMessage = TEXT("CDO 为空。");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutErrorCode = TEXT("class_default_property_not_found");
		OutErrorMessage = TEXT("property_path 不能为空。");
		return false;
	}

	UStruct* CurrentStruct = RootObject->GetClass();
	void* CurrentContainer = RootObject;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bLast = Index == Segments.Num() - 1;
		FProperty* Property = CurrentStruct
			? CurrentStruct->FindPropertyByName(FName(*Segments[Index]))
			: nullptr;

		if (!Property)
		{
			OutErrorCode = TEXT("class_default_property_not_found");
			OutErrorMessage = FString::Printf(TEXT("未找到 Class Default 属性路径段: %s"), *Segments[Index]);
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
		if (bLast)
		{
			OutProperty = Property;
			OutValuePtr = ValuePtr;
			OutExpectedType = Property->GetCPPType();
			return true;
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			CurrentStruct = StructProp->Struct;
			CurrentContainer = ValuePtr;
			continue;
		}

		if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* NestedObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
			if (!NestedObject)
			{
				OutErrorCode = TEXT("object_reference_not_found");
				OutErrorMessage = FString::Printf(TEXT("属性路径段 %s 的对象引用为空。"), *Segments[Index]);
				return false;
			}
			CurrentStruct = NestedObject->GetClass();
			CurrentContainer = NestedObject;
			continue;
		}

		OutErrorCode = TEXT("struct_field_invalid");
		OutErrorMessage = FString::Printf(TEXT("属性路径段 %s 不是 struct/object，不能继续解析。"), *Segments[Index]);
		return false;
	}

	return false;
}

// ─── JsonValueToImportText ───

bool FBlueprintHelperClassSettingsService::JsonValueToImportText(
	const TSharedPtr<FJsonValue>& Value,
	FString& OutText,
	FString& OutSummary,
	FString& OutActualType,
	FString& OutError)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		OutActualType = TEXT("null");
		OutError = TEXT("value 不能为空。");
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutText = Value->AsString();
		OutSummary = OutText.Left(128);
		OutActualType = TEXT("string");
		return true;
	case EJson::Boolean:
		OutText = Value->AsBool() ? TEXT("true") : TEXT("false");
		OutSummary = OutText;
		OutActualType = TEXT("bool");
		return true;
	case EJson::Number:
		OutText = LexToString(Value->AsNumber());
		OutSummary = OutText;
		OutActualType = TEXT("number");
		return true;
	default:
		OutActualType = TEXT("object_or_array");
		OutError = TEXT("第一版 Class Default value 只支持 string / bool / number。复杂 struct 请使用叶子属性路径。");
		return false;
	}
}

// ─── ValidateClassDefaultSetting ───

bool FBlueprintHelperClassSettingsService::ValidateClassDefaultSetting(
	UObject* CDO,
	const FBlueprintHelperClassDefaultPropertySetting& Setting,
	FBlueprintHelperInvalidClassDefaultSetting& OutInvalid)
{
	FBlueprintHelperClassDefaultResolvedMutationTarget MutationTarget;
	FBlueprintHelperClassDefaultMutationPolicyDecision MutationDecision;
	if (!FBlueprintHelperClassSettingsServiceLocal::ResolveMutationDecision(
		CDO,
		Setting,
		MutationTarget,
		MutationDecision,
		OutInvalid))
	{
		return false;
	}

	if (MutationDecision.Strategy == EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty)
	{
		const FBlueprintHelperClassDefaultSetterMutationResult SetterPreview =
			FBlueprintHelperClassSettingsServiceLocal::ApplySetterMutation(
				FString(),
				CDO,
				MutationTarget,
				Setting.Value,
				true);
		if (!SetterPreview.bOk)
		{
			OutInvalid.Code = SetterPreview.ErrorCode;
			OutInvalid.ExpectedType = MutationTarget.ExpectedType;
			OutInvalid.ValueSummary = SetterPreview.ErrorMessage.Left(128);
			return false;
		}
		return true;
	}

	FString ImportText;
	FString Summary;
	FString ActualType;
	FString ConvertError;
	if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ActualType, ConvertError))
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ActualType = ActualType;
		OutInvalid.ValueSummary = ConvertError.Left(128);
		return false;
	}

	// 试导入到临时内存，验证 ImportText 格式正确。
	void* TempValue = FMemory_Alloca(MutationTarget.LeafProperty->GetSize());
	MutationTarget.LeafProperty->InitializeValue(TempValue);
	const TCHAR* ImportEnd = MutationTarget.LeafProperty->ImportText_Direct(*ImportText, TempValue, CDO, PPF_None);
	MutationTarget.LeafProperty->DestroyValue(TempValue);

	if (!ImportEnd)
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ActualType = ActualType;
		OutInvalid.ValueSummary = Summary.Left(128);
		return false;
	}

	return true;
}

// ─── SetClassDefaultProperties 事务式实现 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::SetClassDefaultProperties(
	const FString& AssetPath,
	const TArray<FBlueprintHelperClassDefaultPropertySetting>& Settings,
	bool bDryRun) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FBlueprintHelperDefaultPropertyResult PropertyResult;
	PropertyResult.Mode = EBlueprintHelperClassSettingsOperationMode::Batch;
	PropertyResult.RequestedCount = Settings.Num();

	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("set_class_default_properties"),
			TraceId,
			MakeError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage));
	}

	FString CdoErrorCode;
	FString CdoErrorMessage;
	UObject* CDO = ResolveClassDefaultObject(Blueprint, CdoErrorCode, CdoErrorMessage);
	if (!CDO)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("set_class_default_properties"),
			TraceId,
			MakeError(CdoErrorCode, EBlueprintHelperToolStage::ResolveTarget, CdoErrorMessage));
	}

	// Preflight：验证所有设置。
	for (const FBlueprintHelperClassDefaultPropertySetting& Setting : Settings)
	{
		FBlueprintHelperInvalidClassDefaultSetting Invalid;
		if (!ValidateClassDefaultSetting(CDO, Setting, Invalid))
		{
			PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
		}
	}

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintClassSettings.v1"));

	if (PropertyResult.InvalidSettings.Num() > 0)
	{
		Data->SetObjectField(TEXT("default_property_result"), PropertyResult.ToJson());

		const FBlueprintHelperInvalidClassDefaultSetting& FirstInvalid = PropertyResult.InvalidSettings[0];
		const FString FailureCode = FirstInvalid.Code.Equals(TEXT("class_default_property_setter_required"), ESearchCase::IgnoreCase)
			? FirstInvalid.Code
			: TEXT("invalid_class_default_property_settings");
		FBlueprintHelperToolError Error = MakeError(
			FailureCode,
			EBlueprintHelperToolStage::Preflight,
			UBlueprintHelperClassSettingsUtils::BlueprintClassSettingsDescribeInvalidDefaultSetting(FirstInvalid),
			FirstInvalid.PropertyPath);
		Error.SafeNextAction = FirstInvalid.SafeNextAction;
		if (FirstInvalid.SuggestedRoute.IsSet())
		{
			Error.SuggestedRoute = FirstInvalid.SuggestedRoute;
			Data->SetObjectField(TEXT("suggested_route"), FirstInvalid.SuggestedRoute->ToJson());
		}

		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("set_class_default_properties"),
			TraceId,
			Error);
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Failed.Data = Data;
		return Failed;
	}

	if (bDryRun)
	{
		for (const FBlueprintHelperClassDefaultPropertySetting& Setting : Settings)
		{
			FBlueprintHelperClassDefaultResolvedMutationTarget MutationTarget;
			FBlueprintHelperClassDefaultMutationPolicyDecision MutationDecision;
			FBlueprintHelperInvalidClassDefaultSetting Invalid;
			if (!FBlueprintHelperClassSettingsServiceLocal::ResolveMutationDecision(
				CDO,
				Setting,
				MutationTarget,
				MutationDecision,
				Invalid))
			{
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(Invalid.Code, EBlueprintHelperToolStage::DryRun, Invalid.ValueSummary, Invalid.PropertyPath));
			}

			if (MutationDecision.Strategy == EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty)
			{
				const FBlueprintHelperClassDefaultSetterMutationResult SetterPreview =
					FBlueprintHelperClassSettingsServiceLocal::ApplySetterMutation(
						AssetPath,
						CDO,
						MutationTarget,
						Setting.Value,
						true);
				if (!SetterPreview.bOk)
				{
					return FBlueprintHelperToolResultBuilder::Failure(
						TEXT("set_class_default_properties"),
						TraceId,
						MakeError(SetterPreview.ErrorCode, EBlueprintHelperToolStage::DryRun, SetterPreview.ErrorMessage, Setting.PropertyPath));
				}

				PropertyResult.SetterMutationEvidence.Add(SetterPreview.Evidence);
				++PropertyResult.AppliedCount;
				if (SetterPreview.bWouldChange)
				{
					++PropertyResult.ChangedCount;
				}
				else
				{
					++PropertyResult.NoOpCount;
				}
				continue;
			}

			FString Before;
			MutationTarget.LeafProperty->ExportTextItem_Direct(Before, MutationTarget.LeafValuePtr, nullptr, CDO, PPF_None);

			FString ImportText;
			FString Summary;
			FString ActualType;
			FString ConvertError;
			if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ActualType, ConvertError))
			{
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::DryRun, ConvertError));
			}

			void* TempValue = FMemory_Alloca(MutationTarget.LeafProperty->GetSize());
			MutationTarget.LeafProperty->InitializeValue(TempValue);
			const TCHAR* ImportEnd = MutationTarget.LeafProperty->ImportText_Direct(*ImportText, TempValue, CDO, PPF_None);
			if (!ImportEnd)
			{
				MutationTarget.LeafProperty->DestroyValue(TempValue);
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::DryRun,
						FString::Printf(TEXT("属性预览写入失败: %s"), *Setting.PropertyPath)));
			}

			FString After;
			MutationTarget.LeafProperty->ExportTextItem_Direct(After, TempValue, nullptr, CDO, PPF_None);
			MutationTarget.LeafProperty->DestroyValue(TempValue);

			++PropertyResult.AppliedCount;
			if (Before == After)
			{
				++PropertyResult.NoOpCount;
			}
			else
			{
				++PropertyResult.ChangedCount;
			}
		}

		Data->SetBoolField(TEXT("dry_run"), true);
		Data->SetObjectField(TEXT("default_property_result"), PropertyResult.ToJson());

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("set_class_default_properties"), TraceId);
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Result.Data = Data;
		Result.Validation = MakeValidation(false, false);
		return Result;
	}

	// 执行写入。
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Class Default Properties")),
		Blueprint);
	Mutation.Modify(CDO);

	for (const FBlueprintHelperClassDefaultPropertySetting& Setting : Settings)
	{
		FBlueprintHelperClassDefaultResolvedMutationTarget MutationTarget;
		FBlueprintHelperClassDefaultMutationPolicyDecision MutationDecision;
		FBlueprintHelperInvalidClassDefaultSetting Invalid;
		if (!FBlueprintHelperClassSettingsServiceLocal::ResolveMutationDecision(
			CDO,
			Setting,
			MutationTarget,
			MutationDecision,
			Invalid))
		{
			Mutation.Rollback();
			return FBlueprintHelperToolResultBuilder::Failure(
				TEXT("set_class_default_properties"),
				TraceId,
				MakeError(Invalid.Code, EBlueprintHelperToolStage::Execute, Invalid.ValueSummary, Invalid.PropertyPath));
		}

		if (MutationDecision.Strategy == EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty)
		{
			const FBlueprintHelperClassDefaultSetterMutationResult SetterExecute =
				FBlueprintHelperClassSettingsServiceLocal::ApplySetterMutation(
					AssetPath,
					CDO,
					MutationTarget,
					Setting.Value,
					false);
			if (!SetterExecute.bOk)
			{
				Mutation.Rollback();
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(SetterExecute.ErrorCode, EBlueprintHelperToolStage::Execute, SetterExecute.ErrorMessage, Setting.PropertyPath));
			}

			PropertyResult.SetterMutationEvidence.Add(SetterExecute.Evidence);
			++PropertyResult.AppliedCount;
			if (SetterExecute.bModified)
			{
				++PropertyResult.ChangedCount;
			}
			else
			{
				++PropertyResult.NoOpCount;
			}
			continue;
		}

		FString Before;
		MutationTarget.LeafProperty->ExportTextItem_Direct(Before, MutationTarget.LeafValuePtr, nullptr, CDO, PPF_None);

		FString ImportText;
		FString Summary;
		FString ActualType;
		FString ConvertError;
		JsonValueToImportText(Setting.Value, ImportText, Summary, ActualType, ConvertError);

		CDO->PreEditChange(MutationTarget.LeafProperty);
		const TCHAR* ImportEnd = MutationTarget.LeafProperty->ImportText_Direct(*ImportText, MutationTarget.LeafValuePtr, CDO, PPF_None);
		if (!ImportEnd)
		{
			Mutation.Rollback();
			return FBlueprintHelperToolResultBuilder::Failure(
				TEXT("set_class_default_properties"),
				TraceId,
				MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::Execute,
					FString::Printf(TEXT("属性写入失败: %s"), *Setting.PropertyPath)));
		}

		FPropertyChangedEvent ChangeEvent(MutationTarget.LeafProperty, EPropertyChangeType::ValueSet);
		CDO->PostEditChangeProperty(ChangeEvent);

		FString After;
		MutationTarget.LeafProperty->ExportTextItem_Direct(After, MutationTarget.LeafValuePtr, nullptr, CDO, PPF_None);

		++PropertyResult.AppliedCount;
		if (Before == After)
		{
			++PropertyResult.NoOpCount;
		}
		else
		{
			++PropertyResult.ChangedCount;
		}
	}

	if (PropertyResult.ChangedCount == 0)
	{
		Mutation.Rollback();
		Data->SetObjectField(TEXT("default_property_result"), PropertyResult.ToJson());

		FBlueprintHelperToolResultBase NoOp = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("set_class_default_properties"), TraceId);
		NoOp.Target = FBlueprintHelperTargetRef();
		NoOp.Target->AssetPath = AssetPath;
		NoOp.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		NoOp.Data = Data;
		NoOp.Validation = MakeValidation(false, false);
		return NoOp;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Mutation.Commit();

	Data->SetObjectField(TEXT("default_property_result"), PropertyResult.ToJson());

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("set_class_default_properties"), TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	Result.Data = Data;
	Result.Validation = MakeValidation(true, true);
	return Result;
}

// ─── SetClassDefaultProperty 单属性包装 ───

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::SetClassDefaultProperty(
	const FString& AssetPath,
	const FString& PropertyPath,
	const TSharedPtr<FJsonValue>& Value,
	bool bDryRun) const
{
	FBlueprintHelperClassDefaultPropertySetting Setting;
	Setting.PropertyPath = PropertyPath;
	Setting.Value = Value;

	FBlueprintHelperToolResultBase Result = SetClassDefaultProperties(AssetPath, { Setting }, bDryRun);
	Result.Operation = TEXT("set_class_default_property");
	if (Result.Target.IsSet())
	{
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	}
	// 修正 mode 为 single。
	if (Result.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* PropResultObj = nullptr;
		if (Result.Data->TryGetObjectField(TEXT("default_property_result"), PropResultObj) && PropResultObj && PropResultObj->IsValid())
		{
			(*PropResultObj)->SetStringField(TEXT("mode"), TEXT("single"));
		}
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsService::ReparentBlueprint(
	const FString& AssetPath,
	const FString& NewParentClassPath,
	bool bDryRun) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FString ErrorCode;
	FString ErrorMessage;
	UBlueprint* Blueprint = ResolveBlueprint(AssetPath, ErrorCode, ErrorMessage);
	if (!Blueprint)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("reparent_blueprint"),
			TraceId,
			MakeError(ErrorCode, EBlueprintHelperToolStage::ResolveTarget, ErrorMessage));
	}

	FString ParentErrorCode;
	FString ParentErrorMessage;
	UClass* NewParentClass = ResolveParentClass(NewParentClassPath, ParentErrorCode, ParentErrorMessage);
	if (!NewParentClass)
	{
		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("reparent_blueprint"),
			TraceId,
			MakeError(ParentErrorCode, EBlueprintHelperToolStage::Preflight, ParentErrorMessage, TEXT("new_parent_class")));
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		return Failed;
	}

	if (Blueprint->GeneratedClass && NewParentClass->IsChildOf(Blueprint->GeneratedClass))
	{
		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("reparent_blueprint"),
			TraceId,
			MakeError(
				TEXT("invalid_reparent_cycle"),
				EBlueprintHelperToolStage::Preflight,
				FString::Printf(TEXT("Cannot reparent Blueprint to itself or one of its generated child classes: %s"), *NewParentClass->GetPathName()),
				TEXT("new_parent_class")));
		Failed.Target = FBlueprintHelperTargetRef();
		Failed.Target->AssetPath = AssetPath;
		Failed.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		return Failed;
	}

	FBlueprintHelperReparentResult ReparentResult;
	ReparentResult.PreviousParentClass = GetClassPath(Blueprint->ParentClass);
	ReparentResult.NewParentClass = GetClassPath(NewParentClass);
	ReparentResult.bAlreadyParented = Blueprint->ParentClass.Get() == NewParentClass;

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintClassSettings.v1"));
	Data->SetObjectField(TEXT("reparent_result"), ReparentResult.ToJson());

	if (ReparentResult.bAlreadyParented)
	{
		if (bDryRun)
		{
			Data->SetBoolField(TEXT("dry_run"), true);
		}
		FBlueprintHelperToolResultBase NoOp = bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(TEXT("reparent_blueprint"), TraceId)
			: FBlueprintHelperToolResultBuilder::NoOp(TEXT("reparent_blueprint"), TraceId);
		NoOp.Target = FBlueprintHelperTargetRef();
		NoOp.Target->AssetPath = AssetPath;
		NoOp.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		NoOp.Data = Data;
		NoOp.Validation = MakeValidation(false, false);
		return NoOp;
	}

	if (bDryRun)
	{
		Data->SetBoolField(TEXT("dry_run"), true);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("reparent_blueprint"), TraceId);
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
		Result.Data = Data;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Reparent Blueprint")),
		Blueprint);

	Blueprint->ParentClass = NewParentClass;
	if (Blueprint->SimpleConstructionScript)
	{
		Blueprint->SimpleConstructionScript->ValidateSceneRootNodes();
	}
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("reparent_blueprint"), TraceId);
	Result.Target = FBlueprintHelperTargetRef();
	Result.Target->AssetPath = AssetPath;
	Result.Target->TargetType = EBlueprintHelperTargetType::Blueprint;
	Result.Data = Data;
	Result.Validation = MakeValidation(true, true);
	return Result;
}
