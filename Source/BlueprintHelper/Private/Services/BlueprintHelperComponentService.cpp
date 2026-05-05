// BlueprintHelper Service Layer — Blueprint Component 服务实现

#include "Services/BlueprintHelperComponentService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "BlueprintHelper.h"

#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ═══════════════════════════════════════════════════════════
// ToJson
// ═══════════════════════════════════════════════════════════

namespace
{
	struct FBlueprintHelperComponentOperationState
	{
		bool bOk = false;
		FString Operation;
		FString Status;
		bool bModified = false;

		FString AssetPath;
		FString TargetType;
		FString ComponentName;
		FString ComponentClass;
		FString PropertyPath;

		FBlueprintHelperComponentInfo Component;
		FBlueprintHelperComponentAttachmentInfo Attachment;
		FBlueprintHelperComponentNameCollisionInfo NameCollision;
		FBlueprintHelperComponentPropertyResult PropertyResult;

		TArray<FBlueprintHelperComponentInfo> Components;
		int32 ComponentCount = 0;
		int32 RootComponentCount = 0;

		bool bShouldCompile = false;
		bool bShouldSave = false;

		FString ErrorCode;
		FString ErrorStage;
		FString ErrorMessage;
		bool bRetryable = false;
		FString RollbackResult = TEXT("not_needed");
	};

	EBlueprintHelperToolStage ComponentErrorStageFromString(const FString& Stage)
	{
		if (Stage == TEXT("resolve_blueprint") ||
			Stage == TEXT("resolve_component") ||
			Stage == TEXT("resolve_component_class") ||
			Stage == TEXT("resolve_parent_component"))
		{
			return EBlueprintHelperToolStage::ResolveTarget;
		}

		if (Stage == TEXT("preflight") || Stage == TEXT("name_collision"))
		{
			return EBlueprintHelperToolStage::Preflight;
		}

		return EBlueprintHelperToolStage::Execute;
	}

	EBlueprintHelperRollbackResult ComponentRollbackFromString(const FString& RollbackResult)
	{
		return RollbackResult == TEXT("rolled_back")
			? EBlueprintHelperRollbackResult::RolledBack
			: EBlueprintHelperRollbackResult::NotNeeded;
	}

	FBlueprintHelperToolError MakeComponentError(const FBlueprintHelperComponentOperationState& State)
	{
		FBlueprintHelperToolError Error;
		Error.Code = State.ErrorCode;
		Error.Stage = ComponentErrorStageFromString(State.ErrorStage);
		Error.Message = State.ErrorMessage;
		Error.bRetryable = State.bRetryable;
		Error.RollbackResult = ComponentRollbackFromString(State.RollbackResult);
		return Error;
	}

	TSharedRef<FJsonObject> MakeComponentTargetJson(const FBlueprintHelperComponentOperationState& State)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), State.AssetPath);
		Target->SetStringField(TEXT("target_type"), State.TargetType);
		if (!State.ComponentName.IsEmpty()) Target->SetStringField(TEXT("component_name"), State.ComponentName);
		if (!State.ComponentClass.IsEmpty()) Target->SetStringField(TEXT("component_class"), State.ComponentClass);
		if (!State.PropertyPath.IsEmpty()) Target->SetStringField(TEXT("property_path"), State.PropertyPath);
		return Target;
	}

	TSharedRef<FJsonObject> MakeComponentDataJson(const FBlueprintHelperComponentOperationState& State)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintComponent.v1"));

		if (State.Operation == TEXT("read_components"))
		{
			TArray<TSharedPtr<FJsonValue>> Comps;
			for (const auto& Info : State.Components)
			{
				Comps.Add(MakeShared<FJsonValueObject>(Info.ToJson(false)));
			}
			Data->SetArrayField(TEXT("components"), Comps);
			TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
			Stats->SetNumberField(TEXT("components"), State.ComponentCount);
			Stats->SetNumberField(TEXT("root_components"), State.RootComponentCount);
			Data->SetObjectField(TEXT("stats"), Stats);
		}
		else if (State.Operation == TEXT("add_component"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(true));
			Data->SetObjectField(TEXT("attachment"), State.Attachment.ToJson());
			Data->SetObjectField(TEXT("name_collision"), State.NameCollision.ToJson());
		}
		else if (State.Operation == TEXT("set_component_property") || State.Operation == TEXT("set_component_properties"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(false));
			Data->SetObjectField(TEXT("property_result"), State.PropertyResult.ToJson());
		}
		else if (State.Operation == TEXT("remove_component"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(false));
		}

		return Data;
	}

	FBlueprintHelperValidationSummary MakeComponentValidation(const FBlueprintHelperComponentOperationState& State)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = State.bShouldCompile;
		Validation.bShouldSave = State.bShouldSave;
		return Validation;
	}

	FBlueprintHelperToolResultBase BuildComponentToolResult(const FBlueprintHelperComponentOperationState& State)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

		FBlueprintHelperToolResultBase Result = !State.bOk
			? FBlueprintHelperToolResultBuilder::Failure(State.Operation, TraceId, MakeComponentError(State))
			: State.Status == TEXT("applied")
				? FBlueprintHelperToolResultBuilder::Applied(State.Operation, TraceId)
				: State.Status == TEXT("no_op")
					? FBlueprintHelperToolResultBuilder::NoOp(State.Operation, TraceId)
					: FBlueprintHelperToolResultBuilder::Completed(State.Operation, TraceId);

		Result.bModified = State.bModified;
		Result.CustomTargetJson = MakeComponentTargetJson(State);
		Result.Data = MakeComponentDataJson(State);
		if (State.Operation != TEXT("read_components") && State.bOk)
		{
			Result.Validation = MakeComponentValidation(State);
		}
		return Result;
	}
}

