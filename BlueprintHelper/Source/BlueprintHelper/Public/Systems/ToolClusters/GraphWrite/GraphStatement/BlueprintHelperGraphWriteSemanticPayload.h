#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteSemanticPayload
{
	static const TCHAR* SchemaName();

	FString TargetAssetPath;
	FString TargetGraph;
	FString Mode;

	bool bCompile = false;
	bool bSave = false;
	bool bStrict = true;
	bool bDryRun = false;
	bool bCreateMissingVariables = false;
	bool bReconstructExistingNodes = false;

	TSharedPtr<FJsonObject> LogicSpec;

	TSharedRef<FJsonObject> ToJsonObject() const;
	FString ToJsonString() const;
};
