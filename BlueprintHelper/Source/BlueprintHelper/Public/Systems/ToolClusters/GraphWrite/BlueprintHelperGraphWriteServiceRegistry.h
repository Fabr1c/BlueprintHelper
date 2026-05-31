// BlueprintHelper GraphWrite operation registry.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteServiceRegistry
{
public:
	using FExecuteHandler = TFunction<FBlueprintHelperToolResultBase(const TSharedRef<FJsonObject>& Payload)>;

	static bool IsKnownOperation(const FString& Operation);

	void RegisterHandler(const FString& Operation, FExecuteHandler Handler);
	bool HasHandler(const FString& Operation) const;
	FBlueprintHelperToolResultBase Execute(
		const FString& Operation,
		const TSharedRef<FJsonObject>& Payload) const;

private:
	static FString NormalizeOperation(const FString& Operation);
	static FBlueprintHelperToolError MakeUnsupportedOperationError(
		const FString& Operation,
		bool bKnownOperation);

	TMap<FString, FExecuteHandler> Handlers;
};