// ═══════════════════════════════════════════════════════════
// 服务实现
// ═══════════════════════════════════════════════════════════

FBlueprintHelperComponentService::FBlueprintHelperComponentService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

// ─── ResolveBlueprint ───

UBlueprint* FBlueprintHelperComponentService::ResolveBlueprint(const FString& AssetPath, FString& OutError) const
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag);
	if (!Blueprint)
	{
		OutError = Diag.Items.Num() > 0
			? Diag.Items[0].Message
			: FString::Printf(TEXT("无法解析蓝图资产: %s"), *AssetPath);
	}
	return Blueprint;
}

// ─── FindComponentNodeByName ───

USCS_Node* FBlueprintHelperComponentService::FindComponentNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}

	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node) continue;
		if (Node->GetVariableName().ToString() == ComponentName)
		{
			return Node;
		}
	}

	return nullptr;
}

// ─── GetShortComponentClassName ───

FString FBlueprintHelperComponentService::GetShortComponentClassName(const UClass* ComponentClass)
{
	if (!ComponentClass) return TEXT("");
	FString Name = ComponentClass->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

// ─── ResolveComponentClass ───

UClass* FBlueprintHelperComponentService::ResolveComponentClass(const FString& ComponentClass, FString& OutError)
{
	if (ComponentClass.IsEmpty())
	{
		OutError = TEXT("component_class 不能为空。");
		return nullptr;
	}

	UClass* Class = nullptr;

	if (ComponentClass.StartsWith(TEXT("/Script/")))
	{
		Class = LoadClass<UActorComponent>(nullptr, *ComponentClass);
	}
	else
	{
		const FString EnginePath = FString::Printf(TEXT("/Script/Engine.%s"), *ComponentClass);
		Class = LoadClass<UActorComponent>(nullptr, *EnginePath);

		if (!Class && !ComponentClass.EndsWith(TEXT("Component")))
		{
			const FString WithSuffixPath = FString::Printf(TEXT("/Script/Engine.%sComponent"), *ComponentClass);
			Class = LoadClass<UActorComponent>(nullptr, *WithSuffixPath);
		}
	}

	if (!Class)
	{
		OutError = FString::Printf(TEXT("不支持或无法加载组件类: %s"), *ComponentClass);
		return nullptr;
	}

	if (!Class->IsChildOf(UActorComponent::StaticClass()))
	{
		OutError = FString::Printf(TEXT("%s 不是 UActorComponent 子类。"), *ComponentClass);
		return nullptr;
	}

	return Class;
}

// ─── ReadComponents ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::ReadComponents(
	const FBlueprintHelperReadComponentsRequest& Request) const
{
	FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("read_components");
	Result.Status = TEXT("completed");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("blueprint");

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error.IsEmpty() ? TEXT("蓝图没有 SimpleConstructionScript。") : Error;
		return BuildComponentToolResult(Result);
	}

	TSet<FString> RootNames;
	for (USCS_Node* RootNode : Blueprint->SimpleConstructionScript->GetRootNodes())
	{
		if (RootNode) RootNames.Add(RootNode->GetVariableName().ToString());
	}

	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node || !Node->ComponentTemplate) continue;

		FBlueprintHelperComponentInfo Info;
		Info.ComponentName = Node->GetVariableName().ToString();
		Info.ComponentClass = GetShortComponentClassName(Node->ComponentTemplate->GetClass());

		Info.ParentComponent = Node->ParentComponentOrVariableName.IsNone() ? TEXT("") : Node->ParentComponentOrVariableName.ToString();

		for (USCS_Node* Child : Node->GetChildNodes())
		{
			if (Child) Info.Children.Add(Child->GetVariableName().ToString());
		}

		Result.Components.Add(MoveTemp(Info));
	}

	Result.ComponentCount = Result.Components.Num();
	Result.RootComponentCount = RootNames.Num();
	Result.bOk = true;
	return BuildComponentToolResult(Result);
}

