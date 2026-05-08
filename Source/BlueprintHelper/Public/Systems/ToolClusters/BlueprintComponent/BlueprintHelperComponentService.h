// BlueprintHelper Service Layer — Blueprint Component 服务类型定义与服务声明
// 第 5 簇：蓝图组件树的读取、添加、属性设置、删除

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonValue;
class FBlueprintHelperGraphResolver;
class UBlueprint;
class USCS_Node;
class UActorComponent;
class UClass;

// ─── 枚举 ───

enum class EBlueprintHelperAttachRule : uint8
{
	KeepRelative,
	SnapToTarget
};

inline const TCHAR* AttachRuleToString(EBlueprintHelperAttachRule Rule)
{
	switch (Rule)
	{
	case EBlueprintHelperAttachRule::KeepRelative: return TEXT("keep_relative");
	case EBlueprintHelperAttachRule::SnapToTarget: return TEXT("snap_to_target");
	default:                                       return TEXT("keep_relative");
	}
}

static bool TryParseAttachRule(const FString& Text, EBlueprintHelperAttachRule& OutRule)
{
	if (Text.IsEmpty() || Text == TEXT("keep_relative")) { OutRule = EBlueprintHelperAttachRule::KeepRelative; return true; }
	if (Text == TEXT("snap_to_target")) { OutRule = EBlueprintHelperAttachRule::SnapToTarget; return true; }
	return false;
}

enum class EBlueprintHelperComponentNameCollisionPolicy : uint8
{
	FailIfExists,
	ReuseIfExists
};

inline const TCHAR* NameCollisionPolicyToString(EBlueprintHelperComponentNameCollisionPolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperComponentNameCollisionPolicy::FailIfExists:  return TEXT("fail_if_exists");
	case EBlueprintHelperComponentNameCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists");
	default:                                                          return TEXT("fail_if_exists");
	}
}

static bool TryParseNameCollisionPolicy(const FString& Text, EBlueprintHelperComponentNameCollisionPolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("fail_if_exists")) { OutPolicy = EBlueprintHelperComponentNameCollisionPolicy::FailIfExists; return true; }
	if (Text == TEXT("reuse_if_exists")) { OutPolicy = EBlueprintHelperComponentNameCollisionPolicy::ReuseIfExists; return true; }
	return false;
}

enum class EBlueprintHelperComponentPropertyMode : uint8
{
	Single,
	Batch
};

inline const TCHAR* PropertyModeToString(EBlueprintHelperComponentPropertyMode Mode)
{
	return Mode == EBlueprintHelperComponentPropertyMode::Single ? TEXT("single") : TEXT("batch");
}

// ─── 请求结构体 ───

struct FBlueprintHelperReadComponentsRequest
{
	FString AssetPath;
};

struct FBlueprintHelperComponentPropertySetting
{
	FString PropertyPath;
	TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperSetComponentPropertiesRequest
{
	FString AssetPath;
	FString ComponentName;
	EBlueprintHelperComponentPropertyMode Mode = EBlueprintHelperComponentPropertyMode::Batch;
	TArray<FBlueprintHelperComponentPropertySetting> Settings;
	bool bDryRun = false;
};

struct FBlueprintHelperAddComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	FString ComponentClass;
	FString ParentComponent;
	FString SocketName;
	EBlueprintHelperAttachRule AttachRule = EBlueprintHelperAttachRule::KeepRelative;
	EBlueprintHelperComponentNameCollisionPolicy NameCollisionPolicy = EBlueprintHelperComponentNameCollisionPolicy::FailIfExists;
	bool bDryRun = false;
};

struct FBlueprintHelperRemoveComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	bool bDryRun = false;
};

// ─── 返回子结构体 ───

struct FBlueprintHelperInvalidComponentPropertySetting
{
	FString PropertyPath;
	FString Code;
	FString ExpectedType;
	FString ActualType;
	FString ValueSummary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("property_path"), PropertyPath);
		J->SetStringField(TEXT("code"), Code);
		if (!ExpectedType.IsEmpty()) J->SetStringField(TEXT("expected_type"), ExpectedType);
		if (!ActualType.IsEmpty()) J->SetStringField(TEXT("actual_type"), ActualType);
		if (!ValueSummary.IsEmpty()) J->SetStringField(TEXT("value_summary"), ValueSummary.Left(128));
		return J;
	}
};

