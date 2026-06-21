// BlueprintHelper Service Layer �?Blueprint Class Settings 类型定义
// �?6 簇：蓝图 Class Settings 读写工具的数据类�?

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassDefaultMutationTypes.h"

// ─── 操作模式 ───

/** Class Settings 操作模式�?*/
enum class EBlueprintHelperClassSettingsOperationMode : uint8
{
	Single,
	Batch
};

inline const TCHAR* ClassSettingsModeToString(EBlueprintHelperClassSettingsOperationMode Mode)
{
	switch (Mode)
	{
	case EBlueprintHelperClassSettingsOperationMode::Single: return TEXT("single");
	case EBlueprintHelperClassSettingsOperationMode::Batch:  return TEXT("batch");
	default: return TEXT("unknown");
	}
}

// ─── read_class_settings 返回数据 ───

/** read_class_settings 返回�?Class Settings 摘要�?*/
struct FBlueprintHelperClassSettingsSummary
{
	FString ParentClass;
	FString GeneratedClass;
	TArray<FString> ImplementedInterfaces;
	int32 ClassDefaultCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("parent_class"), ParentClass);
		Json->SetStringField(TEXT("generated_class"), GeneratedClass);

		TArray<TSharedPtr<FJsonValue>> Interfaces;
		for (const FString& Path : ImplementedInterfaces)
		{
			Interfaces.Add(MakeShared<FJsonValueString>(Path));
		}
		Json->SetArrayField(TEXT("implemented_interfaces"), Interfaces);
		Json->SetNumberField(TEXT("class_default_count"), ClassDefaultCount);
		return Json;
	}
};

struct FBlueprintHelperClassDefaultPropertyContext
{
	FString AssetPath;
	FString PropertyPath;
	FString ClassName;
	FString TypeName;
	FString Value;
	FString Category;
	FString Flags;
	FString OwnerObjectPath;
	FString OwnerObjectClass;
	TArray<FString> DirectWriteBlockedBy;
	bool bDirectWriteWritable = false;
	bool bSetterAwareWriteSupported = false;
	FString SetterFunction;
	FString GetterFunction;
	FString SetterAwareRouteId;
	FString SetterAwareTargetKind;
	bool bFound = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintClassDefaultPropertyContext.v1"));
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("target_type"), TEXT("property"));
		Json->SetStringField(TEXT("property_path"), PropertyPath);
		Json->SetBoolField(TEXT("found"), bFound);
		Json->SetStringField(TEXT("owner_root"), TEXT("blueprint_cdo"));
		if (!ClassName.IsEmpty()) Json->SetStringField(TEXT("class_name"), ClassName);
		if (!TypeName.IsEmpty()) Json->SetStringField(TEXT("type"), TypeName);
		if (!Value.IsEmpty()) Json->SetStringField(TEXT("value"), Value);
		if (!Category.IsEmpty()) Json->SetStringField(TEXT("category"), Category);
		if (!Flags.IsEmpty()) Json->SetStringField(TEXT("flags"), Flags);
		if (!OwnerObjectPath.IsEmpty()) Json->SetStringField(TEXT("owner_object_path"), OwnerObjectPath);
		if (!OwnerObjectClass.IsEmpty()) Json->SetStringField(TEXT("owner_object_class"), OwnerObjectClass);

		TSharedRef<FJsonObject> DirectWrite = MakeShared<FJsonObject>();
		DirectWrite->SetBoolField(TEXT("writable"), bDirectWriteWritable);
		if (DirectWriteBlockedBy.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> BlockedBy;
			for (const FString& Reason : DirectWriteBlockedBy)
			{
				BlockedBy.Add(MakeShared<FJsonValueString>(Reason));
			}
			DirectWrite->SetArrayField(TEXT("blocked_by"), BlockedBy);
		}
		Json->SetObjectField(TEXT("direct_write"), DirectWrite);

		if (bSetterAwareWriteSupported || !SetterFunction.IsEmpty() || !GetterFunction.IsEmpty())
		{
			TSharedRef<FJsonObject> SetterAwareWrite = MakeShared<FJsonObject>();
			SetterAwareWrite->SetBoolField(TEXT("supported"), bSetterAwareWriteSupported);
			if (!SetterFunction.IsEmpty()) SetterAwareWrite->SetStringField(TEXT("setter_function"), SetterFunction);
			if (!GetterFunction.IsEmpty()) SetterAwareWrite->SetStringField(TEXT("getter_function"), GetterFunction);
			if (!SetterAwareRouteId.IsEmpty()) SetterAwareWrite->SetStringField(TEXT("route_id"), SetterAwareRouteId);
			if (!SetterAwareTargetKind.IsEmpty()) SetterAwareWrite->SetStringField(TEXT("target_kind"), SetterAwareTargetKind);
			Json->SetObjectField(TEXT("setter_aware_write"), SetterAwareWrite);
		}
		return Json;
	}
};

// ─── Interface 操作返回数据 ───

