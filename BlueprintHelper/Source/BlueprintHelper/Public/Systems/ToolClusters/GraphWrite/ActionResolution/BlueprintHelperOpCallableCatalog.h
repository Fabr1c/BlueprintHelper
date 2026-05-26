#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperOpCallableSpec
{
	FString OperationId;
	FString SpawnFamily;
	FString StableCallableId;
	FString RequiredNodeClassPath;
	TArray<FString> RequiredEvidenceKeys;
	FString RejectionCode;
};

class BLUEPRINTHELPER_API FBlueprintHelperOpCallableCatalog
{
public:
	static const TArray<FBlueprintHelperOpCallableSpec>& GetSupportedCallableSpecs();
	static const TArray<FBlueprintHelperOpCallableSpec>& GetExcludedSpecs();
	static const TArray<FString>& GetTypePromotionOperationIds();
	static const FBlueprintHelperOpCallableSpec* FindSupportedSpec(const FString& OperationId);
	static const FBlueprintHelperOpCallableSpec* FindExcludedSpec(const FString& OperationId);
	static bool IsTypePromotionOperation(const FString& OperationId);
	static FString NormalizeOperationId(const FString& OperationId);
};
