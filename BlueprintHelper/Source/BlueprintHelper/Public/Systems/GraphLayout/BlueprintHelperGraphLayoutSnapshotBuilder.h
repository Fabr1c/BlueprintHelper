#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

class UEdGraph;
class UEdGraphNode;

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FSnapshotBuilder
{
public:
	static FGraphSnapshot CaptureGraph(const UEdGraph* Graph);
	static FNodeSnapshot CaptureNode(const UEdGraphNode* Node);
};
}
