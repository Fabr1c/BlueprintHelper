#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorFingerprintService
{
public:
	FString BuildNodeFingerprint(const UEdGraphNode* Node) const;
	FString BuildPinFingerprint(const UEdGraphPin* Pin) const;
	FString BuildCompactNodeFingerprint(const UEdGraphNode* Node) const;
	FString BuildCompactPinFingerprint(const UEdGraphPin* Pin) const;
	FString BuildExecBoundaryFingerprint(const UEdGraphPin* SourcePin) const;
	FString BuildLinkFingerprint(
		const UEdGraphPin* SourcePin,
		const UEdGraphPin* TargetPin,
		const FString& LinkKind) const;
};
