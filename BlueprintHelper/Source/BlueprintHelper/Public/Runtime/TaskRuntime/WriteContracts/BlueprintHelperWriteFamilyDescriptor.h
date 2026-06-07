#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperWriteFamilyCapabilityStatus : uint8
{
	Active,
	Hidden,
	Planned,
	Reserved
};

struct BLUEPRINTHELPER_API FBlueprintHelperWriteFamilyDescriptor
{
	FString WriteFamily;
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	FString BridgeCommand;
	FString ClusterFamily;
	FString BodyKind;
	FString OwnershipMode;
	FString ExternalAnchorMode;
	FString DryRunPolicyId;
	FString ReadbackProjectionMode;
	FString ResultProjectionPolicyId;
	FString MetricsIdentity;
	EBlueprintHelperWriteFamilyCapabilityStatus Status =
		EBlueprintHelperWriteFamilyCapabilityStatus::Active;
	bool bSupportsPreviewUnitOfWork = false;
	bool bRequiresSandboxGraph = false;
	bool bRequiresAdapterPreflight = false;
	bool bAllowsPreviewMutation = false;
	bool bRequiresRollbackFinalizer = false;
	bool bRequiresReadbackProjection = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperWriteFamilyDescriptorRegistry
{
public:
	static const TArray<FBlueprintHelperWriteFamilyDescriptor>& GetKnownDescriptors();
	static bool TryFindByWriteFamily(
		const FString& WriteFamily,
		FBlueprintHelperWriteFamilyDescriptor& OutDescriptor);
	static bool TryFindByClusterFamily(
		const FString& ClusterFamily,
		FBlueprintHelperWriteFamilyDescriptor& OutDescriptor);
	static FString StatusToString(EBlueprintHelperWriteFamilyCapabilityStatus Status);
};
