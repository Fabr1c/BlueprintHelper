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
	KeepWorld,
	SnapToTarget
};

inline const TCHAR* AttachRuleToString(EBlueprintHelperAttachRule Rule)
{
	switch (Rule)
	{
	case EBlueprintHelperAttachRule::KeepRelative: return TEXT("keep_relative");
	case EBlueprintHelperAttachRule::KeepWorld: return TEXT("keep_world");
	case EBlueprintHelperAttachRule::SnapToTarget: return TEXT("snap_to_target");
	default:                                       return TEXT("keep_relative");
	}
}

static bool TryParseAttachRule(const FString& Text, EBlueprintHelperAttachRule& OutRule)
{
	if (Text.IsEmpty() || Text == TEXT("keep_relative")) { OutRule = EBlueprintHelperAttachRule::KeepRelative; return true; }
	if (Text == TEXT("keep_world")) { OutRule = EBlueprintHelperAttachRule::KeepWorld; return true; }
	if (Text == TEXT("snap_to_target")) { OutRule = EBlueprintHelperAttachRule::SnapToTarget; return true; }
	return false;
}

enum class EBlueprintHelperComponentNameCollisionPolicy : uint8
{
	FailIfExists,
	ReuseIfExists,
	BlockIfClassMismatch
};

inline const TCHAR* NameCollisionPolicyToString(EBlueprintHelperComponentNameCollisionPolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperComponentNameCollisionPolicy::FailIfExists:  return TEXT("fail_if_exists");
	case EBlueprintHelperComponentNameCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists");
	case EBlueprintHelperComponentNameCollisionPolicy::BlockIfClassMismatch: return TEXT("block_if_class_mismatch");
	default:                                                          return TEXT("fail_if_exists");
	}
}

static bool TryParseNameCollisionPolicy(const FString& Text, EBlueprintHelperComponentNameCollisionPolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("fail_if_exists")) { OutPolicy = EBlueprintHelperComponentNameCollisionPolicy::FailIfExists; return true; }
	if (Text == TEXT("reuse_if_exists")) { OutPolicy = EBlueprintHelperComponentNameCollisionPolicy::ReuseIfExists; return true; }
	if (Text == TEXT("block_if_class_mismatch")) { OutPolicy = EBlueprintHelperComponentNameCollisionPolicy::BlockIfClassMismatch; return true; }
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

enum class EBlueprintHelperComponentTransformPolicy : uint8
{
	PreserveWorld,
	PreserveRelative,
	ResetRelative
};

inline const TCHAR* ComponentTransformPolicyToString(EBlueprintHelperComponentTransformPolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperComponentTransformPolicy::PreserveWorld: return TEXT("preserve_world");
	case EBlueprintHelperComponentTransformPolicy::PreserveRelative: return TEXT("preserve_relative");
	case EBlueprintHelperComponentTransformPolicy::ResetRelative: return TEXT("reset_relative");
	default: return TEXT("preserve_relative");
	}
}

static bool TryParseComponentTransformPolicy(const FString& Text, EBlueprintHelperComponentTransformPolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("preserve_relative")) { OutPolicy = EBlueprintHelperComponentTransformPolicy::PreserveRelative; return true; }
	if (Text == TEXT("preserve_world")) { OutPolicy = EBlueprintHelperComponentTransformPolicy::PreserveWorld; return true; }
	if (Text == TEXT("reset_relative")) { OutPolicy = EBlueprintHelperComponentTransformPolicy::ResetRelative; return true; }
	return false;
}

enum class EBlueprintHelperComponentOldRootPolicy : uint8
{
	KeepAsChild,
	RemoveDefaultSceneRootWhenEmpty
};

inline const TCHAR* ComponentOldRootPolicyToString(EBlueprintHelperComponentOldRootPolicy Policy)
{
	return Policy == EBlueprintHelperComponentOldRootPolicy::RemoveDefaultSceneRootWhenEmpty
		? TEXT("remove_default_scene_root_when_empty")
		: TEXT("keep_as_child");
}

static bool TryParseComponentOldRootPolicy(const FString& Text, EBlueprintHelperComponentOldRootPolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("keep_as_child")) { OutPolicy = EBlueprintHelperComponentOldRootPolicy::KeepAsChild; return true; }
	if (Text == TEXT("remove_default_scene_root_when_empty")) { OutPolicy = EBlueprintHelperComponentOldRootPolicy::RemoveDefaultSceneRootWhenEmpty; return true; }
	return false;
}

enum class EBlueprintHelperComponentDefaultRootPolicy : uint8
{
	RequireSceneComponent,
	CreateDefaultSceneRootWhenNeeded
};

