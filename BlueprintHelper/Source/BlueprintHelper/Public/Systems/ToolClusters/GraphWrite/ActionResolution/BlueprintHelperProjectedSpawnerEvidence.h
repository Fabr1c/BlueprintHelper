#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprintNodeSpawner;
class UObject;
class UClass;

struct BLUEPRINTHELPER_API FBlueprintHelperProjectedAssetActionEvidence
{
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;

	bool HasSelector() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperProjectedTypePromotionEvidence
{
	FString StableId;
	FString OperatorName;
	FString SourcePinType;
	FString TargetPinType;
	FString ResultPinType;

	bool IsComplete() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperProjectedSpawnerEvidence
{
public:
	static FBlueprintHelperProjectedAssetActionEvidence ReadAssetActionEvidence(
		const FBlueprintHelperActionResolutionRequest& Request);

	static FBlueprintHelperProjectedTypePromotionEvidence ReadTypePromotionEvidence(
		const FBlueprintHelperActionResolutionRequest& Request);

	static FString MakeAssetActionStableId(
		const UObject* ActionOwner,
		const UBlueprintNodeSpawner* Spawner,
		const UClass* NodeClass);

	static FString MakeTypePromotionStableId(
		const FString& OperatorName,
		const FString& SourcePinType,
		const FString& TargetPinType);
};
