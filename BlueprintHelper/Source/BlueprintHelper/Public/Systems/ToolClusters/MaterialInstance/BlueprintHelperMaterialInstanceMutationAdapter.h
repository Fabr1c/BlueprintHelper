// BlueprintHelper MaterialInstance mutation adapter.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceMutationAdapter
{
public:
	static constexpr const TCHAR* OperationMaterialInstanceEdit = TEXT("material_instance_edit");

	static FBlueprintHelperToolResultBase ExecutePayload(
		const TSharedPtr<FJsonObject>& Payload);
};
