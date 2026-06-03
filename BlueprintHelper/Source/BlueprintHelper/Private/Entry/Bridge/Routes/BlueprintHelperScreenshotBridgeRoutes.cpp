#include "Entry/Bridge/Routes/BlueprintHelperScreenshotBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"
#include "Systems/Debug/BlueprintHelperEditorFocusService.h"
#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"
#include "Systems/Debug/BlueprintHelperScreenshotSettings.h"

class FBlueprintHelperScreenshotBridgeRoutesLocalUtils
{
public:
	static FString JsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("missing");
		}
		switch (Value->Type)
		{
		case EJson::String:
			return TEXT("string");
		case EJson::Number:
			return TEXT("number");
		case EJson::Boolean:
			return TEXT("bool");
		case EJson::Array:
			return TEXT("array");
		case EJson::Object:
			return TEXT("object");
		case EJson::Null:
			return TEXT("null");
		default:
			return TEXT("unknown");
		}
	}

	static FBlueprintHelperBridgeResponse InvalidField(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Field,
		const FString& Expected,
		const FString& Actual)
	{
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("%s must be %s; actual type is %s."), *Field, *Expected, *Actual));
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("field"), Field);
		Response.Result->SetStringField(TEXT("expected_type"), Expected);
		Response.Result->SetStringField(TEXT("actual_type"), Actual);
		return Response;
	}

	static bool TryReadString(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		FString& OutValue,
		FString& OutActualType)
	{
		OutValue.Empty();
		OutActualType.Empty();
		if (!Payload.IsValid())
		{
			OutActualType = TEXT("missing");
			return !bRequired;
		}
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Payload, FieldName);
		if (!FoundValue.IsValid())
		{
			OutActualType = TEXT("missing");
			return !bRequired;
		}
		if (!FoundValue->TryGetString(OutValue))
		{
			OutActualType = JsonValueTypeToString(FoundValue);
			return false;
		}
		return true;
	}

	static bool TryReadInt(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		int32& OutValue,
		FString& OutActualType)
	{
		OutValue = 0;
		OutActualType.Empty();
		if (!Payload.IsValid())
		{
			OutActualType = TEXT("missing");
			return !bRequired;
		}
		const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Payload, FieldName);
		if (!FoundValue.IsValid())
		{
			OutActualType = TEXT("missing");
			return !bRequired;
		}
		double NumberValue = 0.0;
		if (!FoundValue->TryGetNumber(NumberValue) ||
			FMath::FloorToDouble(NumberValue) != NumberValue)
		{
			OutActualType = JsonValueTypeToString(FoundValue);
			return false;
		}
		OutValue = static_cast<int32>(NumberValue);
		return true;
	}

	static FBlueprintHelperBridgeResponse MakeSuccessResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const TSharedRef<FJsonObject>& Result)
	{
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = Result;
		return Response;
	}

	static FBlueprintHelperBridgeResponse MakeExecutionFailure(
		const FBlueprintHelperBridgeRequest& Request,
		const TSharedRef<FJsonObject>& Result,
		const FString& Message)
	{
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Message);
		Response.Result = Result;
		return Response;
	}
};

FBlueprintHelperScreenshotBridgeRoutes::FBlueprintHelperScreenshotBridgeRoutes(
	const FBlueprintHelperEditorFocusService& InFocusService,
	const FBlueprintHelperScreenshotCaptureService& InCaptureService)
	: FocusService(InFocusService)
	, CaptureService(InCaptureService)
{
}

bool FBlueprintHelperScreenshotBridgeRoutes::IsScreenshotCommand(const FString& Command)
{
	return Command == TEXT("focus_blueprint_editor_target") ||
		Command == TEXT("capture_editor_screenshot") ||
		Command == TEXT("capture_focused_graph_screenshot");
}

FBlueprintHelperBridgeResponse FBlueprintHelperScreenshotBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (Request.Command == TEXT("focus_blueprint_editor_target"))
	{
		return HandleFocusBlueprintEditorTarget(Request);
	}
	if (Request.Command == TEXT("capture_editor_screenshot"))
	{
		return HandleCaptureEditorScreenshot(Request);
	}
	if (Request.Command == TEXT("capture_focused_graph_screenshot"))
	{
		return HandleCaptureFocusedGraphScreenshot(Request);
	}
	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown screenshot bridge command: %s"), *Request.Command));
}

