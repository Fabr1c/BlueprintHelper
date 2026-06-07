#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperWriteFamilyAdapterRegistry
{
public:
	static const TArray<TSharedPtr<IBlueprintHelperWriteFamilyAdapter>>& GetAdapters();
	static bool TryFindByWriteFamily(
		const FString& WriteFamily,
		TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& OutAdapter);
	static bool TryFindByRuntimeAdapterId(
		const FString& RuntimeAdapterId,
		TSharedPtr<IBlueprintHelperWriteFamilyAdapter>& OutAdapter);
	static TArray<FString> GetRegisteredWriteFamilies();
};