inline const TCHAR* ComponentDefaultRootPolicyToString(EBlueprintHelperComponentDefaultRootPolicy Policy)
{
	return Policy == EBlueprintHelperComponentDefaultRootPolicy::CreateDefaultSceneRootWhenNeeded
		? TEXT("create_default_scene_root_when_needed")
		: TEXT("require_scene_component");
}

static bool TryParseComponentDefaultRootPolicy(const FString& Text, EBlueprintHelperComponentDefaultRootPolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("require_scene_component")) { OutPolicy = EBlueprintHelperComponentDefaultRootPolicy::RequireSceneComponent; return true; }
	if (Text == TEXT("create_default_scene_root_when_needed")) { OutPolicy = EBlueprintHelperComponentDefaultRootPolicy::CreateDefaultSceneRootWhenNeeded; return true; }
	return false;
}

enum class EBlueprintHelperComponentDeletePolicy : uint8
{
	BlockIfChildren,
	PromoteChildren,
	DeleteOwnedChildren,
	ReattachChildrenToParent
};

inline const TCHAR* ComponentDeletePolicyToString(EBlueprintHelperComponentDeletePolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperComponentDeletePolicy::BlockIfChildren: return TEXT("block_if_children");
	case EBlueprintHelperComponentDeletePolicy::PromoteChildren: return TEXT("promote_children");
	case EBlueprintHelperComponentDeletePolicy::DeleteOwnedChildren: return TEXT("delete_owned_children");
	case EBlueprintHelperComponentDeletePolicy::ReattachChildrenToParent: return TEXT("reattach_children_to_parent");
	default: return TEXT("block_if_children");
	}
}

static bool TryParseComponentDeletePolicy(const FString& Text, EBlueprintHelperComponentDeletePolicy& OutPolicy)
{
	if (Text.IsEmpty() || Text == TEXT("block_if_children")) { OutPolicy = EBlueprintHelperComponentDeletePolicy::BlockIfChildren; return true; }
	if (Text == TEXT("promote_children")) { OutPolicy = EBlueprintHelperComponentDeletePolicy::PromoteChildren; return true; }
	if (Text == TEXT("delete_owned_children")) { OutPolicy = EBlueprintHelperComponentDeletePolicy::DeleteOwnedChildren; return true; }
	if (Text == TEXT("reattach_children_to_parent")) { OutPolicy = EBlueprintHelperComponentDeletePolicy::ReattachChildrenToParent; return true; }
	return false;
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
	EBlueprintHelperComponentDeletePolicy DeletePolicy = EBlueprintHelperComponentDeletePolicy::BlockIfChildren;
	bool bDryRun = false;
};

struct FBlueprintHelperRenameComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	FString NewComponentName;
	bool bDryRun = false;
};

struct FBlueprintHelperReparentComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	FString NewParentComponent;
	FString SocketName;
	EBlueprintHelperAttachRule AttachRule = EBlueprintHelperAttachRule::KeepRelative;
	EBlueprintHelperComponentTransformPolicy TransformPolicy = EBlueprintHelperComponentTransformPolicy::PreserveRelative;
	bool bDryRun = false;
};

struct FBlueprintHelperAttachComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	FString ParentComponent;
	FString SocketName;
	EBlueprintHelperAttachRule AttachRule = EBlueprintHelperAttachRule::KeepRelative;
	EBlueprintHelperComponentTransformPolicy TransformPolicy = EBlueprintHelperComponentTransformPolicy::PreserveRelative;
	bool bDryRun = false;
};

struct FBlueprintHelperDetachComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	EBlueprintHelperComponentTransformPolicy TransformPolicy = EBlueprintHelperComponentTransformPolicy::PreserveRelative;
	EBlueprintHelperComponentDefaultRootPolicy DefaultRootPolicy = EBlueprintHelperComponentDefaultRootPolicy::RequireSceneComponent;
	bool bDryRun = false;
};

