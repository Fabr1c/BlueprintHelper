#pragma once

#include "CoreMinimal.h"

class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperGraphNodeLifecycle
{
public:
	static void NotifyPinConnectionChanged(UEdGraphPin* Pin);
	static void NotifyDataConnectionChanged(UEdGraphPin* FromPin, UEdGraphPin* ToPin);
};
