#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperContainerActionRoleBinding
{
	FString RoleName;
	FString FunctionPinName;
};

struct FBlueprintHelperContainerActionSpec
{
	FString OperationId;
	FString ContainerKind;
	FString ContainerOperation;
	FString FunctionQuery;
	TArray<FString> RequiredRoles;
	TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings;
	bool bMutatesTarget = false;
	bool bReturnsValue = false;
	bool bPureQuery = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperContainerActionVocabulary
{
public:
	static const FBlueprintHelperContainerActionSpec* Find(
		const FString& ContainerKind,
		const FString& ContainerOperation);

	static TArray<FBlueprintHelperContainerActionSpec> All();
};
