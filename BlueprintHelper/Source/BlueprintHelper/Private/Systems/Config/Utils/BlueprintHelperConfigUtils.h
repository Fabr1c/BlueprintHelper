// BlueprintHelper Config utility functions.
// Extracted from anonymous namespaces in Systems/Config/*.cpp into a shared UBlueprintFunctionLibrary.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperRuntimeProfileTypes.h"
#include "BlueprintHelperConfigUtils.generated.h"

class FJsonObject;
class FJsonValue;

/** Internal path segment parsed from a dot-notation setting path (e.g., "foo.bar[0].baz"). */
struct FBlueprintHelperSettingPathSegment
{
	FString FieldName;
	TArray<int32> ArrayIndices;
};

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperConfigUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── Diagnostics helpers (from BlueprintHelperRuntimeSettingResolver) ───
	static void ResetDiagnostics(FString* OutDiagnostics);
	static void SetDiagnostics(FString* OutDiagnostics, const FString& Message);
	static FString MissingDiagnostic(const FString& DotPath, const FString& Error);
	static FString TypeMismatchDiagnostic(const FString& DotPath);
	static bool ResolveValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString* OutDiagnostics);
	static bool TryParseBoolText(const FString& Text, bool& OutValue);
	static bool TryParseIntText(const FString& Text, int32& OutValue);
	static bool TryParseDoubleText(const FString& Text, double& OutValue);
	static bool TryJsonValueToDouble(const TSharedPtr<FJsonValue>& Value, double& OutValue);
	static bool TryJsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, int32 ExpectedCount, TArray<double>& OutValues);
	static bool TryCommaTextToDoubles(const FString& Text, int32 ExpectedCount, TArray<double>& OutValues);

	// ─── Safety profile helpers (from BlueprintHelperSafetyProfileResolver) ───
	static FString BuildActiveProfileSafetyPath(const FString& ActiveProfile);
	static EBlueprintHelperSafetyProfile ParseSafetyProfile(const FString& Profile);

	// ─── Setting path / JSON helpers (from BlueprintHelperSettingStore) ───
	static const TCHAR* GetSettingSchema();
	static const TCHAR* GetSettingVersion();
	static bool SplitDotPath(const FString& DotPath, TArray<FString>& OutParts, FString& OutError);
	static bool ParseJsonPathSegment(const FString& DotPath, const FString& Part, FBlueprintHelperSettingPathSegment& OutSegment, FString& OutError);
	static bool ParseJsonPath(const FString& DotPath, TArray<FBlueprintHelperSettingPathSegment>& OutSegments, FString& OutError);
	static bool ParseJsonObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject, FString& OutError);
	static bool TryGetObjectFieldSafe(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, TSharedPtr<FJsonObject>& OutChild);
	static bool ApplyJsonPathArrayIndices(const FString& DotPath, const FBlueprintHelperSettingPathSegment& Segment, TSharedPtr<FJsonValue>& InOutValue, FString& OutError);
	static bool TryGetValueAtPath(const TSharedPtr<FJsonObject>& RootObject, const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
	static void MergeJsonObjectInto(TSharedPtr<FJsonObject> Target, const TSharedPtr<FJsonObject>& Source);
	static bool MergeJsonFileIfExists(const FString& Path, TSharedPtr<FJsonObject> Target, FString& OutError);
	static FString JsonValueToSettingString(const TSharedPtr<FJsonValue>& Value);
	static TSharedPtr<FJsonValue> TryParseJsonLiteralValue(const FString& NewValue);
	static TSharedPtr<FJsonValue> ConvertSettingStringToJsonValue(const FString& NewValue);
};
