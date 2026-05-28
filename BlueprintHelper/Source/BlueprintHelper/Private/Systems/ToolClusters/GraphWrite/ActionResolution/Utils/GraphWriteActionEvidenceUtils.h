// GraphWrite ActionResolution 匿名命名空间函数提取 — 统一工具类
// 包含所有从 8 个 ActionResolution .cpp 文件匿名命名空间中提取的 static 函数

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "GraphWriteActionEvidenceUtils.generated.h"

class FField;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteActionEvidenceUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ===== BlueprintHelperGenericOpsEvidence.cpp =====
	static FString GetEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key);
	static void CopyFactsWithPrefix(const FBlueprintHelperActionResolutionRequest& Request, const FString& Prefix, TMap<FString, FString>& OutFacts);
	static TArray<FString> SplitList(const FString& Value);
	static bool FailMissing(const TCHAR* Key, FString& OutErrorCode, FString& OutMessage);
	static bool IsLatentScheduleOperation(const FString& Operation);

	// ===== BlueprintHelperOpCallableEvidence.cpp =====
	static FString GetOpCallableEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const FString& Key);
	static FString ReadRequestedOperationId(const FBlueprintHelperActionResolutionRequest& Request);

	// ===== BlueprintHelperProjectedSpawnerEvidence.cpp =====
	static FString ReadTrimmedEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key);

	// ===== BlueprintHelperEventDelegatePolicy.cpp =====
	static FBlueprintHelperEventDelegatePolicyDecision AllowEventDelegate();
	static FBlueprintHelperEventDelegatePolicyDecision BlockEventDelegate(const FString& ErrorCode, const FString& Message, EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::Blocked);
	static FString GetDirectEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key);
	static bool IsTrueEvidenceValue(const FString& Value);
	static bool IsFalseEvidenceValue(const FString& Value);
	static bool HasExistingBindingEvidence(const FBlueprintHelperActionResolutionRequest& Request);

	// ===== BlueprintHelperEventDelegateUseSiteEvidence.cpp =====
	static FString GetNamespacedDelegateEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const FString& Key);
	static FString FirstNonEmpty(const TArray<FString>& Values);
	static bool SetMissingResult(const FString& Detail, const FString& Message, FString& OutMissingDetail, FString& OutMessage);
	static UClass* FindClassByPath(const FString& ClassPath);
	template <typename TProperty>
	static TProperty* FindPropertyOnClass(UClass* OwnerClass, const FString& PropertyName);
	static bool PathMatches(const FField* Field, const FString& ExpectedPath);
	static bool ResolveDelegateProperty(FBlueprintHelperEventDelegateUseSiteEvidence& Evidence, FString& OutMissingDetail, FString& OutMessage);
	static bool ResolveComponentBindingProperty(FBlueprintHelperEventDelegateUseSiteEvidence& Evidence, FString& OutMissingDetail, FString& OutMessage);
	static bool ResolveHandlerFunction(FBlueprintHelperEventDelegateUseSiteEvidence& Evidence, FString& OutMissingDetail, FString& OutMessage);
	static FString NormalizeDelegateOperationToken(const FString& Operation);
	static bool IsSupportedDelegateOperation(const FString& Operation);
	static bool RequiresBindingObject(EBlueprintHelperActionSemanticKind SemanticKind);
	static bool RequiresHandlerForOperation(const FString& Operation);
	static bool RequiresResolvedHandlerForOp(EBlueprintHelperActionSemanticKind SemanticKind, const FString& Operation);

	// ===== BlueprintHelperArrayTypedPinEvidenceGuard.cpp =====
	static FString GetMapEvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key);
	static FString NormalizeToken(const FString& Token);
	static bool ElementCategoryRequiresObjectPath(const FString& Category);
	static FString BuildArrayElementIdentity(FBlueprintHelperCallFunctionPinType& PinType);
	static FBlueprintHelperArrayTypedPinEvidenceGuardResult FailArrayPinEvidence(const FString& ErrorCode, const FString& Message);

	// ===== BlueprintHelperFieldCapabilityTypes.cpp =====
	static FString NormalizeFieldCapabilityToken(const FString& Value);
	static FBlueprintHelperFieldCapabilitySpec MakeFieldCapabilitySpec(
		const TCHAR* Id,
		EBlueprintHelperFieldCapabilityPriority Priority,
		EBlueprintHelperFieldCapabilityRootKind RootKind,
		EBlueprintHelperFieldCapabilityAccessMode AccessMode,
		const TCHAR* FieldOperation,
		const TCHAR* FieldScope,
		const TCHAR* ExpectedNodeFamily,
		const TCHAR* ExpectedNodeClass,
		bool bRequiresOwnerClass,
		bool bRequiresFunctionScope,
		bool bRequiresTargetPin,
		bool bRequiresPropertyPath,
		bool bProducesExecPins);
	static FBlueprintHelperFieldCapabilitySpec MakeRejectedFieldCapabilitySpec(
		const TCHAR* Id,
		EBlueprintHelperFieldCapabilityPriority Priority,
		EBlueprintHelperFieldCapabilityAccessMode AccessMode,
		const TCHAR* FieldOperation,
		const TCHAR* FieldScope,
		const TCHAR* RejectReason);
	static const TArray<FBlueprintHelperFieldCapabilitySpec>& GetAllFieldCapabilitySpecs();
	static const FBlueprintHelperFieldCapabilitySpec* FindKnownFieldCapabilitySpec(const FString& CapabilityId, bool bFirstClassOnly);

	// ===== BlueprintHelperContainerActionVocabulary.cpp =====
	static FString NormalizeContainerActionToken(const FString& Value);
	static FBlueprintHelperContainerActionRoleBinding BindInputRole(const TCHAR* RoleName, const TCHAR* FunctionPinName);
	static FBlueprintHelperContainerActionRoleBinding BindOutputRole(const TCHAR* RoleName, const TCHAR* FunctionPinName);
	static FBlueprintHelperContainerActionWildcardPolicy MakeWildcardPolicy(TArray<FString> TypedRoles);
	static FBlueprintHelperContainerActionSpec MakeContainerActionSpec(
		const TCHAR* OperationId,
		const TCHAR* ContainerKind,
		const TCHAR* ContainerOperation,
		const TCHAR* StableUFunctionPath,
		TArray<FString> RequiredRoles,
		TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings,
		EBlueprintHelperContainerActionResultKind ResultKind,
		FBlueprintHelperContainerActionWildcardPolicy WildcardPolicy,
		TArray<FString> ReadbackPinRoles,
		bool bMutatesTarget,
		bool bReturnsValue);
	static const TArray<FBlueprintHelperContainerActionSpec>& GetContainerActionSpecs();
};

// Template definition must be in header
template <typename TProperty>
TProperty* UGraphWriteActionEvidenceUtils::FindPropertyOnClass(UClass* OwnerClass, const FString& PropertyName)
{
	if (!OwnerClass || PropertyName.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	for (UClass* Class = OwnerClass; Class; Class = Class->GetSuperClass())
	{
		if (TProperty* Property = FindFProperty<TProperty>(Class, FName(*PropertyName.TrimStartAndEnd())))
		{
			return Property;
		}
	}
	return nullptr;
}
