// BlueprintHelper external-user GraphWrite operation policy.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperExternalGraphWriteAdapterOperationKind : uint8
{
	Unknown,
	MergeFlow,
	PropertyPatch,
	LinkPatch,
	BodyReplace,
};

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphWriteOperationPolicy
{
public:
	static const FString& ExternalGraphEditStrategy();
	static const FString& MergeFlowAdapterOperation();
	static const FString& PropertyPatchAdapterOperation();
	static const FString& LinkPatchAdapterOperation();
	static const FString& BodyReplaceAdapterOperation();

	static bool IsExternalGraphEditStrategy(const FString& Strategy);
	static bool IsExternalPropertyPatchAdapterOperation(const FString& AdapterOperation);
	static bool IsExternalLinkPatchAdapterOperation(const FString& AdapterOperation);
	static bool TryClassifyAdapterOperation(
		const FString& AdapterOperation,
		EBlueprintHelperExternalGraphWriteAdapterOperationKind& OutKind);
	static bool TryResolveTaskOpAdapterOperation(
		const FString& TaskOp,
		FString& OutAdapterOperation,
		EBlueprintHelperExternalGraphWriteAdapterOperationKind& OutKind);
	static bool TryBuildExpectedMutationAllowlist(
		const FString& AdapterOperation,
		TArray<FString>& OutExpectedMutations);
};
