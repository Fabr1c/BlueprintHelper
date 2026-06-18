#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

enum class EBlueprintHelperBridgeCommandSource : uint8
{
	SystemCore,
	SystemDebug,
	SystemReadHelper,
	SystemAssetBrowser,
	SystemAssetDiscovery,
	SystemEditorCommand,
	SystemTaskRuntime,
	SystemReview,
	SystemInternalDirectRoute,
	SystemInternal
};

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeSystemCommandDescriptor
{
	FString Command;
	EBlueprintHelperBridgeRouteCluster Cluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	EBlueprintHelperBridgeCommandSource Source = EBlueprintHelperBridgeCommandSource::SystemInternal;
	const TCHAR* PolicyId = TEXT("system.non_agent_visible");
	bool bRequiresGameThread = true;
	bool bAgentVisible = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperBridgeSystemCommandRegistry
{
public:
	static TArray<FBlueprintHelperBridgeSystemCommandDescriptor> ListDescriptors();
	static bool TryFindDescriptor(
		const FString& Command,
		FBlueprintHelperBridgeSystemCommandDescriptor& OutDescriptor);
	static const TCHAR* GetSourceName(EBlueprintHelperBridgeCommandSource Source);
};
