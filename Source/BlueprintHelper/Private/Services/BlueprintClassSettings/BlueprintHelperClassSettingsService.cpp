// BlueprintHelper Service Layer — Blueprint Class Settings 服务实现

#include "Services/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperServiceTypes.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#include "UObject/Interface.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag);
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
	const FString& Message)
{
	FBlueprintHelperToolError Error;
	Error.Code = Code;
	Error.Stage = Stage;
	Error.Message = Message;
	Error.bRetryable = false;
	Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
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
		if (FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Prop))
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
				TEXT("One or more interfaces are invalid.")));
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
				TEXT("One or more interfaces are invalid.")));
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
		OutCode = TEXT("class_default_property_not_found");
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
	OutInvalid.PropertyPath = Setting.PropertyPath;

	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;
	FString ExpectedType;
	FString ErrorCode;
	FString ErrorMessage;

	if (!ResolvePropertyPath(CDO, Setting.PropertyPath, Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
	{
		OutInvalid.Code = ErrorCode;
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = ErrorMessage.Left(128);
		return false;
	}

	OutInvalid.ExpectedType = ExpectedType;

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
	{
		OutInvalid.Code = TEXT("class_default_property_not_writable");
		return false;
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
	void* TempValue = FMemory_Alloca(Property->GetSize());
	Property->InitializeValue(TempValue);
	const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, TempValue, CDO, PPF_None);
	Property->DestroyValue(TempValue);

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

		FBlueprintHelperToolResultBase Failed = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("set_class_default_properties"),
			TraceId,
			MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::Preflight,
				TEXT("One or more class default property settings are invalid.")));
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
			FProperty* Property = nullptr;
			void* ValuePtr = nullptr;
			FString ExpectedType;
			FString ResolveCode;
			FString ResolveMessage;

			if (!ResolvePropertyPath(CDO, Setting.PropertyPath, Property, ValuePtr, ExpectedType, ResolveCode, ResolveMessage))
			{
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(ResolveCode, EBlueprintHelperToolStage::DryRun, ResolveMessage));
			}

			FString Before;
			Property->ExportTextItem_Direct(Before, ValuePtr, nullptr, CDO, PPF_None);

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

			void* TempValue = FMemory_Alloca(Property->GetSize());
			Property->InitializeValue(TempValue);
			const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, TempValue, CDO, PPF_None);
			if (!ImportEnd)
			{
				Property->DestroyValue(TempValue);
				return FBlueprintHelperToolResultBuilder::Failure(
					TEXT("set_class_default_properties"),
					TraceId,
					MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::DryRun,
						FString::Printf(TEXT("属性预览写入失败: %s"), *Setting.PropertyPath)));
			}

			FString After;
			Property->ExportTextItem_Direct(After, TempValue, nullptr, CDO, PPF_None);
			Property->DestroyValue(TempValue);

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
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType;
		FString ResolveCode;
		FString ResolveMessage;

		if (!ResolvePropertyPath(CDO, Setting.PropertyPath, Property, ValuePtr, ExpectedType, ResolveCode, ResolveMessage))
		{
			Mutation.Rollback();
			return FBlueprintHelperToolResultBuilder::Failure(
				TEXT("set_class_default_properties"),
				TraceId,
				MakeError(ResolveCode, EBlueprintHelperToolStage::Execute, ResolveMessage));
		}

		FString Before;
		Property->ExportTextItem_Direct(Before, ValuePtr, nullptr, CDO, PPF_None);

		FString ImportText;
		FString Summary;
		FString ActualType;
		FString ConvertError;
		JsonValueToImportText(Setting.Value, ImportText, Summary, ActualType, ConvertError);

		CDO->PreEditChange(Property);
		const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, ValuePtr, CDO, PPF_None);
		if (!ImportEnd)
		{
			Mutation.Rollback();
			return FBlueprintHelperToolResultBuilder::Failure(
				TEXT("set_class_default_properties"),
				TraceId,
				MakeError(TEXT("invalid_class_default_property_settings"), EBlueprintHelperToolStage::Execute,
					FString::Printf(TEXT("属性写入失败: %s"), *Setting.PropertyPath)));
		}

		FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ValueSet);
		CDO->PostEditChangeProperty(ChangeEvent);

		FString After;
		Property->ExportTextItem_Direct(After, ValuePtr, nullptr, CDO, PPF_None);

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
