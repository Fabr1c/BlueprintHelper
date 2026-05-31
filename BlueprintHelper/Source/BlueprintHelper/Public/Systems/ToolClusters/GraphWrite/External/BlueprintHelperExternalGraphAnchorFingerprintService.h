#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorFingerprintService
{
public:
	FString BuildNodeFingerprint(const UEdGraphNode* Node) const;
	FString BuildPinFingerprint(const UEdGraphPin* Pin) const;
	FString BuildExecBoundaryFingerprint(const UEdGraphPin* SourcePin) const;
};
