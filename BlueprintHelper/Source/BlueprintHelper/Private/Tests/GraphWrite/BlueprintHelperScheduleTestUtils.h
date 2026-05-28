// 共享的 Schedule 测试辅助函数

#pragma once

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

inline bool TryProjectScheduleEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& Query,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence)
{
	FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Blueprint;
	ProjectionRequest.TargetGraph = Graph;
	ProjectionRequest.RequiredEvidence.Query = Query;
	ProjectionRequest.Query = Query;
	ProjectionRequest.ErrorPrefix = TEXT("schedule");

	const FBlueprintHelperActionDatabaseProjectionResult Projection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);
	if ((Projection.Status != EBlueprintHelperActionResolutionStatus::Resolved
		&& Projection.Status != EBlueprintHelperActionResolutionStatus::Ambiguous)
		|| Projection.Candidates.Num() == 0)
	{
		return false;
	}

	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate = Projection.Candidates[0];
	OutEvidence.StableId = Candidate.StableId;
	OutEvidence.NodeClassPath = Candidate.NodeClassPath;
	OutEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	OutEvidence.OwnerPath = Candidate.OwnerPath;
	OutEvidence.Query = Candidate.Query;
	OutEvidence.MenuName = Candidate.MenuName;
	OutEvidence.Category = Candidate.Category;

	FBlueprintHelperActionDatabaseProjectionRequest ExactProjectionRequest;
	ExactProjectionRequest.Blueprint = Blueprint;
	ExactProjectionRequest.TargetGraph = Graph;
	ExactProjectionRequest.RequiredEvidence.StableId = OutEvidence.StableId;
	ExactProjectionRequest.RequiredEvidence.NodeClassPath = OutEvidence.NodeClassPath;
	ExactProjectionRequest.RequiredEvidence.SpawnerSignature = OutEvidence.SpawnerSignature;
	ExactProjectionRequest.RequiredEvidence.OwnerPath = OutEvidence.OwnerPath;
	ExactProjectionRequest.RequiredEvidence.Query = OutEvidence.Query;
	ExactProjectionRequest.RequiredEvidence.MenuName = OutEvidence.MenuName;
	ExactProjectionRequest.RequiredEvidence.Category = OutEvidence.Category;
	ExactProjectionRequest.ErrorPrefix = TEXT("schedule");
	const FBlueprintHelperActionDatabaseProjectionResult ExactProjection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ExactProjectionRequest);
	return ExactProjection.Status == EBlueprintHelperActionResolutionStatus::Resolved
		&& ExactProjection.Candidates.Num() == 1;
}

inline bool TryProjectScheduleEvidenceFromQueries(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FString>& Queries,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence)
{
	for (const FString& Query : Queries)
	{
		if (TryProjectScheduleEvidence(Blueprint, Graph, Query, OutEvidence))
		{
			return true;
		}
	}
	return false;
}
