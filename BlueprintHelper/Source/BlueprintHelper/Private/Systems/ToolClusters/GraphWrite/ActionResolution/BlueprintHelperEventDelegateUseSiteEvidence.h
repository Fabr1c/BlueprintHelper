#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FMulticastDelegateProperty;
class FObjectPropertyBase;
class UClass;
class UFunction;

struct FBlueprintHelperEventDelegateUseSiteEvidence
{
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	FString DelegateName;
	FString DelegateOperation;
	FString DelegateOwnerClassPath;
	FString DelegatePropertyName;
	FString DelegatePropertyPath;
	FString DelegateSignature;
	FString DelegateSignatureFunctionPath;
	FString ComponentPath;
	FString ComponentBindingOwnerClassPath;
	FString ComponentBindingFieldPath;
	FString BindingObjectPath;
	FString HandlerName;
	FString HandlerScopeClassPath;
	FString UnbindMode;
	FMulticastDelegateProperty* DelegateProperty = nullptr;
	FObjectPropertyBase* ComponentBindingProperty = nullptr;
	UFunction* HandlerFunction = nullptr;
};

class FBlueprintHelperEventDelegateUseSiteEvidenceReader
{
public:
	static bool TryRead(
		const FBlueprintHelperActionResolutionRequest& Request,
		EBlueprintHelperActionSemanticKind SemanticKind,
		FBlueprintHelperEventDelegateUseSiteEvidence& OutEvidence,
		FString& OutMissingDetail,
		FString& OutMessage);
};
