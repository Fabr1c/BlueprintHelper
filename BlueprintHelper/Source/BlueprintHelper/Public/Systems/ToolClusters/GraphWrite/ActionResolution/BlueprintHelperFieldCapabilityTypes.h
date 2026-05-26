#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperFieldCapabilityPriority : uint8
{
	P0,
	P1,
	P2,
	SupportOnly,
	OtherCluster,
	DiagnosticOnly
};

enum class EBlueprintHelperFieldCapabilityRootKind : uint8
{
	Member,
	InheritedMember,
	SparseData,
	FunctionParam,
	Local,
	ObjectPinMember,
	ComponentRef,
	ComponentProperty,
	StructMember,
	NestedPropertyPath,
	Unsupported
};

enum class EBlueprintHelperFieldCapabilityAccessMode : uint8
{
	Get,
	Set,
	ReadWritePath,
	Diagnostic
};

struct BLUEPRINTHELPER_API FBlueprintHelperFieldCapabilitySpec
{
	FString Id;
	EBlueprintHelperFieldCapabilityPriority Priority = EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly;
	EBlueprintHelperFieldCapabilityRootKind RootKind = EBlueprintHelperFieldCapabilityRootKind::Unsupported;
	EBlueprintHelperFieldCapabilityAccessMode AccessMode = EBlueprintHelperFieldCapabilityAccessMode::Diagnostic;
	FString FieldOperation;
	FString FieldScope;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClass;
	bool bFirstClassStatement = false;
	bool bRequiresOwnerClass = false;
	bool bRequiresFunctionScope = false;
	bool bRequiresTargetPin = false;
	bool bRequiresPropertyPath = false;
	bool bProducesExecPins = false;
	FString RejectReason;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldCapabilityRegistry
{
public:
	static const FBlueprintHelperFieldCapabilitySpec* FindById(const FString& CapabilityId);
	static TArray<FBlueprintHelperFieldCapabilitySpec> GetFirstClassSpecs();
	static bool IsAllowedUserStatement(const FString& CapabilityId, FString& OutRejectReason);
	static TArray<FBlueprintHelperFieldCapabilitySpec> GetSpecsByOperationAndScope(
		const FString& FieldOperation,
		const FString& FieldScope);
	static const FBlueprintHelperFieldCapabilitySpec* InferFromOperationAndScope(
		const FString& FieldOperation,
		const FString& FieldScope);
	static FString MakeStableCapabilityKey(const FBlueprintHelperFieldCapabilitySpec& Spec);
};
