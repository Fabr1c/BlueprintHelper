// BlueprintHelper runtime typed settings resolver.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

class FJsonValue;

class BLUEPRINTHELPER_API FBlueprintHelperRuntimeSettingResolver
{
public:
	static bool GetBool(const FString& DotPath, bool DefaultValue, FString* OutDiagnostics = nullptr);
	static int32 GetInt(const FString& DotPath, int32 DefaultValue, FString* OutDiagnostics = nullptr);
	static double GetDouble(const FString& DotPath, double DefaultValue, FString* OutDiagnostics = nullptr);
	static FString GetString(const FString& DotPath, const FString& DefaultValue, FString* OutDiagnostics = nullptr);
	static FVector2D GetVector2(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics = nullptr);
	static FVector2D GetVector2D(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics = nullptr);
	static FMargin GetMargin(const FString& DotPath, const FMargin& DefaultValue, FString* OutDiagnostics = nullptr);
	static TSharedPtr<FJsonValue> GetJsonValue(const FString& DotPath, FString* OutDiagnostics = nullptr);
};
