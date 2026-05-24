#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperFieldPathRole : uint8
{
	Variable,
	PropertyPath,
	ComponentRef,
	FieldAccess,
	Unknown
};

struct BLUEPRINTHELPER_API FBlueprintHelperResolvedFieldPath
{
	EBlueprintHelperFieldPathRole Role = EBlueprintHelperFieldPathRole::Unknown;
	FString RootName;
	FString LeafName;
	FString FullPath;
	TArray<FString> Segments;
	FString OwnerClassPath;
	FBlueprintHelperCallFunctionPinType OwnerPinType;
	FBlueprintHelperCallFunctionPinType LeafPinType;
	bool bRequiresFragmentDecomposition = false;
	bool bIsValid = false;
	FString ErrorCode;
	FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldPathResolution
{
public:
	static FBlueprintHelperResolvedFieldPath Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const TMap<FString, FString>& Evidence);

	static EBlueprintHelperFieldPathRole RoleFromScope(const FString& FieldScope);
	static FString RoleToScope(EBlueprintHelperFieldPathRole Role);
};
