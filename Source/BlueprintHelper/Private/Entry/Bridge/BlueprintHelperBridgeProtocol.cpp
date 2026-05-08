// BlueprintHelper Bridge Layer — 协议序列化/反序列化实现

#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TOptional<FBlueprintHelperBridgeRequest> FBlueprintHelperBridgeProtocol::ParseRequest(const FString& JsonText)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return {};
	}

	FBlueprintHelperBridgeRequest Req;
	Root->TryGetStringField(TEXT("request_id"), Req.RequestId);
	if (!Root->TryGetStringField(TEXT("command"), Req.Command) || Req.Command.IsEmpty())
	{
		return {};
	}
	Root->TryGetStringField(TEXT("auth_token"), Req.AuthToken);

	if (Root->HasField(TEXT("payload")))
	{
		const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("payload"), PayloadObject) || !PayloadObject || !PayloadObject->IsValid())
		{
			return {};
		}
		Req.Payload = *PayloadObject;
	}
	else
	{
		Req.Payload = MakeShared<FJsonObject>();
	}

	return Req;
}

FString FBlueprintHelperBridgeProtocol::SerializeResponse(const FBlueprintHelperBridgeResponse& Response)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("request_id"), Response.RequestId);
	Root->SetBoolField(TEXT("success"), Response.bSuccess);

	if (!Response.bSuccess)
	{
		TSharedRef<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetStringField(TEXT("code"), BridgeErrorToString(Response.ErrorCode));
		ErrorObj->SetStringField(TEXT("message"), Response.Message);
		Root->SetObjectField(TEXT("error"), ErrorObj);
	}
	else if (!Response.Message.IsEmpty())
	{
		Root->SetStringField(TEXT("message"), Response.Message);
	}

	if (Response.Result.IsValid())
	{
		Root->SetObjectField(TEXT("result"), Response.Result.ToSharedRef());
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);
	return OutputString;
}

TSharedPtr<FJsonObject> FBlueprintHelperBridgeProtocol::ContextToJson(const FBlueprintHelperEditorContext& Ctx)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("active_blueprint_path"), Ctx.ActiveBlueprintPath);
	Obj->SetStringField(TEXT("active_graph_name"), Ctx.ActiveGraphName);
	Obj->SetStringField(TEXT("blueprint_display_name"), Ctx.BlueprintDisplayName);
	Obj->SetNumberField(TEXT("node_count"), Ctx.NodeCount);
	Obj->SetBoolField(TEXT("is_compiled"), Ctx.bIsCompiled);
	Obj->SetNumberField(TEXT("blueprint_status"), Ctx.BlueprintStatus);
	return Obj;
}
