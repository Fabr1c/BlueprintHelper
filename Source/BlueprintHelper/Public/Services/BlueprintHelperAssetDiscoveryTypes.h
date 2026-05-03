// BlueprintHelper Service Layer — Asset Discovery / Editor Navigation 类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 枚举 ───

enum class EBlueprintHelperAssetQueryScope : uint8 { AssetRegistry };
inline const TCHAR* AssetQueryScopeToString(EBlueprintHelperAssetQueryScope S)
{
	switch (S) { case EBlueprintHelperAssetQueryScope::AssetRegistry: return TEXT("asset_registry"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperEditorScope : uint8 { AssetEditor };
inline const TCHAR* EditorScopeToString(EBlueprintHelperEditorScope S)
{
	switch (S) { case EBlueprintHelperEditorScope::AssetEditor: return TEXT("asset_editor"); default: return TEXT("unknown"); }
}

enum class EBlueprintHelperAssetDiscoveryErrorCode : uint8
{
	InvalidRequest, AssetRegistryUnavailable, AssetNotFound, AssetLoadFailed,
	OpenAssetFailed, EditorSubsystemUnavailable, CursorInvalid, InternalError
};
inline const TCHAR* AssetDiscoveryErrorCodeToString(EBlueprintHelperAssetDiscoveryErrorCode C)
{
	switch (C)
	{
	case EBlueprintHelperAssetDiscoveryErrorCode::InvalidRequest:             return TEXT("invalid_request");
	case EBlueprintHelperAssetDiscoveryErrorCode::AssetRegistryUnavailable:   return TEXT("asset_registry_unavailable");
	case EBlueprintHelperAssetDiscoveryErrorCode::AssetNotFound:              return TEXT("asset_not_found");
	case EBlueprintHelperAssetDiscoveryErrorCode::AssetLoadFailed:            return TEXT("asset_load_failed");
	case EBlueprintHelperAssetDiscoveryErrorCode::OpenAssetFailed:            return TEXT("open_asset_failed");
	case EBlueprintHelperAssetDiscoveryErrorCode::EditorSubsystemUnavailable: return TEXT("editor_subsystem_unavailable");
	case EBlueprintHelperAssetDiscoveryErrorCode::CursorInvalid:              return TEXT("cursor_invalid");
	case EBlueprintHelperAssetDiscoveryErrorCode::InternalError:              return TEXT("internal_error");
	default:                                                                    return TEXT("unknown");
	}
}

// ─── FindAssets ───

struct FBlueprintHelperAssetListItem
{
	FString AssetPath, AssetType, AssetClass;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("asset_path"), AssetPath);
		J->SetStringField(TEXT("asset_type"), AssetType);
		J->SetStringField(TEXT("asset_class"), AssetClass);
		return J;
	}
};

struct FBlueprintHelperAssetPageInfo
{
	int32 Limit = 20;
	bool bHasMore = false;
	TOptional<FString> NextCursor;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("limit"), Limit);
		J->SetBoolField(TEXT("has_more"), bHasMore);
		if (NextCursor.IsSet()) J->SetStringField(TEXT("next_cursor"), *NextCursor);
		return J;
	}
};

struct FBlueprintHelperFindAssetsResultData
{
	FString Schema = TEXT("FindAssets.v1");
	TArray<FBlueprintHelperAssetListItem> Assets;
	FBlueprintHelperAssetPageInfo Page;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		TArray<TSharedPtr<FJsonValue>> A; for (const auto& Item : Assets) A.Add(MakeShared<FJsonValueObject>(Item.ToJson()));
		J->SetArrayField(TEXT("assets"), A);
		J->SetObjectField(TEXT("page"), Page.ToJson());
		return J;
	}
};

// ─── ReadAssetSummary ───

struct FBlueprintHelperAssetSummary
{
	FString AssetPath, AssetType, AssetClass;
	bool bLoaded = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("asset_path"), AssetPath);
		J->SetStringField(TEXT("asset_type"), AssetType);
		J->SetStringField(TEXT("asset_class"), AssetClass);
		J->SetBoolField(TEXT("loaded"), bLoaded);
		return J;
	}
};

struct FBlueprintHelperReadAssetSummaryResultData
{
	FString Schema = TEXT("ReadAssetSummary.v1");
	FBlueprintHelperAssetSummary Asset;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("asset"), Asset.ToJson());
		return J;
	}
};

// ─── OpenAssetInEditor ───

struct FBlueprintHelperOpenAssetResult
{
	bool bOpened = false, bAlreadyOpen = false;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("opened"), bOpened);
		J->SetBoolField(TEXT("already_open"), bAlreadyOpen);
		return J;
	}
};

struct FBlueprintHelperOpenAssetInEditorResultData
{
	FString Schema = TEXT("OpenAssetInEditor.v1");
	FBlueprintHelperOpenAssetResult OpenResult;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("open_result"), OpenResult.ToJson());
		return J;
	}
};

// ─── GetEditorContext ───

struct FBlueprintHelperEditorContextResult
{
	TArray<FString> OpenAssets;
	TArray<FString> SelectedAssets;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		auto StrArr = [](const TCHAR* Key, const TArray<FString>& Arr, TSharedRef<FJsonObject> J)
		{ TArray<TSharedPtr<FJsonValue>> A; for (const auto& S : Arr) A.Add(MakeShared<FJsonValueString>(S)); J->SetArrayField(Key, A); };
		StrArr(TEXT("open_assets"), OpenAssets, J);
		StrArr(TEXT("selected_assets"), SelectedAssets, J);
		return J;
	}
};

struct FBlueprintHelperGetEditorContextResultData
{
	FString Schema = TEXT("GetEditorContext.v1");
	FBlueprintHelperEditorContextResult EditorContext;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("schema"), Schema);
		J->SetObjectField(TEXT("editor_context"), EditorContext.ToJson());
		return J;
	}
};
