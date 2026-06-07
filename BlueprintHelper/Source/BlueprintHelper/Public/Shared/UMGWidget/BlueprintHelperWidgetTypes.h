// BlueprintHelper Service Layer — UMG Widget 专属类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ─── 枚举 ───

enum class EBlueprintHelperWidgetScope : uint8 { WidgetTree, WidgetProperty, WidgetSlot };

inline const TCHAR* WidgetScopeToString(EBlueprintHelperWidgetScope S)
{
	switch (S) { case EBlueprintHelperWidgetScope::WidgetTree: return TEXT("widget_tree"); case EBlueprintHelperWidgetScope::WidgetProperty: return TEXT("widget_property"); case EBlueprintHelperWidgetScope::WidgetSlot: return TEXT("widget_slot"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperNameCollisionPolicy : uint8 { FailIfExists, ReuseIfExists };

inline const TCHAR* NameCollisionPolicyToString(EBlueprintHelperNameCollisionPolicy P)
{
	switch (P) { case EBlueprintHelperNameCollisionPolicy::FailIfExists: return TEXT("fail_if_exists"); case EBlueprintHelperNameCollisionPolicy::ReuseIfExists: return TEXT("reuse_if_exists"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperWidgetErrorCode : uint8
{
	InvalidRequest, AssetNotFound, TargetNotWidgetBlueprint, WidgetTreeNotFound,
	ParentWidgetNotFound, WidgetNotFound, WidgetNameAlreadyExists, UnsupportedWidgetClass,
	PropertyNotFound, PropertyNotWritable, TypeMismatch, SlotTypeMismatch,
	WidgetCreateFailed, WidgetInsertFailed, WidgetRemoveFailed, RollbackFailed
};

inline const TCHAR* WidgetErrorCodeToString(EBlueprintHelperWidgetErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperWidgetErrorCode::InvalidRequest:            return TEXT("invalid_request");
	case EBlueprintHelperWidgetErrorCode::AssetNotFound:             return TEXT("asset_not_found");
	case EBlueprintHelperWidgetErrorCode::TargetNotWidgetBlueprint:  return TEXT("target_not_widget_blueprint");
	case EBlueprintHelperWidgetErrorCode::WidgetTreeNotFound:        return TEXT("widget_tree_not_found");
	case EBlueprintHelperWidgetErrorCode::ParentWidgetNotFound:      return TEXT("parent_widget_not_found");
	case EBlueprintHelperWidgetErrorCode::WidgetNotFound:            return TEXT("widget_not_found");
	case EBlueprintHelperWidgetErrorCode::WidgetNameAlreadyExists:   return TEXT("widget_name_already_exists");
	case EBlueprintHelperWidgetErrorCode::UnsupportedWidgetClass:    return TEXT("unsupported_widget_class");
	case EBlueprintHelperWidgetErrorCode::PropertyNotFound:          return TEXT("property_not_found");
	case EBlueprintHelperWidgetErrorCode::PropertyNotWritable:       return TEXT("property_not_writable");
	case EBlueprintHelperWidgetErrorCode::TypeMismatch:              return TEXT("type_mismatch");
	case EBlueprintHelperWidgetErrorCode::SlotTypeMismatch:         return TEXT("slot_type_mismatch");
	case EBlueprintHelperWidgetErrorCode::WidgetCreateFailed:        return TEXT("widget_create_failed");
	case EBlueprintHelperWidgetErrorCode::WidgetInsertFailed:        return TEXT("widget_insert_failed");
	case EBlueprintHelperWidgetErrorCode::WidgetRemoveFailed:        return TEXT("widget_remove_failed");
	case EBlueprintHelperWidgetErrorCode::RollbackFailed:            return TEXT("rollback_failed");
	default:                                                          return TEXT("unknown");
	}
}

// ─── AddWidget ───

struct FBlueprintHelperAddWidgetResult
{
	int32 AddedCount = 0;
	TOptional<bool> bReusedExisting;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("added_count"), AddedCount);
		if (bReusedExisting.IsSet()) J->SetBoolField(TEXT("reused_existing"), *bReusedExisting);
		return J;
	}
};

struct FBlueprintHelperAddWidgetResultData
{
	FString Schema = TEXT("AddWidgetToTree.v1");
	FBlueprintHelperAddWidgetResult AddWidgetResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("add_widget_result"), AddWidgetResult.ToJson());
		return J;
	}
};

// ─── Property Result（普通/Slot 共用） ───

struct FBlueprintHelperPropertyWriteResult
{
	FString Mode; // single | batch
	int32 RequestedCount = 0, AppliedCount = 0, ChangedCount = 0, NoOpCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("mode"), Mode);
		J->SetNumberField(TEXT("requested_count"), RequestedCount);
		J->SetNumberField(TEXT("applied_count"), AppliedCount);
		J->SetNumberField(TEXT("changed_count"), ChangedCount);
		J->SetNumberField(TEXT("no_op_count"), NoOpCount);
		return J;
	}
};

// ─── RemoveWidget ───

struct FBlueprintHelperRemoveWidgetResult
{
	int32 RemovedCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("removed_count"), RemovedCount);
		return J;
	}
};

struct FBlueprintHelperRemoveWidgetResultData
{
	FString Schema = TEXT("RemoveWidgetFromTree.v1");
	FBlueprintHelperRemoveWidgetResult RemoveWidgetResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("remove_widget_result"), RemoveWidgetResult.ToJson());
		return J;
	}
};

