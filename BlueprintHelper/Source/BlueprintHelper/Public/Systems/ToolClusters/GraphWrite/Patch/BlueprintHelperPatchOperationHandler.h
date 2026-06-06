#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

struct BLUEPRINTHELPER_API FBlueprintHelperPatchOperationApplyContext
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	UEdGraphNode* TargetNode = nullptr;
	UEdGraphPin* TargetPin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* ReplacementPin = nullptr;
	FBlueprintHelperResolvedLink Link;
	bool bDeleteBreakLinks = true;
	TSharedPtr<FJsonObject> PatchJson;
	TFunction<bool(const FBlueprintHelperGraphWriteMutationIntent& Intent, bool& bOutChanged, FString& OutError)> ExecuteMutationIntent;
};

class BLUEPRINTHELPER_API IBlueprintHelperPatchOperationHandler
{
public:
	virtual ~IBlueprintHelperPatchOperationHandler() = default;
	virtual FString GetPatchKind() const = 0;
	virtual bool ValidateRequest(
		const TSharedRef<FJsonObject>& PatchJson,
		FBlueprintHelperToolError& OutError) const = 0;
	virtual bool BuildMutationIntent(
		const TSharedRef<FJsonObject>& PatchJson,
		FBlueprintHelperGraphWriteMutationIntent& OutIntent,
		FBlueprintHelperToolError& OutError) const = 0;
	virtual bool ApplyResolvedPatch(
		const FBlueprintHelperPatchOperationApplyContext& Context,
		bool& bOutChanged,
		FString& OutError) const = 0;
};