// ─── AddComponent ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::AddComponent(
	const FBlueprintHelperAddComponentRequest& Request) const
{
	FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("add_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.ComponentClass = Request.ComponentClass;
	Result.NameCollision.Policy = Request.NameCollisionPolicy;
	Result.Attachment.ParentComponent = Request.ParentComponent;
	Result.Attachment.SocketName = Request.SocketName;
	Result.Attachment.AttachRule = Request.AttachRule;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error.IsEmpty() ? TEXT("蓝图没有 SimpleConstructionScript。") : Error;
		return BuildComponentToolResult(Result);
	}

	FString ClassError;
	UClass* ComponentClass = ResolveComponentClass(Request.ComponentClass, ClassError);
	if (!ComponentClass)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("unsupported_component_class");
		Result.ErrorStage = TEXT("resolve_component_class");
		Result.ErrorMessage = ClassError;
		return BuildComponentToolResult(Result);
	}

	// 名称冲突检查
	USCS_Node* Existing = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (Existing)
	{
		Result.NameCollision.bHandled = true;
		Result.NameCollision.ExistingComponentName = Request.ComponentName;
		Result.Component.ComponentName = Request.ComponentName;
		Result.Component.ComponentClass = Existing->ComponentTemplate
			? GetShortComponentClassName(Existing->ComponentTemplate->GetClass()) : TEXT("");
		Result.Component.bCreated = false;
		Result.Component.bAlreadyExisted = true;

		// 类型不匹配
		if (!Existing->ComponentTemplate || !Existing->ComponentTemplate->GetClass()->IsChildOf(ComponentClass))
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_type_mismatch");
			Result.ErrorStage = TEXT("name_collision");
			Result.ErrorMessage = FString::Printf(TEXT("组件 %s 已存在，但类型不是 %s。"), *Request.ComponentName, *Request.ComponentClass);
			return BuildComponentToolResult(Result);
		}

		if (Request.NameCollisionPolicy == EBlueprintHelperComponentNameCollisionPolicy::FailIfExists)
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_already_exists");
			Result.ErrorStage = TEXT("name_collision");
			Result.ErrorMessage = FString::Printf(TEXT("组件已存在: %s"), *Request.ComponentName);
			return BuildComponentToolResult(Result);
		}

		Result.bOk = true;
		Result.Status = TEXT("no_op");
		Result.bModified = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		return BuildComponentToolResult(Result);
	}

	// 父组件查找
	USCS_Node* ParentNode = nullptr;
	if (!Request.ParentComponent.IsEmpty())
	{
		ParentNode = FindComponentNodeByName(Blueprint, Request.ParentComponent);
		if (!ParentNode)
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("parent_component_not_found");
			Result.ErrorStage = TEXT("resolve_parent_component");
			Result.ErrorMessage = FString::Printf(TEXT("未找到父组件: %s"), *Request.ParentComponent);
			return BuildComponentToolResult(Result);
		}
	}

	// 创建组件
	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Add Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);

	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*Request.ComponentName));
	if (!NewNode || !NewNode->ComponentTemplate)
	{
		Mutation.Rollback();
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("execution_failed");
		Result.ErrorStage = TEXT("create_scs_node");
		Result.ErrorMessage = TEXT("创建 SCS 组件节点失败。");
		Result.RollbackResult = TEXT("rolled_back");
		return BuildComponentToolResult(Result);
	}

	Mutation.Modify(NewNode);
	Mutation.Modify(NewNode->ComponentTemplate);

	if (ParentNode)
	{
		Mutation.Modify(ParentNode);
		ParentNode->AddChildNode(NewNode);
	}
	else
	{
		Blueprint->SimpleConstructionScript->AddNode(NewNode);
	}

	if (!Request.SocketName.IsEmpty())
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewNode->ComponentTemplate))
		{
			SceneComp->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.Component.ComponentName = Request.ComponentName;
	Result.Component.ComponentClass = GetShortComponentClassName(ComponentClass);
	Result.Component.bCreated = true;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return BuildComponentToolResult(Result);
}

