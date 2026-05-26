#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperContainerActionRoleBinding
{
	FString RoleName;
	FString FunctionPinName;
	bool bProjectToCallableRequest = true;
};

enum class EBlueprintHelperContainerActionResultKind : uint8
{
	None,
	ReturnValue,
	OutputPins
};

struct FBlueprintHelperContainerActionWildcardPolicy
{
	TArray<FString> TypedRoles;
};

struct FBlueprintHelperContainerActionSpec
{
	FString OperationId;
	FString ContainerKind;
	FString ContainerOperation;
	FString StableUFunctionPath;
	FString FunctionQuery;
	TArray<FString> RequiredRoles;
	TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings;
	EBlueprintHelperContainerActionResultKind ResultKind = EBlueprintHelperContainerActionResultKind::None;
	FBlueprintHelperContainerActionWildcardPolicy WildcardPolicy;
	TArray<FString> ReadbackPinRoles;
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
