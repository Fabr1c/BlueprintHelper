#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class FJsonObject;
class UBlueprint;
class UEdGraph;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperPatchExternalLinksService
{
public:
	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;

	struct FPatchExternalLinksRequest
	{
		FString AssetPath;
		FString GraphName;
		FString PatchType;
		FBlueprintHelperExternalCompactAnchor SourceAnchor;
		FBlueprintHelperExternalCompactAnchor TargetAnchor;
		FBlueprintHelperExternalCompactAnchor LinkAnchor;
		FBlueprintHelperExternalCompactAnchor ReplacementAnchor;
		FString SourceAnchorParseError;
		FString TargetAnchorParseError;
		FString LinkAnchorParseError;
		FString ReplacementAnchorParseError;
		bool bDryRun = false;
	};

	struct FPatchExternalLinksContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		UEdGraphPin* ReplacementPin = nullptr;
		FString LinkKind;
	};

	struct FPatchExternalLinksPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

private:
	FPatchExternalLinksRequest ParseRequest(const TSharedRef<FJsonObject>& Payload) const;
	bool Preflight(
		const FPatchExternalLinksRequest& Request,
		FPatchExternalLinksContext& Context,
		FPatchExternalLinksPreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FPatchExternalLinksRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FPatchExternalLinksRequest& Request) const;

	bool ApplyPatch(
		const FPatchExternalLinksRequest& Request,
		const FPatchExternalLinksContext& Context,
		bool& bOutChanged,
		FString& OutError) const;
};
