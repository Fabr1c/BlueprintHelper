// BlueprintHelper Bridge Layer - DataTable static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperDataTableService;

class BLUEPRINTHELPER_API FBlueprintHelperDataTableBridgeRoutes
{
public:
	explicit FBlueprintHelperDataTableBridgeRoutes(const FBlueprintHelperDataTableService& InDataTableService);

	static bool IsDataTableCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperDataTableService& DataTableService;
};
