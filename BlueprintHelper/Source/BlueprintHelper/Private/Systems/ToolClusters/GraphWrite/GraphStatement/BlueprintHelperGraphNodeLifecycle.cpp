#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeLifecycle.h"

#include "EdGraph/EdGraphPin.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"

void FBlueprintHelperGraphNodeLifecycle::NotifyPinConnectionChanged(UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return;
	}

	if (UK2Node_PromotableOperator* OpNode = Cast<UK2Node_PromotableOperator>(Pin->GetOwningNode()))
	{
		OpNode->NotifyPinConnectionListChanged(Pin);
	}

	if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Pin->GetOwningNode()))
	{
		SelectNode->NotifyPinConnectionListChanged(Pin);
		SelectNode->NodeConnectionListChanged();
	}
}

void FBlueprintHelperGraphNodeLifecycle::NotifyDataConnectionChanged(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
{
	NotifyPinConnectionChanged(FromPin);
	NotifyPinConnectionChanged(ToPin);
}
