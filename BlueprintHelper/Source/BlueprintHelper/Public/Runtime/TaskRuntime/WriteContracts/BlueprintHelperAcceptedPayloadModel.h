#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct BLUEPRINTHELPER_API FBlueprintHelperAcceptedPayloadModel
{
	FString TaskId;
	FString OperationId;
	FString WriteFamily;
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	FString BridgeCommand;
	FString TargetAssetPath;
	FString GraphName;
	FString Mode;
	FString SourcePayloadSchema;
	FString ReviewScopeIdentity;
	FString DebugTraceId;
	TSharedPtr<FJsonObject> RawPayload;
};

class BLUEPRINTHELPER_API FBlueprintHelperAcceptedPayloadModelUtils
{
public:
	static FString MakeReviewScopeIdentity(const FBlueprintHelperAcceptedPayloadModel& Model);
	static FString MakeDebugTraceId(const FBlueprintHelperAcceptedPayloadModel& Model);
	static TSharedRef<FJsonObject> ToJson(const FBlueprintHelperAcceptedPayloadModel& Model);
};
