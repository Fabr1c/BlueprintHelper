#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;

struct FBlueprintHelperAssetActionProjectionRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* TargetGraph = nullptr;
	FBlueprintHelperProjectedAssetActionEvidence RequiredEvidence;
	FString Query;
};

struct FBlueprintHelperAssetActionProjectedCandidate
{
	const UObject* ActionOwner = nullptr;
	UBlueprintNodeSpawner* Spawner = nullptr;
	UClass* NodeClass = nullptr;
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;
};

struct FBlueprintHelperAssetActionProjectionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	FString ErrorCode;
	FString Message;
	TArray<FBlueprintHelperAssetActionProjectedCandidate> Candidates;
};

class FBlueprintHelperAssetActionProjectionService
{
public:
	static FBlueprintHelperAssetActionProjectionResult Project(
		const FBlueprintHelperAssetActionProjectionRequest& Request);
};