// ─── ReadWidgetTree ───

struct FBlueprintHelperWidgetTreeItem
{
	FString WidgetName;
	FString WidgetClass;
	FString WidgetClassPath;
	FString ParentName;
	FString SlotClassPath;
	FString SlotName;
	TSharedPtr<FJsonObject> SlotProperties;
	int32 VirtualIndex = 0;
	bool bIsVariable = false;
	bool bIsInherited = false;
	TArray<FBlueprintHelperWidgetTreeItem> Children;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("widget_name"), WidgetName);
		J->SetStringField(TEXT("widget_class"), WidgetClass);
		J->SetStringField(TEXT("widget_class_path"), WidgetClassPath);
		if (!ParentName.IsEmpty())
		{
			J->SetStringField(TEXT("parent_name"), ParentName);
		}
		if (!SlotClassPath.IsEmpty())
		{
			J->SetStringField(TEXT("slot_class_path"), SlotClassPath);
		}
		if (!SlotName.IsEmpty())
		{
			J->SetStringField(TEXT("slot_name"), SlotName);
		}
		if (SlotProperties.IsValid() && SlotProperties->Values.Num() > 0)
		{
			J->SetObjectField(TEXT("slot_properties"), SlotProperties);
		}
		J->SetNumberField(TEXT("virtual_index"), VirtualIndex);
		J->SetBoolField(TEXT("is_variable"), bIsVariable);
		J->SetBoolField(TEXT("is_inherited"), bIsInherited);

		TArray<TSharedPtr<FJsonValue>> A;
		for (const FBlueprintHelperWidgetTreeItem& Child : Children)
		{
			A.Add(MakeShared<FJsonValueObject>(Child.ToJson()));
		}
		J->SetArrayField(TEXT("children"), A);
		return J;
	}
};

struct FBlueprintHelperNamedSlotEntry
{
	FString HostWidgetName;
	FString SlotName;
	FString ContentWidgetName;
	int32 VirtualIndex = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("host_widget_name"), HostWidgetName);
		J->SetStringField(TEXT("slot_name"), SlotName);
		J->SetStringField(TEXT("content_widget_name"), ContentWidgetName);
		J->SetNumberField(TEXT("virtual_index"), VirtualIndex);
		return J;
	}
};

struct FBlueprintHelperWidgetTreeSummary
{
	FString AssetClass;
	FString ParentClass;
	FBlueprintHelperWidgetTreeItem Root;
	TMap<FString, FBlueprintHelperWidgetTreeItem> Index;
	TArray<FBlueprintHelperNamedSlotEntry> NamedSlots;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		if (!AssetClass.IsEmpty())
		{
			J->SetStringField(TEXT("asset_class"), AssetClass);
		}
		if (!ParentClass.IsEmpty())
		{
			J->SetStringField(TEXT("parent_class"), ParentClass);
		}
		J->SetObjectField(TEXT("root"), Root.ToJson());

		TSharedRef<FJsonObject> IndexJson = MakeShared<FJsonObject>();
		for (const TPair<FString, FBlueprintHelperWidgetTreeItem>& Pair : Index)
		{
			IndexJson->SetObjectField(Pair.Key, Pair.Value.ToJson());
		}
		J->SetObjectField(TEXT("index"), IndexJson);

		TArray<TSharedPtr<FJsonValue>> NamedSlotArray;
		for (const FBlueprintHelperNamedSlotEntry& Entry : NamedSlots)
		{
			NamedSlotArray.Add(MakeShared<FJsonValueObject>(Entry.ToJson()));
		}
		J->SetArrayField(TEXT("named_slots"), NamedSlotArray);
		return J;
	}
};

struct FBlueprintHelperReadWidgetTreeResultData
{
	FString Schema = TEXT("ReadWidgetTree.v1");
	FBlueprintHelperWidgetTreeSummary WidgetTree;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("widget_tree"), WidgetTree.ToJson());
		return J;
	}
};

// ─── ReadProperties ───

struct FBlueprintHelperWidgetProperties
{
	FString WidgetName;
	int32 PropertyCount = 0;
	TSharedPtr<FJsonObject> Values;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("widget_name"), WidgetName);
		J->SetNumberField(TEXT("property_count"), PropertyCount);
		if (Values.IsValid()) J->SetObjectField(TEXT("values"), Values);
		return J;
	}
};

struct FBlueprintHelperReadWidgetPropertiesResultData
{
	FString Schema = TEXT("ReadWidgetProperties.v1");
	FBlueprintHelperWidgetProperties Properties;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("properties"), Properties.ToJson());
		return J;
	}
};

struct FBlueprintHelperWidgetSlotProperties
{
	FString WidgetName, SlotType;
	int32 PropertyCount = 0;
	TSharedPtr<FJsonObject> Values;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("widget_name"), WidgetName);
		J->SetStringField(TEXT("slot_type"), SlotType);
		J->SetNumberField(TEXT("property_count"), PropertyCount);
		if (Values.IsValid()) J->SetObjectField(TEXT("values"), Values);
		return J;
	}
};

struct FBlueprintHelperReadWidgetSlotPropertiesResultData
{
	FString Schema = TEXT("ReadWidgetSlotProperties.v1");
	FBlueprintHelperWidgetSlotProperties SlotProperties;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("slot_properties"), SlotProperties.ToJson());
		return J;
	}
};