/** Interface 操作中的无效接口信息�?*/
struct FBlueprintHelperInvalidInterface
{
	FString InterfacePath;
	FString Code;
	FString Message;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("interface_path"), InterfacePath);
		Json->SetStringField(TEXT("code"), Code);
		if (!Message.IsEmpty()) { Json->SetStringField(TEXT("message"), Message.Left(128)); }
		return Json;
	}
};

/** Interface 添加 / 移除操作的结果�?*/
struct FBlueprintHelperInterfaceResult
{
	EBlueprintHelperClassSettingsOperationMode Mode = EBlueprintHelperClassSettingsOperationMode::Single;
	int32 RequestedCount = 0;
	int32 AppliedCount = 0;
	int32 AlreadyImplementedCount = 0;
	int32 RemovedCount = 0;
	TArray<FBlueprintHelperInvalidInterface> InvalidInterfaces;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("mode"), ClassSettingsModeToString(Mode));
		Json->SetNumberField(TEXT("requested_count"), RequestedCount);
		Json->SetNumberField(TEXT("applied_count"), AppliedCount);
		Json->SetNumberField(TEXT("already_implemented_count"), AlreadyImplementedCount);
		Json->SetNumberField(TEXT("removed_count"), RemovedCount);

		TArray<TSharedPtr<FJsonValue>> InvalidArray;
		for (const FBlueprintHelperInvalidInterface& Invalid : InvalidInterfaces)
		{
			InvalidArray.Add(MakeShared<FJsonValueObject>(Invalid.ToJson()));
		}
		Json->SetArrayField(TEXT("invalid_interfaces"), InvalidArray);
		return Json;
	}
};

// ─── Class Default 属性操作返回数�?───

/** Class Default 属性写入失败时的无效设置信息�?*/
struct FBlueprintHelperInvalidClassDefaultSetting
{
	FString PropertyPath;
	FString Code;
	FString ExpectedType;
	FString ActualType;
	FString ValueSummary;
	FString SafeNextAction;
	TOptional<FBlueprintHelperToolSuggestedRoute> SuggestedRoute;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("property_path"), PropertyPath);
		Json->SetStringField(TEXT("code"), Code);
		if (!ExpectedType.IsEmpty()) { Json->SetStringField(TEXT("expected_type"), ExpectedType); }
		if (!ActualType.IsEmpty()) { Json->SetStringField(TEXT("actual_type"), ActualType); }
		if (!ValueSummary.IsEmpty()) { Json->SetStringField(TEXT("value_summary"), ValueSummary.Left(128)); }
		if (!SafeNextAction.IsEmpty()) { Json->SetStringField(TEXT("safe_next_action"), SafeNextAction); }
		if (SuggestedRoute.IsSet()) { Json->SetObjectField(TEXT("suggested_route"), SuggestedRoute->ToJson()); }
		return Json;
	}
};

/** 单个 Class Default 属性设置项�?*/
struct FBlueprintHelperClassDefaultPropertySetting
{
	FString PropertyPath;
	TSharedPtr<FJsonValue> Value;
	FString MutationStrategy;
};

/** Class Default 属性设置操作的结果�?*/
struct FBlueprintHelperDefaultPropertyResult
{
	EBlueprintHelperClassSettingsOperationMode Mode = EBlueprintHelperClassSettingsOperationMode::Single;
	int32 RequestedCount = 0;
	int32 AppliedCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
	TArray<FBlueprintHelperInvalidClassDefaultSetting> InvalidSettings;
	TArray<FBlueprintHelperClassDefaultSetterMutationEvidence> SetterMutationEvidence;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("mode"), ClassSettingsModeToString(Mode));
		Json->SetNumberField(TEXT("requested_count"), RequestedCount);
		Json->SetNumberField(TEXT("applied_count"), AppliedCount);
		Json->SetNumberField(TEXT("changed_count"), ChangedCount);
		Json->SetNumberField(TEXT("no_op_count"), NoOpCount);

		TArray<TSharedPtr<FJsonValue>> InvalidArray;
		for (const FBlueprintHelperInvalidClassDefaultSetting& Invalid : InvalidSettings)
		{
			InvalidArray.Add(MakeShared<FJsonValueObject>(Invalid.ToJson()));
		}
		Json->SetArrayField(TEXT("invalid_settings"), InvalidArray);

		if (SetterMutationEvidence.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> EvidenceArray;
			for (const FBlueprintHelperClassDefaultSetterMutationEvidence& Evidence : SetterMutationEvidence)
			{
				EvidenceArray.Add(MakeShared<FJsonValueObject>(Evidence.ToJson()));
			}
			Json->SetArrayField(TEXT("setter_mutation_evidence"), EvidenceArray);
		}
		return Json;
	}
};

/** Blueprint reparent operation result. */
struct FBlueprintHelperReparentResult
{
	FString PreviousParentClass;
	FString NewParentClass;
	bool bAlreadyParented = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("previous_parent_class"), PreviousParentClass);
		Json->SetStringField(TEXT("new_parent_class"), NewParentClass);
		Json->SetBoolField(TEXT("already_parented"), bAlreadyParented);
		return Json;
	}
};
