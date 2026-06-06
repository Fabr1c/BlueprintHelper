#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyAdapterDescriptor
{
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	EBlueprintHelperGraphBodyKind BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	FString BoundarySource;
	bool bSupportsDryRunUnitOfWork = false;
	bool bSupportsExternalAnchors = false;
	bool bReservedOnly = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteRouteSyncValidationIssue
{
	FString RouteId;
	FString RuntimeAdapterId;
	FString Status;
	FString Code;
	FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyAdapterRegistry
{
public:
	static TArray<FBlueprintHelperGraphBodyAdapterDescriptor> GetKnownDescriptors();
	static bool TryFindByRuntimeAdapterId(
		const FString& RuntimeAdapterId,
		FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor);
	static bool TryFindByTaskSpecStrategy(
		const FString& TaskSpecStrategy,
		FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor);
	static TArray<FBlueprintHelperGraphWriteRouteSyncValidationIssue> ValidateGeneratedRouteSync();
};
