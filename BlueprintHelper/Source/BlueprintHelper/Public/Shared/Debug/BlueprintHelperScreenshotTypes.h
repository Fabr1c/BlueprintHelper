#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class UEdGraph;
class UEdGraphNode;

enum class EBlueprintHelperScreenshotTarget : uint8
{
	ActiveWindow,
	ActiveViewport,
	GraphPanel
};

inline const TCHAR* BlueprintHelperScreenshotTargetToString(EBlueprintHelperScreenshotTarget Target)
{
	switch (Target)
	{
	case EBlueprintHelperScreenshotTarget::ActiveViewport:
		return TEXT("active_viewport");
	case EBlueprintHelperScreenshotTarget::GraphPanel:
		return TEXT("graph_panel");
	case EBlueprintHelperScreenshotTarget::ActiveWindow:
	default:
		return TEXT("active_window");
	}
}

inline EBlueprintHelperScreenshotTarget BlueprintHelperParseScreenshotTarget(const FString& Value)
{
	if (Value.Equals(TEXT("active_viewport"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperScreenshotTarget::ActiveViewport;
	}
	if (Value.Equals(TEXT("graph_panel"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperScreenshotTarget::GraphPanel;
	}
	return EBlueprintHelperScreenshotTarget::ActiveWindow;
}

struct BLUEPRINTHELPER_API FBlueprintHelperScreenshotCaptureRequest
{
	EBlueprintHelperScreenshotTarget Target = EBlueprintHelperScreenshotTarget::ActiveWindow;
	FString Label;
	int32 TileIndex = INDEX_NONE;
	int32 TileCount = 0;
	int32 NodeCount = 0;
	FString ViewLabel;
};

struct BLUEPRINTHELPER_API FBlueprintHelperScreenshotCaptureResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString Message;
	FString AbsolutePath;
	FString RelativePath;
	int32 Width = 0;
	int32 Height = 0;
	FString Target = TEXT("active_window");
	int32 TileIndex = INDEX_NONE;
	int32 TileCount = 0;
	int32 NodeCount = 0;
	FString ViewLabel;
	FString CreatedAtUtc;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.EditorScreenshotResult.v1"));
		Json->SetBoolField(TEXT("ok"), bSuccess);
		if (!ErrorCode.IsEmpty())
		{
			Json->SetStringField(TEXT("error_code"), ErrorCode);
		}
		if (!Message.IsEmpty())
		{
			Json->SetStringField(TEXT("message"), Message);
		}
		if (!AbsolutePath.IsEmpty())
		{
			Json->SetStringField(TEXT("screenshot_path"), AbsolutePath);
			Json->SetStringField(TEXT("absolute_path"), AbsolutePath);
		}
		if (!RelativePath.IsEmpty())
		{
			Json->SetStringField(TEXT("relative_path"), RelativePath);
		}
		Json->SetNumberField(TEXT("width"), Width);
		Json->SetNumberField(TEXT("height"), Height);
		Json->SetStringField(TEXT("target"), Target);
		if (TileIndex >= 0)
		{
			Json->SetNumberField(TEXT("tile_index"), TileIndex);
		}
		if (TileCount > 0)
		{
			Json->SetNumberField(TEXT("tile_count"), TileCount);
		}
		if (NodeCount > 0)
		{
			Json->SetNumberField(TEXT("node_count"), NodeCount);
		}
		if (!ViewLabel.IsEmpty())
		{
			Json->SetStringField(TEXT("view_label"), ViewLabel);
		}
		if (!CreatedAtUtc.IsEmpty())
		{
			Json->SetStringField(TEXT("created_at_utc"), CreatedAtUtc);
		}
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphScreenshotCaptureRequest
{
	FString Label;
	int32 MaxNodesPerImage = 8;
	UEdGraph* Graph = nullptr;
	TArray<UEdGraphNode*> Nodes;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphScreenshotCaptureResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString Message;
	FString GraphName;
	int32 SelectedNodeCount = 0;
	int32 ScreenshotCount = 0;
	TArray<FBlueprintHelperScreenshotCaptureResult> Screenshots;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.GraphScreenshotResult.v1"));
		Json->SetBoolField(TEXT("ok"), bSuccess);
		if (!ErrorCode.IsEmpty())
		{
			Json->SetStringField(TEXT("error_code"), ErrorCode);
		}
		if (!Message.IsEmpty())
		{
			Json->SetStringField(TEXT("message"), Message);
		}
		if (!GraphName.IsEmpty())
		{
			Json->SetStringField(TEXT("graph_name"), GraphName);
		}
		Json->SetStringField(TEXT("target"), TEXT("graph_panel"));
		Json->SetNumberField(TEXT("selected_node_count"), SelectedNodeCount);
		Json->SetNumberField(TEXT("screenshot_count"), ScreenshotCount);

		TArray<TSharedPtr<FJsonValue>> ScreenshotValues;
		for (const FBlueprintHelperScreenshotCaptureResult& Screenshot : Screenshots)
		{
			ScreenshotValues.Add(MakeShared<FJsonValueObject>(Screenshot.ToJson()));
		}
		Json->SetArrayField(TEXT("screenshots"), ScreenshotValues);
		if (Screenshots.Num() > 0)
		{
			Json->SetObjectField(TEXT("screenshot"), Screenshots[0].ToJson());
		}
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperEditorFocusRequest
{
	FString AssetPath;
	FString GraphName;
	FString BlockRef;
	FString NodeRef;
};

struct BLUEPRINTHELPER_API FBlueprintHelperEditorFocusResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString Message;
	FString AssetPath;
	FString GraphName;
	FString BlockRef;
	FString NodeRef;
	FString FocusedObjectName;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.EditorFocusResult.v1"));
		Json->SetBoolField(TEXT("ok"), bSuccess);
		if (!ErrorCode.IsEmpty())
		{
			Json->SetStringField(TEXT("error_code"), ErrorCode);
		}
		if (!Message.IsEmpty())
		{
			Json->SetStringField(TEXT("message"), Message);
		}
		if (!AssetPath.IsEmpty())
		{
			Json->SetStringField(TEXT("asset_path"), AssetPath);
		}
		if (!GraphName.IsEmpty())
		{
			Json->SetStringField(TEXT("graph_name"), GraphName);
		}
		if (!BlockRef.IsEmpty())
		{
			Json->SetStringField(TEXT("block_ref"), BlockRef);
		}
		if (!NodeRef.IsEmpty())
		{
			Json->SetStringField(TEXT("node_ref"), NodeRef);
		}
		if (!FocusedObjectName.IsEmpty())
		{
			Json->SetStringField(TEXT("focused_object"), FocusedObjectName);
		}
		return Json;
	}
};
