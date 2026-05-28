#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformSpawnerFactory.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_DynamicCast.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionAdapterUtils.h"

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
		UGraphWriteActionAdapterUtils::CustomizeCastNode,
		TargetClass);
	return Spawner;
}
