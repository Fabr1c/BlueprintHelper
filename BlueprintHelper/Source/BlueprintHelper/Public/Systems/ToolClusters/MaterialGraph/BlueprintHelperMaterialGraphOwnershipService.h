// BlueprintHelper MaterialGraph ownership metadata service.

#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphOwnershipService
{
public:
	static const TCHAR* BlockIdMetadataKey();
	static const TCHAR* NodeKeyMetadataKey();
	static const TCHAR* OwnershipMetadataKey();
	static const TCHAR* OwnedMetadataValue();
};
