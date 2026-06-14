// BlueprintHelper MaterialGraph ownership metadata service.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphOwnershipService.h"

const TCHAR* FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey()
{
	return TEXT("BlueprintHelper.BlockId");
}

const TCHAR* FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey()
{
	return TEXT("BlueprintHelper.NodeKey");
}

const TCHAR* FBlueprintHelperMaterialGraphOwnershipService::OwnershipMetadataKey()
{
	return TEXT("BlueprintHelper.Ownership");
}

const TCHAR* FBlueprintHelperMaterialGraphOwnershipService::OwnedMetadataValue()
{
	return TEXT("owned");
}