// ─── ResolvePropertyPath ───

bool FBlueprintHelperComponentService::ResolvePropertyPath(
	UObject* RootObject, const FString& PropertyPath,
	FProperty*& OutProperty, void*& OutValuePtr,
	FString& OutExpectedType, FString& OutErrorCode, FString& OutErrorMessage)
{
	OutProperty = nullptr;
	OutValuePtr = nullptr;

	if (!RootObject)
	{
		OutErrorCode = TEXT("component_not_found");
		OutErrorMessage = TEXT("组件模板为空。");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutErrorCode = TEXT("property_not_found");
		OutErrorMessage = TEXT("property_path 不能为空。");
		return false;
	}

	UStruct* CurrentStruct = RootObject->GetClass();
	void* CurrentContainer = RootObject;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bLast = Index == Segments.Num() - 1;
		const FName SegmentName(*Segments[Index]);

		FProperty* Property = CurrentStruct ? CurrentStruct->FindPropertyByName(SegmentName) : nullptr;
		if (!Property)
		{
			OutErrorCode = TEXT("property_not_found");
			OutErrorMessage = FString::Printf(TEXT("未找到属性路径段: %s"), *Segments[Index]);
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

bool FBlueprintHelperComponentService::JsonValueToImportText(
	const TSharedPtr<FJsonValue>& Value, FString& OutText, FString& OutSummary, FString& OutError)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		OutError = TEXT("value 不能为空。");
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutText = Value->AsString();
		OutSummary = OutText.Left(128);
		return true;
	case EJson::Boolean:
		OutText = Value->AsBool() ? TEXT("true") : TEXT("false");
		OutSummary = OutText;
		return true;
	case EJson::Number:
		OutText = LexToString(Value->AsNumber());
		OutSummary = OutText;
		return true;
	default:
		OutError = TEXT("第一版组件属性 value 只支持 string / bool / number。");
		return false;
	}
}

// ─── ValidatePropertySetting ───

bool FBlueprintHelperComponentService::ValidatePropertySetting(
	UObject* ComponentTemplate, const FBlueprintHelperComponentPropertySetting& Setting,
	FBlueprintHelperInvalidComponentPropertySetting& OutInvalid)
{
	OutInvalid.PropertyPath = Setting.PropertyPath;

	FString ExpectedType, ErrorCode, ErrorMessage;
	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;

	if (!ResolvePropertyPath(ComponentTemplate, Setting.PropertyPath, Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
	{
		OutInvalid.Code = ErrorCode;
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = ErrorMessage.Left(128);
		return false;
	}

	OutInvalid.ExpectedType = ExpectedType;

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
	{
		OutInvalid.Code = TEXT("property_not_writable");
		return false;
	}

	FString ImportText, Summary, ConvertError;
	if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ConvertError))
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = ConvertError.Left(128);
		return false;
	}

	// 尝试通过 ImportText_Direct 验证
	void* TempValue = FMemory_Alloca(Property->GetSize());
	Property->InitializeValue(TempValue);
	const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, TempValue, ComponentTemplate, PPF_None);
	Property->DestroyValue(TempValue);

	if (!ImportEnd)
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = Summary;
		return false;
	}

	return true;
}

