// BlueprintHelper Bridge Layer - static route planning

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperBridgeRouteCluster : uint8
{
	Unknown,
	Core,
	Debug,
	SharedServices,
	AssetBrowser,
	AssetDiscovery,
	TaskRuntime,
	GraphWrite,
	BlueprintVariables,
	BlueprintStructure,
	AssetFactory,
	Component,
	ClassSettings,
	Signature,
	UMGWidget,
	DataTable,
	ObjectProperty,
	EditorCommand,
	Review
};

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeRoutePlan
{
	FString Command;
	FString SourceId;
	FString PolicyId;
	EBlueprintHelperBridgeRouteCluster Cluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	bool bKnownCommand = false;
	bool bRequiresGameThread = false;
	bool bAgentVisible = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperBridgeRoutePlanner
{
public:
	static FBlueprintHelperBridgeRoutePlan BuildPlan(const FString& Command);
	static const TCHAR* GetClusterName(EBlueprintHelperBridgeRouteCluster Cluster);
};
