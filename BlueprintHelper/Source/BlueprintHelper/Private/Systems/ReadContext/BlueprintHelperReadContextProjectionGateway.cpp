#include "Systems/ReadContext/BlueprintHelperReadContextProjectionGateway.h"

#include "Dom/JsonObject.h"

class FBlueprintHelperReadContextProjectionGatewayState
{
public:
	static TSharedPtr<IBlueprintHelperReadContextProjectionBackend>& Backend()
	{
		static TSharedPtr<IBlueprintHelperReadContextProjectionBackend> Instance;
		return Instance;
	}
};

void FBlueprintHelperReadContextProjectionGateway::SetBackend(
	TSharedPtr<IBlueprintHelperReadContextProjectionBackend> InBackend)
{
	FBlueprintHelperReadContextProjectionGatewayState::Backend() = MoveTemp(InBackend);
}

void FBlueprintHelperReadContextProjectionGateway::ClearBackend()
{
	FBlueprintHelperReadContextProjectionGatewayState::Backend().Reset();
}

bool FBlueprintHelperReadContextProjectionGateway::Project(
	const TSharedRef<FJsonObject>& RawLogicJson,
	const FString& RequestedFormat,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload.Reset();
	OutError = FBlueprintHelperToolError();

	const TSharedPtr<IBlueprintHelperReadContextProjectionBackend>& Backend =
		FBlueprintHelperReadContextProjectionGatewayState::Backend();
	if (!Backend.IsValid())
	{
		OutError = MakeBackendUnavailableError(RequestedFormat);
		return false;
	}

	return Backend->Project(RawLogicJson, RequestedFormat, OutPayload, OutError) && OutPayload.IsValid();
}

FBlueprintHelperToolError FBlueprintHelperReadContextProjectionGateway::MakeBackendUnavailableError(
	const FString& RequestedFormat)
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("canonical_projection_backend_unavailable");
	Error.Stage = EBlueprintHelperToolStage::Execute;
	Error.Message = TEXT("TaskSpecWorkbench export requires the canonical ReadContext projection backend.");
	Error.Field = TEXT("format");
	Error.Expected = TEXT("logic_flow|logic_json");
	Error.Actual = RequestedFormat;
	return Error;
}
