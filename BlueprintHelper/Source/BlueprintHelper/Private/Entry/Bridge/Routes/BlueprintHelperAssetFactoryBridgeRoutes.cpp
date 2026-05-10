// BlueprintHelper Bridge Layer - AssetFactory static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperAssetFactoryBridgeRoutes.h"

#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"

class FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeInvalidRequest(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
	}

	static bool TryReadString(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		FString& OutValue)
	{
		OutValue.Empty();
		if (!Payload.IsValid())
		{
			return !bRequired;
		}
		if (!Payload->TryGetStringField(FieldName, OutValue))
		{
			return !bRequired;
		}
		return true;
	}

	static FBlueprintHelperToolError MakeAssetFactoryError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		bool bRetryable)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = bRetryable;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return Error;
	}

	static void BuildAssetFactoryResult(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperAssetFactoryData& Data,
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType)
	{
		FBlueprintHelperTargetRef Target;
		Target.AssetPath = AssetPath;
		Target.TargetType = EBlueprintHelperTargetType::Asset;
		switch (AssetType)
		{
		case EBlueprintHelperAssetType::BlueprintClass: Target.AssetClass = TEXT("Blueprint"); break;
		case EBlueprintHelperAssetType::BlueprintInterface: Target.AssetClass = TEXT("Blueprint"); break;
		case EBlueprintHelperAssetType::Structure: Target.AssetClass = TEXT("UserDefinedStruct"); break;
		case EBlueprintHelperAssetType::InputAction: Target.AssetClass = TEXT("InputAction"); break;
		case EBlueprintHelperAssetType::InputMappingContext: Target.AssetClass = TEXT("InputMappingContext"); break;
		case EBlueprintHelperAssetType::DataAsset: Target.AssetClass = TEXT("DataAsset"); break;
		case EBlueprintHelperAssetType::DataTable: Target.AssetClass = TEXT("DataTable"); break;
		default: break;
		}
		Result.Target = Target;
		Result.Data = Data.ToJson();

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = FBlueprintHelperAssetFactoryService::ShouldCompile(AssetType);
		Validation.bShouldSave = FBlueprintHelperAssetFactoryService::ShouldSave(AssetType);
		Validation.bCompiled = false;
		Validation.bSaved = false;
		Result.Validation = Validation;
	}

};

FBlueprintHelperAssetFactoryBridgeRoutes::FBlueprintHelperAssetFactoryBridgeRoutes(
	const FBlueprintHelperAssetFactoryService& InAssetFactoryService)
	: AssetFactoryService(InAssetFactoryService)
{
}

bool FBlueprintHelperAssetFactoryBridgeRoutes::IsAssetFactoryCommand(const FString& Command)
{
	return Command == TEXT("create_asset");
}

FBlueprintHelperBridgeResponse FBlueprintHelperAssetFactoryBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (Request.Command != TEXT("create_asset"))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::UnknownCommand,
			FString::Printf(TEXT("Unknown AssetFactory command: %s"), *Request.Command));
	}

	FString AssetPath;
	FString AssetTypeText;
	if (!FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::TryReadString(Request.Payload, TEXT("asset_path"), true, AssetPath) ||
		!FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::TryReadString(Request.Payload, TEXT("asset_type"), true, AssetTypeText))
	{
		return FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::MakeInvalidRequest(Request, TEXT("payload 缺少 asset_path 或 asset_type 字段。"));
	}

	FString ParentClass;
	FString ValueType;
	FString CollisionText;
	FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::TryReadString(Request.Payload, TEXT("parent_class"), false, ParentClass);
	FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::TryReadString(Request.Payload, TEXT("value_type"), false, ValueType);
	FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::TryReadString(Request.Payload, TEXT("collision"), false, CollisionText);

	EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
	if (!FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(AssetTypeText, ParentClass, AssetType))
	{
		return FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::MakeInvalidRequest(
			Request,
			FString::Printf(TEXT("不支持的 asset_type: %s"), *AssetTypeText));
	}

	EBlueprintHelperAssetCollisionPolicy Collision = EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	if (CollisionText.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase))
	{
		Collision = EBlueprintHelperAssetCollisionPolicy::ReuseIfExists;
	}

	const FBlueprintHelperAssetFactoryData FactoryData = AssetFactoryService.CreateAsset(
		AssetPath,
		AssetType,
		ParentClass,
		ValueType,
		Collision);

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	if (FactoryData.Asset.bAlreadyExisted)
	{
		if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists &&
			FactoryData.Collision.bHandled)
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
				TEXT("create_asset"),
				TraceId);
			FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
			FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
			Response.Result = Result.ToJson();
			return Response;
		}

		if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::FailIfExists)
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("create_asset"),
				TraceId,
				FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::MakeAssetFactoryError(
					TEXT("asset_already_exists"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("Target asset already exists."),
					false));
			FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
			FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
			Response.Result = Result.ToJson();
			return Response;
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("create_asset"),
			TraceId,
			FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::MakeAssetFactoryError(
				TEXT("asset_type_mismatch"),
				EBlueprintHelperToolStage::Preflight,
				TEXT("Existing asset type does not match requested asset type."),
				false));
		FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = Result.ToJson();
		return Response;
	}

	if (!FactoryData.Asset.bCreated)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("create_asset"),
			TraceId,
			FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::MakeAssetFactoryError(
				TEXT("creation_failed"),
				EBlueprintHelperToolStage::Execute,
				TEXT("Failed to create asset."),
				false));
		FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = Result.ToJson();
		return Response;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("create_asset"),
		TraceId);
	FBlueprintHelperAssetFactoryBridgeRoutesLocalUtils::BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);

	FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
	Response.Result = Result.ToJson();
	return Response;
}
