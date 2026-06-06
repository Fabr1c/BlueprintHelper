#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"

class UEdGraphNode;
struct FBlueprintGenerateResult;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReplacePlan
{
	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphBodySemanticContext SemanticContext;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphConnectivityPolicy ConnectivityPolicy;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReplaceCoordinator
{
public:
	bool BuildPlan(
		const FBlueprintHelperGraphBodyRequest& Request,
		const IBlueprintHelperGraphBodyAdapter& Adapter,
		FBlueprintHelperGraphBodyReplacePlan& OutPlan,
		FString& OutError) const;

	static bool CanAcceptAdapterPlanConnectivityDiagnostics(
		const FBlueprintHelperGraphBodyReplacePlan& ReplacePlan,
		const FBlueprintGenerateResult& GenerateResult,
		const TSet<UEdGraphNode*>& NodesBeforeImport);
};
