#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphJsonParser
{
public:
	static EParsedBlueprintNodeType ResolveNodeType(const TSharedPtr<FJsonObject>& NodeObject);
	static FString ConvertJsonValueToString(const TSharedPtr<FJsonValue>& JsonValue);
	static FString ResolveNodeFunctionName(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedPinType ResolvePinType(const TSharedPtr<FJsonObject>& PinTypeObject);
	static FParsedVariableReference ResolveVariableReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedMacroReference ResolveMacroReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedEventReference ResolveEventReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedDelegateReference ResolveDelegateReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedContainerReference ResolveContainerReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedStructReference ResolveStructReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedCastReference ResolveCastReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedSpawnReference ResolveSpawnReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedFormatTextReference ResolveFormatTextReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedTimelineReference ResolveTimelineReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedLiteralReference ResolveLiteralReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedComponentBoundEventReference ResolveComponentBoundEventReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedCommentReference ResolveCommentReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedEnhancedInputActionReference ResolveEnhancedInputActionReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedSwitchReference ResolveSwitchReference(const TSharedPtr<FJsonObject>& NodeObject);
	static FParsedSelectReference ResolveSelectReference(const TSharedPtr<FJsonObject>& NodeObject);
	static void ResolveLocalVariableDeclarations(const TSharedPtr<FJsonObject>& JsonObject, TArray<FParsedLocalVariableDeclaration>& OutDeclarations);
};
