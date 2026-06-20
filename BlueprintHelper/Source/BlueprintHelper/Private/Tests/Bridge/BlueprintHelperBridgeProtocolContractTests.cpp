#if WITH_DEV_AUTOMATION_TESTS

#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

class FBlueprintHelperBridgeProtocolContractTestUtils
{
public:
	static TSharedPtr<FJsonObject> ParseJsonObject(
		FAutomationTestBase& Test,
		const FString& JsonText,
		const TCHAR* Context)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		TSharedPtr<FJsonObject> Root;
		const bool bParsed = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
		Test.TestTrue(FString::Printf(TEXT("%s parses as json object"), Context), bParsed);
		return Root;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeProtocol_ResponseContract,
	"BlueprintHelper.Bridge.Protocol.ResponseContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBridgeProtocol_ResponseContract::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Error(
		TEXT("req_contract"),
		EBlueprintHelperBridgeError::EditorNotReady,
		TEXT("Editor is not ready."));

	const FString JsonText = FBlueprintHelperBridgeProtocol::SerializeResponse(Response);
	const TSharedPtr<FJsonObject> Root =
		FBlueprintHelperBridgeProtocolContractTestUtils::ParseJsonObject(*this, JsonText, TEXT("Serialized response"));
	if (!Root.IsValid())
	{
		return false;
	}

	FString Schema;
	TestTrue(TEXT("response includes top-level schema"), Root->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("response schema matches canonical contract"), Schema, FString(TEXT("BlueprintHelper.BridgeResponse.v1")));

	FString RequestId;
	TestTrue(TEXT("response includes top-level request_id"), Root->TryGetStringField(TEXT("request_id"), RequestId));
	TestEqual(TEXT("response request_id is preserved"), RequestId, FString(TEXT("req_contract")));

	bool bSuccess = true;
	TestTrue(TEXT("response includes top-level success"), Root->TryGetBoolField(TEXT("success"), bSuccess));
	TestFalse(TEXT("response success is false for error response"), bSuccess);

	FString ErrorCode;
	TestTrue(TEXT("response includes top-level error_code"), Root->TryGetStringField(TEXT("error_code"), ErrorCode));
	TestEqual(TEXT("response error_code matches canonical contract"), ErrorCode, FString(TEXT("editor_not_ready")));

	FString Message;
	TestTrue(TEXT("response includes top-level message"), Root->TryGetStringField(TEXT("message"), Message));
	TestEqual(TEXT("response message matches canonical contract"), Message, FString(TEXT("Editor is not ready.")));

	TestFalse(TEXT("response does not emit nested error object"), Root->HasField(TEXT("error")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeProtocol_RequestIdRequired,
	"BlueprintHelper.Bridge.Protocol.RequestIdRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBridgeProtocol_RequestIdRequired::RunTest(const FString& Parameters)
{
	const TOptional<FBlueprintHelperBridgeRequest> MissingRequestId =
		FBlueprintHelperBridgeProtocol::ParseRequest(TEXT("{\"command\":\"get_editor_context\"}"));
	TestFalse(TEXT("request without request_id is rejected"), MissingRequestId.IsSet());

	const TOptional<FBlueprintHelperBridgeRequest> ValidRequest =
		FBlueprintHelperBridgeProtocol::ParseRequest(
			TEXT("{\"request_id\":\"req_present\",\"command\":\"get_editor_context\"}"));
	TestTrue(TEXT("request with request_id parses"), ValidRequest.IsSet());
	if (!ValidRequest.IsSet())
	{
		return false;
	}

	TestEqual(TEXT("parsed request_id is preserved"), ValidRequest->RequestId, FString(TEXT("req_present")));
	TestEqual(TEXT("parsed command is preserved"), ValidRequest->Command, FString(TEXT("get_editor_context")));
	TestNotNull(TEXT("missing payload still materializes empty object"), ValidRequest->Payload.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeProtocol_ResponseRequestIdRequired,
	"BlueprintHelper.Bridge.Protocol.ResponseRequestIdRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBridgeProtocol_ResponseRequestIdRequired::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Error(
		TEXT(""),
		EBlueprintHelperBridgeError::InvalidRequest,
		TEXT("Bridge request JSON parse failed."));

	const FString JsonText = FBlueprintHelperBridgeProtocol::SerializeResponse(Response);
	const TSharedPtr<FJsonObject> Root =
		FBlueprintHelperBridgeProtocolContractTestUtils::ParseJsonObject(*this, JsonText, TEXT("Serialized invalid request response"));
	if (!Root.IsValid())
	{
		return false;
	}

	FString RequestId;
	TestTrue(TEXT("response includes normalized request_id"), Root->TryGetStringField(TEXT("request_id"), RequestId));
	TestEqual(TEXT("empty response request_id is normalized"), RequestId, FString(TEXT("invalid_request")));
	TestFalse(TEXT("normalized response request_id is not empty"), RequestId.IsEmpty());

	FString ErrorCode;
	TestTrue(TEXT("normalized invalid request response includes error_code"), Root->TryGetStringField(TEXT("error_code"), ErrorCode));
	TestEqual(TEXT("normalized invalid request response keeps error code"), ErrorCode, FString(TEXT("invalid_request")));
	return true;
}

#endif
