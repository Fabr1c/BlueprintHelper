#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyRequest
{
	FString OperationKind;
	FString TaskSpecStrategy;
	FString ReplaceScope;
	FString PatchScope;
	FString MergeScope;
	FString AssetPath;
	FString GraphName;
	FString EntryName;
	FString SelectorKind;
	FString RuntimeAdapterId;
	TSharedPtr<FJsonObject> Payload;
	UBlueprint* Blueprint = nullptr;
};
