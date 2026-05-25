#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformSpawnerFactory.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_DynamicCast.h"

namespace
{
static void CustomizeCastNode(UEdGraphNode* NewNode, bool bIsTemplateNode, UClass* TargetClass)
{
	if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(NewNode))
	{
		CastNode->TargetType = TargetClass;
	}
}
}

UBlueprintNodeSpawner* FBlueprintHelperGenericTransformSpawnerFactory::CreateCastSpawner(
	TSubclassOf<UEdGraphNode> ResolvedNodeClass,
	UClass* TargetClass)
{
	if (!ResolvedNodeClass || !TargetClass)
	{
		return nullptr;
	}

	UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(ResolvedNodeClass);
	if (!Spawner)
	{
		return nullptr;
	}

	Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(
		CustomizeCastNode,
		TargetClass);
	return Spawner;
}