struct FBlueprintHelperComponentPropertyResult
{
	EBlueprintHelperComponentPropertyMode Mode = EBlueprintHelperComponentPropertyMode::Single;
	int32 RequestedCount = 0;
	int32 AppliedCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
	TArray<FBlueprintHelperInvalidComponentPropertySetting> InvalidSettings;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("mode"), PropertyModeToString(Mode));
		J->SetNumberField(TEXT("requested_count"), RequestedCount);
		J->SetNumberField(TEXT("applied_count"), AppliedCount);
		J->SetNumberField(TEXT("changed_count"), ChangedCount);
		J->SetNumberField(TEXT("no_op_count"), NoOpCount);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const auto& Inv : InvalidSettings) { Arr.Add(MakeShared<FJsonValueObject>(Inv.ToJson())); }
		J->SetArrayField(TEXT("invalid_settings"), Arr);
		return J;
	}
};

struct FBlueprintHelperComponentInfo
{
	FString ComponentName;
	FString ComponentClass;
	FString ParentComponent;
	TArray<FString> Children;
	bool bCreated = false;
	bool bAlreadyExisted = false;
	bool bRemoved = false;

	TSharedRef<FJsonObject> ToJson(bool bIncludeAddFlags = true) const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("component_name"), ComponentName);
		J->SetStringField(TEXT("component_class"), ComponentClass);
		if (ParentComponent.IsEmpty()) J->SetField(TEXT("parent_component"), MakeShared<FJsonValueNull>());
		else J->SetStringField(TEXT("parent_component"), ParentComponent);
		if (Children.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> C;
			for (const FString& Ch : Children) C.Add(MakeShared<FJsonValueString>(Ch));
			J->SetArrayField(TEXT("children"), C);
		}
		if (bIncludeAddFlags) { J->SetBoolField(TEXT("created"), bCreated); J->SetBoolField(TEXT("already_existed"), bAlreadyExisted); }
		if (bRemoved) J->SetBoolField(TEXT("removed"), true);
		return J;
	}
};

struct FBlueprintHelperComponentAttachmentInfo
{
	FString ParentComponent;
	FString SocketName;
	EBlueprintHelperAttachRule AttachRule = EBlueprintHelperAttachRule::KeepRelative;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("parent_component"), ParentComponent);
		if (!SocketName.IsEmpty()) J->SetStringField(TEXT("socket_name"), SocketName);
		J->SetStringField(TEXT("attach_rule"), AttachRuleToString(AttachRule));
		return J;
	}
};

struct FBlueprintHelperComponentNameCollisionInfo
{
	EBlueprintHelperComponentNameCollisionPolicy Policy = EBlueprintHelperComponentNameCollisionPolicy::FailIfExists;
	bool bHandled = false;
	FString ExistingComponentName;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("policy"), NameCollisionPolicyToString(Policy));
		J->SetBoolField(TEXT("handled"), bHandled);
		if (!ExistingComponentName.IsEmpty()) J->SetStringField(TEXT("existing_component_name"), ExistingComponentName);
		return J;
	}
};

// ─── 顶层工具结果 ───

// ─── 服务声明 ───

class BLUEPRINTHELPER_API FBlueprintHelperComponentService
{
public:
	explicit FBlueprintHelperComponentService(const FBlueprintHelperGraphResolver& InResolver);

	FBlueprintHelperToolResultBase ReadComponents(const FBlueprintHelperReadComponentsRequest& Request) const;
	FBlueprintHelperToolResultBase AddComponent(const FBlueprintHelperAddComponentRequest& Request) const;
	FBlueprintHelperToolResultBase SetComponentProperty(const FBlueprintHelperSetComponentPropertiesRequest& Request) const;
	FBlueprintHelperToolResultBase SetComponentProperties(const FBlueprintHelperSetComponentPropertiesRequest& Request) const;
	FBlueprintHelperToolResultBase RemoveComponent(const FBlueprintHelperRemoveComponentRequest& Request) const;

private:
	UBlueprint* ResolveBlueprint(const FString& AssetPath, FString& OutError) const;
	static USCS_Node* FindComponentNodeByName(UBlueprint* Blueprint, const FString& ComponentName);
	static UClass* ResolveComponentClass(const FString& ComponentClass, FString& OutError);
	static FString GetShortComponentClassName(const UClass* ComponentClass);

	static bool ResolvePropertyPath(UObject* RootObject, const FString& PropertyPath,
		FProperty*& OutProperty, void*& OutValuePtr, FString& OutExpectedType,
		FString& OutErrorCode, FString& OutErrorMessage);

	static bool JsonValueToImportText(const TSharedPtr<FJsonValue>& Value,
		FString& OutText, FString& OutSummary, FString& OutError);

	static bool ValidatePropertySetting(UObject* ComponentTemplate,
		const FBlueprintHelperComponentPropertySetting& Setting,
		FBlueprintHelperInvalidComponentPropertySetting& OutInvalid);

	const FBlueprintHelperGraphResolver& Resolver;
};
