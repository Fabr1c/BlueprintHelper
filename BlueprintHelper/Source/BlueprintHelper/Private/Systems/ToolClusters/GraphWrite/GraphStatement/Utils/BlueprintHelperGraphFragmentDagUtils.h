// BlueprintHelper GraphStatement FBlueprintHelperGraphFragmentDagUtils declarations.

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperGraphFragmentDagUtils
{
public:
	static FString NormalizeFragmentId(const FString& FragmentId);
	static const TCHAR* SeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity);
	static const TCHAR* DirectionToString(const EBlueprintHelperGraphFragmentPortDirection Direction);
	static const TCHAR* LayoutKindToString(const EBlueprintHelperGraphFragmentLayoutKind Kind);
	static TSharedRef<FJsonObject> Vector2DToJson(const FVector2D& Value);
	static TArray<TSharedPtr<FJsonValue>> StringMapToJsonArray(const TMap<FString, FString>& Values);
	static TArray<TSharedPtr<FJsonValue>> EndpointsToJsonArray(const TArray<FBlueprintHelperGraphFragmentEndpointRef>& Endpoints);
};
