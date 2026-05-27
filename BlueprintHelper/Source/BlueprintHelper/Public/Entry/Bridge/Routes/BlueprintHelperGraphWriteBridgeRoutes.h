// BlueprintHelper Bridge Layer - GraphWrite static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteBridgeRoutes
{
public:
	FBlueprintHelperGraphWriteBridgeRoutes(
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
		const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
		const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
		const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService);

	static bool IsGraphWriteCommand(const FString& Command);
	static bool IsGraphWriteReadCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperAppendBlueprintGraphService& AppendGraphService;
	const FBlueprintHelperReplaceBlueprintGraphService& ReplaceGraphService;
	const FBlueprintHelperPatchBlueprintGraphService& PatchGraphService;
	const FBlueprintHelperMergeBlueprintGraphService& MergeGraphService;
};
