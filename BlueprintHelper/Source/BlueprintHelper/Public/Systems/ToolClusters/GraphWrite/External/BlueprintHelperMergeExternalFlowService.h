// BlueprintHelper Service Layer - Merge external flow service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/GraphWrite/BlueprintHelperMergeGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBoundaryRelationTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class FBlueprintHelperBlockIdService;
class FBlueprintHelperGraphResolver;
class FBlueprintHelperLogicJsonPathService;
class FBlueprintHelperOwnershipService;
class FJsonObject;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_ExecutionSequence;

class BLUEPRINTHELPER_API FBlueprintHelperMergeExternalFlowService
{
public:
	FBlueprintHelperMergeExternalFlowService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperLogicJsonPathService& InPathService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

	struct FMergeExternalFlowRequest
	{
		FString AssetPath;
		FString GraphName;
		EBlueprintHelperInsertStrategy InsertStrategy = EBlueprintHelperInsertStrategy::AppendAfter;
		FBlueprintHelperExternalGraphAnchor Anchor;
		FBlueprintHelperLogicJsonAnchorSelector AnchorSelector;
		FString AnchorParseError;
		FString InsertedBlockId;
		FString FeatureName;
		TSharedPtr<FJsonObject> LogicSpec;
		TArray<FString> SequenceOrder;
		bool bSequenceOrderHadInvalidEntry = false;
		bool bHasAnchorSelector = false;
		bool bDryRun = false;
	};

	struct FMergeExternalFlowContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UEdGraphPin* AnchorPin = nullptr;
		UEdGraphNode* AnchorNode = nullptr;
		TArray<UEdGraphPin*> Successors;
		UEdGraphPin* OriginalSuccessorPin = nullptr;
		TArray<UEdGraphNode*> GeneratedNodes;
		UEdGraphPin* BodyEntryPin = nullptr;
		TArray<UEdGraphPin*> BodyExitPins;
		UK2Node_ExecutionSequence* SequenceNode = nullptr;
		FBlueprintHelperExternalBoundaryRelation Relation;
		TSharedPtr<FJsonObject> FragmentDebugData;
	};

	struct FMergeExternalFlowPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

private:
	FMergeExternalFlowRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FMergeExternalFlowRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FMergeExternalFlowRequest& Request) const;

	bool ResolveTarget(
		const FMergeExternalFlowRequest& Request,
		FMergeExternalFlowContext& Context,
		FMergeExternalFlowPreflightResult& OutResult) const;
	bool ResolveRequestAnchor(
		const FMergeExternalFlowRequest& Request,
		const FMergeExternalFlowContext& Context,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorTarget,
		FString& OutErrorSource,
		bool& bOutConflict) const;
	bool Preflight(
		const FMergeExternalFlowRequest& Request,
		FMergeExternalFlowContext& Context,
		FMergeExternalFlowPreflightResult& OutResult) const;
	bool ApplyExternalMerge(
		const FMergeExternalFlowRequest& Request,
		FMergeExternalFlowContext& Context,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperLogicJsonPathService& PathService;
};
