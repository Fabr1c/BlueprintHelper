#pragma once

#include "CoreMinimal.h"
class UEdGraphNode;
class UEdGraphPin;

enum class EBlueprintHelperGraphWriteMutationIntentKind : uint8
{
	AppendSemanticBody,
	InsertSemanticBodyBetweenPins,
	AppendSemanticBodyAfterPin,
	BranchForkSemanticBody,
	SetPinDefault,
	ConnectPins,
	DisconnectPins,
	ReplacePinConnection,
	ReplaceSourcePinConnection,
	Unknown
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWritePinEndpoint
{
	FString NodeRef;
	FString PinRef;
	UEdGraphPin* Pin = nullptr;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteMutationIntent
{
	EBlueprintHelperGraphWriteMutationIntentKind Kind = EBlueprintHelperGraphWriteMutationIntentKind::Unknown;
	FString IntentId;
	FString DefaultValue;
	FBlueprintHelperGraphWritePinEndpoint Source;
	FBlueprintHelperGraphWritePinEndpoint Target;
	FBlueprintHelperGraphWritePinEndpoint ReplacementSource;
	FBlueprintHelperGraphWritePinEndpoint ReplacementTarget;
	UEdGraphNode* InsertedNode = nullptr;
	UEdGraphPin* OriginalSuccessorPin = nullptr;
	TArray<FString> SequenceOrder;
	UEdGraphNode** OutSequenceNode = nullptr;
};
