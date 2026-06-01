// BlueprintHelper TaskRuntime main-thread commit service.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperTaskRuntimeClusterHub;
class FJsonObject;

class FBlueprintHelperTaskRuntimeCommitService
{
public:
	FBlueprintHelperTaskRuntimeCommitService(
		const FBlueprintHelperTaskRuntimeClusterHub& InClusterHub,
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService);

	FBlueprintHelperToolResultBase ExecuteStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		bool bDryRun) const;

	FBlueprintHelperToolResultBase CompileAsset(const FString& AssetPath) const;
	FBlueprintHelperToolResultBase SaveAsset(const FString& AssetPath) const;
	FBlueprintHelperToolResultBase MakeSkippedPostOperationResult(
		const FString& Operation,
		const FString& AssetPath,
		const FString& Reason) const;
	bool FlushGraphLayout() const;

private:
	const FBlueprintHelperTaskRuntimeClusterHub& ClusterHub;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
};
