// GraphWrite ActionResolution 匿名命名空间函数提取 — 统一工具类
// 包含从 4 个 ActionResolution .cpp 文件匿名命名空间中提取的 static 函数

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "UObject/Class.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

#include "GraphWriteActionAdapterUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteActionAdapterUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ===== BlueprintHelperGenericTransformSpawnerFactory.cpp =====
	static void CustomizeCastNode(UEdGraphNode* NewNode, bool bIsTemplateNode, UClass* TargetClass);

	// ===== BlueprintHelperAssetActionProjectionService.cpp =====
	static FBlueprintHelperActionDatabaseProjectionEvidence ToNeutralEvidence(
		const FBlueprintHelperProjectedAssetActionEvidence& Evidence);

	static FBlueprintHelperAssetActionProjectedCandidate ToAssetCandidate(
		const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate);

	// ===== BlueprintHelperActionDatabaseProjectionService.cpp =====
	static FString NormalizeProjectionText(const FString& Value);

	static bool MatchesExactEvidence(const FString& Expected, const FString& Actual);

	static bool MatchesQueryEvidence(
		const FString& Query,
		const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate);

	static bool TryBuildCandidate(
		const FBlueprintActionContext& ActionContext,
		const UObject* ActionOwner,
		UBlueprintNodeSpawner* Spawner,
		FBlueprintHelperActionDatabaseProjectedCandidate& OutCandidate);

	static bool MatchesProjectedEvidence(
		const FBlueprintHelperActionDatabaseProjectionEvidence& Evidence,
		const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate);

	static FBlueprintHelperActionDatabaseProjectionEvidence BuildEffectiveEvidence(
		const FBlueprintHelperActionDatabaseProjectionRequest& Request);

	static FBlueprintHelperActionDatabaseProjectionResult MakeProjectionFailure(
		EBlueprintHelperActionResolutionStatus Status,
		const FString& ErrorCode,
		const FString& Message);

	// ===== BlueprintHelperActionNodeSpawnerAdapter.cpp =====
	static UK2Node* InvokeNodeSpawnerInternal(
		UEdGraph* TargetGraph,
		UBlueprintNodeSpawner* NodeSpawner,
		const FBlueprintHelperActionResolutionResult* ActionResult,
		const FString& StableId,
		const FVector2D& Location,
		const FBlueprintHelperActionNodeSpawnOptions& Options,
		FString& OutError,
		TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics);
};
