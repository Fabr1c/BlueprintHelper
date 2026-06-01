#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class ESemanticSceneEdgeKind : uint8
{
	Exec,
	Data,
	Collision
};

struct FSemanticSceneNodeDefinition
{
	ENodeRole Role = ENodeRole::Unknown;
	FText Label;
	FText TooltipZh;
	bool bDraggable = true;
};

struct FSemanticSceneEdgeDefinition
{
	ENodeRole FromRole = ENodeRole::Unknown;
	ENodeRole ToRole = ENodeRole::Unknown;
	ESemanticSceneEdgeKind Kind = ESemanticSceneEdgeKind::Data;
	FLinearColor Color = FLinearColor::White;
};

struct FSemanticSceneDefinition
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FText DisplayName;
	TArray<FSemanticSceneNodeDefinition> Nodes;
	TArray<FSemanticSceneEdgeDefinition> Edges;
	TMap<ENodeRole, FVector2D> DefaultRoleCenters;
};

class BLUEPRINTHELPER_API FSemanticSceneCatalog
{
public:
	static TArray<FSemanticSceneDefinition> GetAllScenes();
	static const FSemanticSceneDefinition* FindScene(ESemanticScene Scene);
	static FEditorCanvasSceneState BuildDefaultState(ESemanticScene Scene);
};

class BLUEPRINTHELPER_API FSemanticSceneAdapter
{
public:
	static FEditorCanvasSceneState ResolveSceneState(const FRuleSet& RuleSet, ESemanticScene Scene);
	static void ApplySceneStateToRuleSet(ESemanticScene Scene, const FEditorCanvasSceneState& State, FRuleSet& RuleSet, float Scale);
};
}
