#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

struct FParsedMacroReference
{
	FString LibraryType;
	FString MacroName;
	FString MacroAssetPath;
};

struct FParsedLink
{
	FString FromId;
	FString FromPin;
	FString ToId;
	FString ToPin;
};

struct FParsedNode
{
	FString Id;
	EParsedBlueprintNodeType NodeType = EParsedBlueprintNodeType::Unknown;
	FString SourceType;
	FString FunctionName;
	FString ResolvedCallFunctionStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectName;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	float X = 0.0f;
	float Y = 0.0f;
	TMap<FString, FString> DefaultValues;
	FParsedVariableReference VariableReference;
	FParsedMacroReference MacroReference;
	FParsedEventReference EventReference;
	FParsedDelegateReference DelegateReference;
	FParsedContainerReference ContainerReference;
	FParsedStructReference StructReference;
	FParsedCastReference CastReference;
	FParsedSpawnReference SpawnReference;
	FParsedFormatTextReference FormatTextReference;
	FParsedLiteralReference LiteralReference;
	FParsedComponentBoundEventReference ComponentBoundEventReference;
	FParsedCommentReference CommentReference;
	FParsedEnhancedInputActionReference EnhancedInputActionReference;
	FParsedSwitchReference SwitchReference;
	FParsedSelectReference SelectReference;
};
