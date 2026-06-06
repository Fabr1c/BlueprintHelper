#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"

class UBlueprint;
class UEdGraph;
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

	static EBlueprintHelperGraphBodyKind BodyKindForReplaceScope(EBlueprintHelperReplaceScope Scope);
	static FString RuntimeAdapterIdForReplaceScope(EBlueprintHelperReplaceScope Scope);
	static bool IsEntryReconnectScope(EBlueprintHelperReplaceScope Scope);
	static bool IsWholeGraphBodyReplacementScope(EBlueprintHelperReplaceScope Scope);
	static bool UsesMemberGraphTarget(EBlueprintHelperReplaceScope Scope);
	static UEdGraph* ResolveGraphForReplaceScope(
		UBlueprint* Blueprint,
		const FString& GraphName,
		EBlueprintHelperReplaceScope Scope,
		FString& OutErrorCode,
		FString& OutErrorMessage);
	static UEdGraph* ResolveSemanticContextGraph(
		UBlueprint* Blueprint,
		const FString& GraphName,
		EBlueprintHelperReplaceScope Scope);
	static bool CanAcceptBoundaryConnectivityDiagnostics(
		EBlueprintHelperReplaceScope Scope,
		const FBlueprintGenerateResult& GenerateResult,
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport);
};
