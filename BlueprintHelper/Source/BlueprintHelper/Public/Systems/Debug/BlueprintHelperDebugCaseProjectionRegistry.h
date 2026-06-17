// Registry for DebugCase export projections.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Debug/BlueprintHelperDebugCaseProjectionAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperDebugCaseProjectionRegistry
{
public:
	FBlueprintHelperDebugCaseProjectionRegistry() = default;
	FBlueprintHelperDebugCaseProjectionRegistry(FBlueprintHelperDebugCaseProjectionRegistry&& Other) = default;
	FBlueprintHelperDebugCaseProjectionRegistry& operator=(FBlueprintHelperDebugCaseProjectionRegistry&& Other) = default;
	FBlueprintHelperDebugCaseProjectionRegistry(const FBlueprintHelperDebugCaseProjectionRegistry& Other) = delete;
	FBlueprintHelperDebugCaseProjectionRegistry& operator=(const FBlueprintHelperDebugCaseProjectionRegistry& Other) = delete;

	static FBlueprintHelperDebugCaseProjectionRegistry BuildDefault();

	void RegisterAdapter(TUniquePtr<IBlueprintHelperDebugCaseProjectionAdapter>&& Adapter);
	bool BuildProjection(
		const FBlueprintHelperDebugCase& DebugCase,
		const FBlueprintHelperDebugCaseProjectionContext& Context,
		FBlueprintHelperDebugCaseProjectionResult& OutResult,
		FString* OutError) const;
	int32 GetAdapterCount() const;

private:
	TArray<TUniquePtr<IBlueprintHelperDebugCaseProjectionAdapter>> Adapters;
};
