// BlueprintHelper Service Layer - internal Blueprint function/event signature service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintSignature/BlueprintHelperSignatureTypes.h"

class FBlueprintHelperBlueprintStructureService;

class BLUEPRINTHELPER_API FBlueprintHelperSignatureService
{
public:
	explicit FBlueprintHelperSignatureService(const FBlueprintHelperBlueprintStructureService& InStructureService);

	FBlueprintHelperToolResultBase EnsureFunction(
		const FBlueprintHelperEnsureFunctionSignatureRequest& Request) const;

	FBlueprintHelperToolResultBase EnsureCustomEvent(
		const FBlueprintHelperEnsureCustomEventSignatureRequest& Request) const;

	FBlueprintHelperToolResultBase EnsureMacro(
		const FBlueprintHelperEnsureMacroSignatureRequest& Request) const;

	FBlueprintHelperToolResultBase RemoveSignature(
		const FBlueprintHelperRemoveSignatureRequest& Request) const;

	FBlueprintHelperToolResultBase EnsureEventDispatcher(
		const FBlueprintHelperEnsureEventDispatcherSignatureRequest& Request) const;

	FBlueprintHelperToolResultBase EnsureOverrideEvent(
		const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request) const;

private:
	const FBlueprintHelperBlueprintStructureService& StructureService;
};