struct FBlueprintHelperSetRootComponentRequest
{
	FString AssetPath;
	FString ComponentName;
	EBlueprintHelperComponentOldRootPolicy OldRootPolicy = EBlueprintHelperComponentOldRootPolicy::KeepAsChild;
	EBlueprintHelperComponentDefaultRootPolicy DefaultRootPolicy = EBlueprintHelperComponentDefaultRootPolicy::RequireSceneComponent;
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

struct FBlueprintHelperChangedComponentProperty
{
	FString PropertyPath;
	FString BeforeValue;
	FString AfterValue;
	FString ExpectedType;
	FString ValueSummary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("property_path"), PropertyPath);
		J->SetStringField(TEXT("before_value"), BeforeValue);
		J->SetStringField(TEXT("after_value"), AfterValue);
		if (!ExpectedType.IsEmpty()) J->SetStringField(TEXT("expected_type"), ExpectedType);
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
	TArray<FBlueprintHelperChangedComponentProperty> ChangedProperties;
	TArray<FBlueprintHelperInvalidComponentPropertySetting> InvalidSettings;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("mode"), PropertyModeToString(Mode));
		J->SetNumberField(TEXT("requested_count"), RequestedCount);
		J->SetNumberField(TEXT("applied_count"), AppliedCount);
		J->SetNumberField(TEXT("changed_count"), ChangedCount);
		J->SetNumberField(TEXT("no_op_count"), NoOpCount);
		TArray<TSharedPtr<FJsonValue>> ChangedArr;
		for (const auto& Changed : ChangedProperties) { ChangedArr.Add(MakeShared<FJsonValueObject>(Changed.ToJson())); }
		J->SetArrayField(TEXT("changed_properties"), ChangedArr);
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
	FString ClassPath;
	FString ComponentTemplatePath;
	FString ComponentId;
	FString ParentComponent;
	TArray<FString> Children;
	FString SocketName;
	TSharedPtr<FJsonObject> RelativeTransform;
	TSharedPtr<FJsonObject> SelectedDefaults;
	FString ReadbackRevision;
	FString ReadbackFingerprint;
	bool bHasReadbackFacts = false;
	bool bIsRoot = false;
	bool bIsDefaultSceneRoot = false;
	bool bIsOwnedSCS = false;
	bool bIsInherited = false;
	bool bIsNative = false;
	bool bCanDelete = false;
	bool bCanRename = false;
	bool bCanReparent = false;
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
		if (Children.Num() > 0 || bHasReadbackFacts)
		{
			TArray<TSharedPtr<FJsonValue>> C;
			for (const FString& Ch : Children) C.Add(MakeShared<FJsonValueString>(Ch));
			J->SetArrayField(TEXT("children"), C);
		}
		if (bHasReadbackFacts)
		{
			J->SetStringField(TEXT("class_path"), ClassPath);
			J->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);
			J->SetStringField(TEXT("component_id"), ComponentId);
			if (ParentComponent.IsEmpty()) J->SetField(TEXT("parent"), MakeShared<FJsonValueNull>());
			else J->SetStringField(TEXT("parent"), ParentComponent);
			if (SocketName.IsEmpty()) J->SetField(TEXT("socket"), MakeShared<FJsonValueNull>());
			else J->SetStringField(TEXT("socket"), SocketName);
			if (SocketName.IsEmpty()) J->SetField(TEXT("socket_name"), MakeShared<FJsonValueNull>());
			else J->SetStringField(TEXT("socket_name"), SocketName);
			J->SetObjectField(TEXT("relative_transform"), RelativeTransform.IsValid() ? RelativeTransform.ToSharedRef() : MakeShared<FJsonObject>());
			J->SetObjectField(TEXT("selected_defaults"), SelectedDefaults.IsValid() ? SelectedDefaults.ToSharedRef() : MakeShared<FJsonObject>());
			J->SetStringField(TEXT("readback_revision"), ReadbackRevision);
			J->SetStringField(TEXT("readback_fingerprint"), ReadbackFingerprint);
			J->SetBoolField(TEXT("is_root"), bIsRoot);
			J->SetBoolField(TEXT("is_default_scene_root"), bIsDefaultSceneRoot);
			J->SetBoolField(TEXT("is_owned_scs"), bIsOwnedSCS);
			J->SetBoolField(TEXT("is_inherited"), bIsInherited);
			J->SetBoolField(TEXT("is_native"), bIsNative);
			J->SetBoolField(TEXT("can_delete"), bCanDelete);
			J->SetBoolField(TEXT("can_rename"), bCanRename);
			J->SetBoolField(TEXT("can_reparent"), bCanReparent);
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
	FString ExistingComponentClass;
	FString ExpectedComponentClass;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("policy"), NameCollisionPolicyToString(Policy));
		J->SetBoolField(TEXT("handled"), bHandled);
		if (!ExistingComponentName.IsEmpty()) J->SetStringField(TEXT("existing_component_name"), ExistingComponentName);
		if (!ExistingComponentClass.IsEmpty()) J->SetStringField(TEXT("existing_component_class"), ExistingComponentClass);
		if (!ExpectedComponentClass.IsEmpty()) J->SetStringField(TEXT("expected_component_class"), ExpectedComponentClass);
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
	FBlueprintHelperToolResultBase RenameComponent(const FBlueprintHelperRenameComponentRequest& Request) const;
	FBlueprintHelperToolResultBase ReparentComponent(const FBlueprintHelperReparentComponentRequest& Request) const;
	FBlueprintHelperToolResultBase AttachComponent(const FBlueprintHelperAttachComponentRequest& Request) const;
	FBlueprintHelperToolResultBase DetachComponent(const FBlueprintHelperDetachComponentRequest& Request) const;
	FBlueprintHelperToolResultBase SetRootComponent(const FBlueprintHelperSetRootComponentRequest& Request) const;
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
