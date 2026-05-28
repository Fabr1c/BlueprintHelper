// BlueprintHelper call_function resolver anonymous namespace utility helpers.

#pragma once

#include "CoreMinimal.h"

// The generated header must be the last include
#include "GraphWriteFunctionResolutionUtils.generated.h"

struct FBlueprintHelperCallFunctionPinType;
struct FBlueprintHelperCallFunctionResolveRequest;
struct FBlueprintHelperCallFunctionCandidate;
struct FBlueprintHelperK2CallContext;
class UFunction;
class UClass;
class UBlueprintNodeSpawner;
class UEdGraph;
class FProperty;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteFunctionResolutionUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FString DescribePinTypeForDiagnostics(const FBlueprintHelperCallFunctionPinType& PinType);
	static UClass* ResolveClassFromPinType(const FBlueprintHelperCallFunctionPinType& PinType);
	static UClass* ResolveNodeClassByPath(const FString& NodeClassPath);
	static TSubclassOf<UK2Node_CallFunction> InferNodeClassForFunction(const UFunction* Function);
	static TSubclassOf<UK2Node_CallFunction> ResolveCandidateNodeClass(const UFunction* Function, const UBlueprintNodeSpawner* NodeSpawner);
	static void GetPermittedNodeClasses(const FBlueprintHelperCallFunctionResolveRequest& Request, TArray<UClass*>& OutNodeClasses);
	static bool IsStableCallableIdPermitted(const FString& StableId, const FBlueprintHelperCallFunctionResolveRequest& Request);
	static UFunction* ResolveStableCallableFunction(const FString& StableCallableId);
	static bool IsNodeClassPermitted(const UClass* NodeClass, const FBlueprintHelperCallFunctionResolveRequest& Request);
	static bool IsContainerPinCompatibleWithProperty(const FBlueprintHelperCallFunctionPinType& PinType, const FProperty* Property);
	static UFunction* CanonicalizeActionDatabaseFunction(const UFunction* Function);
	static FString StripLeadingBoolPrefixForCompare(const FString& Name);
	static bool IsTargetObjectSemanticPortName(const FString& Name);
	static void RemoveTargetObjectSemanticPortFromContext(FBlueprintHelperK2CallContext& Context);
	static void AddRequiredStableCallableCandidates(const FBlueprintHelperCallFunctionResolveRequest& Request, TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates);
};