FBlueprintHelperBridgeResponse FBlueprintHelperScreenshotBridgeRoutes::HandleCaptureFocusedGraphScreenshot(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FBlueprintHelperScreenshotSettings Settings = FBlueprintHelperScreenshotSettings::Load();
	FBlueprintHelperGraphScreenshotCaptureRequest CaptureRequest;
	CaptureRequest.MaxNodesPerImage = Settings.GraphMaxNodesPerImage;
	FBlueprintHelperEditorFocusedGraphSelection FocusedSelection;
	if (FocusService.TryGetLastFocusedGraphSelection(FocusedSelection))
	{
		CaptureRequest.Graph = FocusedSelection.Graph;
		CaptureRequest.Nodes = FocusedSelection.Nodes;
	}

	FString ActualType;
	FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("label"),
		false,
		CaptureRequest.Label,
		ActualType);
	if (!ActualType.IsEmpty() && ActualType != TEXT("missing"))
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::InvalidField(
			Request,
			TEXT("payload.label"),
			TEXT("string"),
			ActualType);
	}

	int32 MaxNodesPerImage = 0;
	if (FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadInt(
		Request.Payload,
		TEXT("max_nodes_per_image"),
		false,
		MaxNodesPerImage,
		ActualType) && MaxNodesPerImage > 0)
	{
		CaptureRequest.MaxNodesPerImage = MaxNodesPerImage;
	}
	if (!ActualType.IsEmpty() && ActualType != TEXT("missing"))
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::InvalidField(
			Request,
			TEXT("payload.max_nodes_per_image"),
			TEXT("integer"),
			ActualType);
	}

	const FBlueprintHelperGraphScreenshotCaptureResult Result = CaptureService.CaptureFocusedGraph(CaptureRequest);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeExecutionFailure(
			Request,
			Result.ToJson(),
			Result.Message);
	}
	return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeSuccessResponse(Request, Result.ToJson());
}

FBlueprintHelperBridgeResponse FBlueprintHelperScreenshotBridgeRoutes::HandleFocusBlueprintEditorTarget(
	const FBlueprintHelperBridgeRequest& Request) const
{
	FBlueprintHelperEditorFocusRequest FocusRequest;
	FString ActualType;
	if (!FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("asset_path"),
		true,
		FocusRequest.AssetPath,
		ActualType))
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::InvalidField(
			Request,
			TEXT("payload.asset_path"),
			TEXT("string"),
			ActualType);
	}
	FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("graph_name"),
		false,
		FocusRequest.GraphName,
		ActualType);
	FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("block_ref"),
		false,
		FocusRequest.BlockRef,
		ActualType);
	FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("node_ref"),
		false,
		FocusRequest.NodeRef,
		ActualType);

	const FBlueprintHelperEditorFocusResult Result =
		FocusService.FocusBlueprintEditorTarget(FocusRequest);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeExecutionFailure(
			Request,
			Result.ToJson(),
			Result.Message);
	}
	return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeSuccessResponse(Request, Result.ToJson());
}

FBlueprintHelperBridgeResponse FBlueprintHelperScreenshotBridgeRoutes::HandleCaptureEditorScreenshot(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FBlueprintHelperScreenshotSettings Settings = FBlueprintHelperScreenshotSettings::Load();
	FBlueprintHelperScreenshotCaptureRequest CaptureRequest;
	CaptureRequest.Target = Settings.DefaultCaptureTarget;

	FString ActualType;
	FString Target;
	if (FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("target"),
		false,
		Target,
		ActualType) && !Target.IsEmpty())
	{
		CaptureRequest.Target = BlueprintHelperParseScreenshotTarget(Target);
	}
	if (!ActualType.IsEmpty() && ActualType != TEXT("missing"))
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::InvalidField(
			Request,
			TEXT("payload.target"),
			TEXT("string"),
			ActualType);
	}
	FBlueprintHelperScreenshotBridgeRoutesLocalUtils::TryReadString(
		Request.Payload,
		TEXT("label"),
		false,
		CaptureRequest.Label,
		ActualType);
	if (!ActualType.IsEmpty() && ActualType != TEXT("missing"))
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::InvalidField(
			Request,
			TEXT("payload.label"),
			TEXT("string"),
			ActualType);
	}

	const FBlueprintHelperScreenshotCaptureResult Result = CaptureService.Capture(CaptureRequest);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeExecutionFailure(
			Request,
			Result.ToJson(),
			Result.Message);
	}
	return FBlueprintHelperScreenshotBridgeRoutesLocalUtils::MakeSuccessResponse(Request, Result.ToJson());
}
