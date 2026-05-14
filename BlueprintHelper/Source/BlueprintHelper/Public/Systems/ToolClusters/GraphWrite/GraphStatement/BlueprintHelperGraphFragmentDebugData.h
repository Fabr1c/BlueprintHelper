#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentDebugData
{
public:
	static TSharedPtr<FJsonObject> BuildFromLogicSpec(
		const TSharedPtr<FJsonObject>& LogicSpec,
		UBlueprint* Blueprint);

	static void AttachToData(
		TSharedPtr<FJsonObject>& Data,
		const TSharedPtr<FJsonObject>& FragmentDebugData);
};
