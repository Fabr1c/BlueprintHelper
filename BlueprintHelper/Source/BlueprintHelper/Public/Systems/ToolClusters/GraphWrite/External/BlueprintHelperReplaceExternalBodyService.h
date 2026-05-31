// BlueprintHelper Service Layer - replace external body service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class FJsonObject;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;

class BLUEPRINTHELPER_API FBlueprintHelperReplaceExternalBodyService
{
public:
	FBlueprintHelperReplaceExternalBodyService(
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperExternalBodySnapshotService& InSnapshotService,
		const FBlueprintHelperExternalDependentsAnalysisService& InDependentsAnalysisService);

	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;

	struct FReplaceExternalBodyRequest
	{
		FString AssetPath;
		FString GraphName;
		EBlueprintHelperReplaceScope Scope = EBlueprintHelperReplaceScope::CustomEventBody;
		FBlueprintHelperExternalGraphAnchor Anchor;
		FString AnchorParseError;
		TSharedPtr<FJsonObject> Body;
		FString ExpectedBodyFingerprint;
		bool bRequireFullDryRun = false;
		bool bDryRun = false;
		FString FeatureName;
	};

	struct FReplaceExternalBodyContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UEdGraphNode* EntryNode = nullptr;
		FBlueprintHelperExternalBodySnapshot BeforeSnapshot;
		FBlueprintHelperExternalDependentsAnalysis DependentsAnalysis;
		TArray<UEdGraphNode*> GeneratedNodes;
		FString ReplacementBlockId;
	};

	struct FReplaceExternalBodyPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

private:
	FReplaceExternalBodyRequest ParseRequest(const TSharedRef<FJsonObject>& Payload) const;
	bool Preflight(
		const FReplaceExternalBodyRequest& Request,
		FReplaceExternalBodyContext& Context,
		FReplaceExternalBodyPreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FReplaceExternalBodyRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FReplaceExternalBodyRequest& Request) const;

	bool ApplyReplacement(
		const FReplaceExternalBodyRequest& Request,
		FReplaceExternalBodyContext& Context,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;

	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperExternalBodySnapshotService& SnapshotService;
	const FBlueprintHelperExternalDependentsAnalysisService& DependentsAnalysisService;
};
