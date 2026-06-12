// BlueprintHelper Service Layer - Patch external graph service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalNodePropertyDescriptorRegistry.h"

class FJsonObject;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperPatchExternalGraphService
{
public:
	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;

	struct FPatchExternalGraphRequest
	{
		FString AssetPath;
		FString GraphName;
		FString PatchType;
		FBlueprintHelperExternalGraphAnchor Anchor;
		FBlueprintHelperExternalCompactAnchor CompactAnchor;
		FString AnchorParseError;
		FString PropertyDescriptorId;
		FString Value;
		FString ExpectedOldValue;
		bool bExpectedOldStateProvided = false;
		bool bHasExpandedAnchor = false;
		bool bHasCompactAnchor = false;
		bool bDryRun = false;
	};

	struct FPatchExternalGraphContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UEdGraphNode* Node = nullptr;
		UEdGraphPin* Pin = nullptr;
		FBlueprintHelperExternalNodePropertyDescriptor PropertyDescriptor;
		bool bHasPropertyDescriptor = false;
		FString BeforeValue;
	};

	struct FPatchExternalGraphPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

private:
	FPatchExternalGraphRequest ParseRequest(const TSharedRef<FJsonObject>& Payload) const;
	bool Preflight(
		const FPatchExternalGraphRequest& Request,
		FPatchExternalGraphContext& Context,
		FPatchExternalGraphPreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FPatchExternalGraphRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FPatchExternalGraphRequest& Request) const;

	bool ApplyPatch(
		const FPatchExternalGraphRequest& Request,
		const FPatchExternalGraphContext& Context,
		bool& bOutChanged,
		FString& OutError) const;
};
