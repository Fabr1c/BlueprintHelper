#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

enum class EBlueprintHelperTaskRuntimeCluster : uint8;

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeClusterFamilyDescriptor
{
	EBlueprintHelperTaskRuntimeCluster Cluster;
	FString ClusterFamily;
	FString WriteFamily;
	bool (*CanExecuteStep)(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) = nullptr;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeClusterFamilyRegistry
{
public:
	static const TArray<FBlueprintHelperTaskRuntimeClusterFamilyDescriptor>& GetKnownDescriptors();
	static TArray<EBlueprintHelperTaskRuntimeCluster> GetRegisteredClusters();
	static bool TryFindByCluster(
		EBlueprintHelperTaskRuntimeCluster Cluster,
		FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& OutDescriptor);
	static bool TryFindByWriteFamily(
		const FString& WriteFamily,
		FBlueprintHelperTaskRuntimeClusterFamilyDescriptor& OutDescriptor);
	static EBlueprintHelperTaskRuntimeCluster ResolveClusterForLoweredStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);
};
