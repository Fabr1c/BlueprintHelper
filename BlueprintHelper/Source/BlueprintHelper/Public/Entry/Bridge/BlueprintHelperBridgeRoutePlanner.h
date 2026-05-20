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
	EBlueprintHelperBridgeRouteCluster Cluster = EBlueprintHelperBridgeRouteCluster::Unknown;
	bool bKnownCommand = false;
	bool bRequiresGameThread = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperBridgeRoutePlanner
{
public:
	static FBlueprintHelperBridgeRoutePlan BuildPlan(const FString& Command);
	static const TCHAR* GetClusterName(EBlueprintHelperBridgeRouteCluster Cluster);
};
