// BlueprintHelper Bridge Layer - AssetDiscovery static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h"
#include "Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h"

namespace BlueprintHelperAssetDiscoveryBridgeRoutesLocal
{
	static constexpr const TCHAR* CursorNotSupportedCode = TEXT("cursor_not_supported_in_p0");

	static TSharedRef<FJsonObject> MakeErrorResult(
		const FString& Code,
		const FString& Message,
		const FString& Field = FString())
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("FindAssets.v1"));

		TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), Code);
		Error->SetStringField(TEXT("message"), Message);
		if (!Field.IsEmpty())
		{
			Error->SetStringField(TEXT("field"), Field);
		}
		Json->SetObjectField(TEXT("error"), Error);
		return Json;
	}

	static FBlueprintHelperBridgeResponse MakeInvalidRequest(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Code,
		const FString& Message,
		const FString& Field = FString())
	{
		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
		Response.Result = MakeErrorResult(Code, Message, Field);
		return Response;
	}

	static bool TryReadSchema(
		const TSharedPtr<FJsonObject>& Payload,
		FString& OutSchema)
	{
		OutSchema.Empty();
		return Payload.IsValid() && Payload->TryGetStringField(TEXT("schema"), OutSchema);
	}

	static bool TryReadStringArrayStrict(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		TArray<FString>& OutValues,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		OutValues.Reset();
		OutErrorField.Empty();
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->HasField(FieldName))
		{
			return true;
		}
		if (!Payload->TryGetArrayField(FieldName, Values) || !Values)
		{
			OutErrorField = TEXT("payload.") + FString(FieldName);
			OutErrorMessage = FString::Printf(TEXT("find_assets requires payload.%s array<string>."), FieldName);
			return false;
		}

		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Values)[Index];
			FString Text;
			if (!Value.IsValid() || !Value->TryGetString(Text))
			{
				OutErrorField = FString::Printf(TEXT("payload.%s[%d]"), FieldName, Index);
				OutErrorMessage = FString::Printf(TEXT("find_assets requires payload.%s[%d] string."), FieldName, Index);
				return false;
			}
			OutValues.Add(Text);
		}
		return true;
	}

	static bool IsKnownSemanticType(const FString& AssetType)
	{
		static const TCHAR* const KnownTypes[] = {
			TEXT("blueprint"),
			TEXT("widget_blueprint"),
			TEXT("data_table"),
			TEXT("data_asset"),
			TEXT("user_defined_struct"),
		};
		for (const TCHAR* KnownType : KnownTypes)
		{
			if (AssetType.Equals(KnownType, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	static bool IsFullClassPath(const FString& ClassPath)
	{
		FString ModulePath;
		FString ClassName;
		return ClassPath.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive) &&
			ClassPath.Split(TEXT("."), &ModulePath, &ClassName, ESearchCase::CaseSensitive, ESearchDir::FromEnd) &&
			ModulePath.Len() > FString(TEXT("/Script/")).Len() &&
			!ClassName.IsEmpty() &&
			!ClassName.Contains(TEXT("/"), ESearchCase::CaseSensitive) &&
			!ClassName.Contains(TEXT("."), ESearchCase::CaseSensitive) &&
			!ModulePath.RightChop(FString(TEXT("/Script/")).Len()).Contains(TEXT("."), ESearchCase::CaseSensitive);
	}

	static bool ValidateFindAssetsRequestSemantics(
		const FBlueprintHelperFindAssetsRequest& Request,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		for (int32 Index = 0; Index < Request.PathPrefixes.Num(); ++Index)
		{
			if (!Request.PathPrefixes[Index].StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
			{
				OutErrorField = FString::Printf(TEXT("payload.path_prefixes[%d]"), Index);
				OutErrorMessage = FString::Printf(TEXT("find_assets requires payload.path_prefixes[%d] slash-prefixed path."), Index);
				return false;
			}
		}

		for (int32 Index = 0; Index < Request.AssetTypes.Num(); ++Index)
		{
			if (!IsKnownSemanticType(Request.AssetTypes[Index]))
			{
				OutErrorField = FString::Printf(TEXT("payload.asset_types[%d]"), Index);
				OutErrorMessage = FString::Printf(TEXT("find_assets requires payload.asset_types[%d] known semantic asset type."), Index);
				return false;
			}
		}

		for (int32 Index = 0; Index < Request.AssetClasses.Num(); ++Index)
		{
			if (!IsFullClassPath(Request.AssetClasses[Index]))
			{
				OutErrorField = FString::Printf(TEXT("payload.asset_classes[%d]"), Index);
				OutErrorMessage = FString::Printf(TEXT("find_assets requires payload.asset_classes[%d] full /Script/Module.Class path."), Index);
				return false;
			}
		}

		if (Request.Limit < 1 || Request.Limit > 100)
		{
			OutErrorField = TEXT("payload.limit");
			OutErrorMessage = TEXT("find_assets requires payload.limit integer in range 1..100.");
			return false;
		}

		return true;
	}

	static bool TryReadFindAssetsRequest(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperFindAssetsRequest& OutRequest,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		if (!TryReadSchema(Payload, OutRequest.Schema))
		{
			OutErrorField = TEXT("payload.schema");
			OutErrorMessage = TEXT("find_assets requires payload.schema string.");
			return false;
		}
		if (OutRequest.Schema != TEXT("BlueprintHelper.FindAssetsRequest.v1"))
		{
			OutErrorField = TEXT("payload.schema");
			OutErrorMessage = TEXT("find_assets requires payload.schema literal BlueprintHelper.FindAssetsRequest.v1.");
			return false;
		}

		Payload->TryGetStringField(TEXT("query"), OutRequest.Query);
		if (!TryReadStringArrayStrict(Payload, TEXT("path_prefixes"), OutRequest.PathPrefixes, OutErrorMessage, OutErrorField) ||
			!TryReadStringArrayStrict(Payload, TEXT("asset_types"), OutRequest.AssetTypes, OutErrorMessage, OutErrorField) ||
			!TryReadStringArrayStrict(Payload, TEXT("asset_classes"), OutRequest.AssetClasses, OutErrorMessage, OutErrorField))
		{
			return false;
		}
		Payload->TryGetBoolField(TEXT("recursive"), OutRequest.bRecursive);
		Payload->TryGetNumberField(TEXT("limit"), OutRequest.Limit);
		Payload->TryGetBoolField(TEXT("include_plugin_content"), OutRequest.bIncludePluginContent);
		Payload->TryGetBoolField(TEXT("include_engine_content"), OutRequest.bIncludeEngineContent);
		Payload->TryGetBoolField(TEXT("include_redirectors"), OutRequest.bIncludeRedirectors);
		return ValidateFindAssetsRequestSemantics(OutRequest, OutErrorMessage, OutErrorField);
	}

	static FBlueprintHelperBridgeResponse MakeFindAssetsFailureResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperFindAssetsResult& Result)
	{
		const FString Code = Result.ErrorCode.IsEmpty()
			? FString(TEXT("find_assets_failed"))
			: Result.ErrorCode;
		const FString Message = Result.ErrorMessage.IsEmpty()
			? FString(TEXT("find_assets failed."))
			: Result.ErrorMessage;

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeErrorResult(Code, Message);
		return Response;
	}
}

FBlueprintHelperAssetDiscoveryBridgeRoutes::FBlueprintHelperAssetDiscoveryBridgeRoutes(
	const FBlueprintHelperAssetDiscoveryService& InAssetDiscoveryService)
	: AssetDiscoveryService(InAssetDiscoveryService)
{
}

bool FBlueprintHelperAssetDiscoveryBridgeRoutes::IsAssetDiscoveryCommand(const FString& Command)
{
	return Command == TEXT("find_assets");
}

FBlueprintHelperBridgeResponse FBlueprintHelperAssetDiscoveryBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	using namespace BlueprintHelperAssetDiscoveryBridgeRoutesLocal;

	if (Request.Command != TEXT("find_assets"))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::UnknownCommand,
			FString::Printf(TEXT("Unknown AssetDiscovery command: %s"), *Request.Command));
	}

	if (Request.Payload.IsValid() && Request.Payload->HasField(TEXT("cursor")))
	{
		const FString Message = TEXT("cursor_not_supported_in_p0: find_assets pagination cursors are not supported in P0.");
		return MakeInvalidRequest(Request, CursorNotSupportedCode, Message, TEXT("payload.cursor"));
	}

	FBlueprintHelperFindAssetsRequest ServiceRequest;
	FString ParseErrorMessage;
	FString ParseErrorField;
	if (!TryReadFindAssetsRequest(Request.Payload, ServiceRequest, ParseErrorMessage, ParseErrorField))
	{
		return MakeInvalidRequest(
			Request,
			TEXT("invalid_request"),
			ParseErrorMessage,
			ParseErrorField);
	}

	const FBlueprintHelperFindAssetsResult FindResult = AssetDiscoveryService.FindAssets(ServiceRequest);
	if (!FindResult.bSuccess)
	{
		return MakeFindAssetsFailureResponse(Request, FindResult);
	}

	FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
	Response.Result = FindResult.Data.ToJson();
	return Response;
}
