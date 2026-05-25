#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;

struct FBlueprintHelperActionDatabaseProjectionEvidence
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

struct FBlueprintHelperActionDatabaseProjectionRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* TargetGraph = nullptr;
	FBlueprintHelperActionDatabaseProjectionEvidence RequiredEvidence;
	FString Query;
	FString ErrorPrefix = TEXT("action_database");
};

struct FBlueprintHelperActionDatabaseProjectedCandidate
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

struct FBlueprintHelperActionDatabaseProjectionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	FString ErrorCode;
	FString Message;
	TArray<FBlueprintHelperActionDatabaseProjectedCandidate> Candidates;
};

class FBlueprintHelperActionDatabaseProjectionService
{
public:
	static FBlueprintHelperActionDatabaseProjectionResult Project(
		const FBlueprintHelperActionDatabaseProjectionRequest& Request);
};
