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
	bool HasProjectedIdentity() const;
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

struct BLUEPRINTHELPER_API FBlueprintHelperProjectedScheduleActionEvidence
{
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;
	FString DelegatePinName;
	FString HandlerName;
	FString HandlerFunctionPath;
	FString HandlerSourceCluster;
	FString SignatureEvidenceId;
	FString GraphLatentAllowed;

	bool HasSelector() const;
	bool HasProjectedIdentity() const;
	bool HasTimerHandlerEvidence() const;
	bool IsGraphLatentAllowed() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperProjectedSpawnerEvidence
{
public:
	static FBlueprintHelperProjectedAssetActionEvidence ReadAssetActionEvidence(
		const FBlueprintHelperActionResolutionRequest& Request);

	static void WriteAssetActionEvidence(
		const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
		TMap<FString, FString>& OutContextEvidence);

	static FBlueprintHelperProjectedTypePromotionEvidence ReadTypePromotionEvidence(
		const FBlueprintHelperActionResolutionRequest& Request);

	static FBlueprintHelperProjectedScheduleActionEvidence ReadScheduleActionEvidence(
		const FBlueprintHelperActionResolutionRequest& Request);

	static void WriteScheduleActionEvidence(
		const FBlueprintHelperProjectedScheduleActionEvidence& Evidence,
		TMap<FString, FString>& OutContextEvidence);

	static FString MakeAssetActionStableId(
		const UObject* ActionOwner,
		const UBlueprintNodeSpawner* Spawner,
		const UClass* NodeClass);

	static FString MakeTypePromotionStableId(
		const FString& OperatorName,
		const FString& SourcePinType,
		const FString& TargetPinType);

	static FString MakeScheduleActionStableId(
		const UObject* ActionOwner,
		const UBlueprintNodeSpawner* Spawner,
		const UClass* NodeClass);
};