// ─── SetComponentProperties ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::SetComponentProperties(
	const FBlueprintHelperSetComponentPropertiesRequest& Request) const
{
	FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("set_component_properties");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.PropertyResult.Mode = EBlueprintHelperComponentPropertyMode::Batch;
	Result.PropertyResult.RequestedCount = Request.Settings.Num();

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error;
		return BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_component");
		Result.ErrorMessage = FString::Printf(TEXT("未找到组件: %s"), *Request.ComponentName);
		return BuildComponentToolResult(Result);
	}

	Result.Component.ComponentName = Request.ComponentName;
	Result.Component.ComponentClass = GetShortComponentClassName(Node->ComponentTemplate->GetClass());

	// Preflight: 检查所有属性设置
	for (const auto& Setting : Request.Settings)
	{
		FBlueprintHelperInvalidComponentPropertySetting Invalid;
		if (!ValidatePropertySetting(Node->ComponentTemplate, Setting, Invalid))
		{
			Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
		}
	}

	if (Result.PropertyResult.InvalidSettings.Num() > 0)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.bModified = false;
		Result.ErrorCode = TEXT("invalid_component_property_settings");
		Result.ErrorStage = TEXT("preflight");
		Result.ErrorMessage = TEXT("One or more component property settings are invalid.");
		Result.PropertyResult.AppliedCount = 0;
		Result.PropertyResult.ChangedCount = 0;
		Result.PropertyResult.NoOpCount = 0;
		return BuildComponentToolResult(Result);
	}

	// 应用属性
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Component Properties")), Blueprint);

	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);

	for (const auto& Setting : Request.Settings)
	{
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType, ErrorCode, ErrorMessage;

		if (!ResolvePropertyPath(Node->ComponentTemplate, Setting.PropertyPath,
			Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
		{
			Mutation.Rollback();
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("invalid_component_property_settings");
			Result.ErrorStage = TEXT("apply");
			Result.ErrorMessage = ErrorMessage;
			Result.RollbackResult = TEXT("rolled_back");
			return BuildComponentToolResult(Result);
		}

		FString Before;
		Property->ExportTextItem_Direct(Before, ValuePtr, nullptr, Node->ComponentTemplate, PPF_None);

		FString ImportText, Summary, ConvertError;
		JsonValueToImportText(Setting.Value, ImportText, Summary, ConvertError);

		const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, ValuePtr, Node->ComponentTemplate, PPF_None);
		if (!ImportEnd)
		{
			Mutation.Rollback();
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("invalid_component_property_settings");
			Result.ErrorStage = TEXT("apply");
			Result.ErrorMessage = FString::Printf(TEXT("属性写入失败: %s"), *Setting.PropertyPath);
			Result.RollbackResult = TEXT("rolled_back");
			return BuildComponentToolResult(Result);
		}

		FString After;
		Property->ExportTextItem_Direct(After, ValuePtr, nullptr, Node->ComponentTemplate, PPF_None);

		++Result.PropertyResult.AppliedCount;
		if (Before == After) ++Result.PropertyResult.NoOpCount;
		else ++Result.PropertyResult.ChangedCount;
	}

	if (Result.PropertyResult.ChangedCount == 0)
	{
		Mutation.Rollback();
		Result.bOk = true;
		Result.Status = TEXT("no_op");
		Result.bModified = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		return BuildComponentToolResult(Result);
	}

	Node->ComponentTemplate->PostEditChange();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return BuildComponentToolResult(Result);
}

// ─── SetComponentProperty ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::SetComponentProperty(
	const FBlueprintHelperSetComponentPropertiesRequest& Request) const
{
	FBlueprintHelperToolResultBase Result = SetComponentProperties(Request);
	Result.Operation = TEXT("set_component_property");
	if (Request.Settings.Num() > 0)
	{
		if (Result.CustomTargetJson.IsValid())
		{
			Result.CustomTargetJson->SetStringField(TEXT("property_path"), Request.Settings[0].PropertyPath);
		}
		if (Result.Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* PropertyResultObj = nullptr;
			if (Result.Data->TryGetObjectField(TEXT("property_result"), PropertyResultObj) &&
				PropertyResultObj &&
				PropertyResultObj->IsValid())
			{
				(*PropertyResultObj)->SetStringField(TEXT("mode"), TEXT("single"));
				(*PropertyResultObj)->SetNumberField(TEXT("requested_count"), 1);
			}
		}
	}
	return Result;
}

// ─── RemoveComponent ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::RemoveComponent(
	const FBlueprintHelperRemoveComponentRequest& Request) const
{
	FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("remove_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error;
		return BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_component");
		Result.ErrorMessage = FString::Printf(TEXT("未找到组件: %s"), *Request.ComponentName);
		return BuildComponentToolResult(Result);
	}

	if (Request.ComponentName == TEXT("DefaultSceneRoot") || Node->GetChildNodes().Num() > 0)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("remove_component_blocked");
		Result.ErrorStage = TEXT("preflight");
		Result.ErrorMessage = TEXT("第一版不删除根组件或带子组件的组件。");
		return BuildComponentToolResult(Result);
	}

	Result.Component.ComponentName = Request.ComponentName;
	Result.Component.ComponentClass = GetShortComponentClassName(Node->ComponentTemplate->GetClass());

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Remove Component")), Blueprint);

	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);

	Blueprint->SimpleConstructionScript->RemoveNode(Node);
	const bool bRemoved = true; // RemoveNode 返回 void，成功执行到这里即成功
	if (!bRemoved)
	{
		Mutation.Rollback();
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("execution_failed");
		Result.ErrorStage = TEXT("remove_scs_node");
		Result.ErrorMessage = TEXT("删除 SCS 组件节点失败。");
		Result.RollbackResult = TEXT("rolled_back");
		return BuildComponentToolResult(Result);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.Component.bRemoved = true;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return BuildComponentToolResult(Result);
}
