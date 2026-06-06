#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperServiceTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperDiagnosticProjection
{
	FString Source;
	FString Code;
	FString Message;
	FString Severity;
	FString AssetPath;
	FString GraphName;
	FString TargetKey;
	FString ScopeIdentity;
	TSharedPtr<FJsonObject> Details;
};

class BLUEPRINTHELPER_API FBlueprintHelperDiagnosticProjectionUtils
{
public:
	static FBlueprintHelperDiagnosticProjection FromDiagnosticItem(
		const FBlueprintHelperDiagnosticItem& Item,
		const FString& Source,
		const FString& AssetPath = FString(),
		const FString& ScopeIdentity = FString());
};
