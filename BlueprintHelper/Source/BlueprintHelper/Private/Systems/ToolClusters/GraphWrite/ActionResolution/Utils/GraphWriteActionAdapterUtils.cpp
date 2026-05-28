#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionAdapterUtils.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "K2Node_DynamicCast.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

// ===== BlueprintHelperGenericTransformSpawnerFactory.cpp =====

void UGraphWriteActionAdapterUtils::CustomizeCastNode(UEdGraphNode* NewNode, bool bIsTemplateNode, UClass* TargetClass)
{
	if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(NewNode))
	{
		CastNode->TargetType = TargetClass;
	}
}

// ===== BlueprintHelperAssetActionProjectionService.cpp =====

FBlueprintHelperActionDatabaseProjectionEvidence UGraphWriteActionAdapterUtils::ToNeutralEvidence(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	FBlueprintHelperActionDatabaseProjectionEvidence Result;
	Result.StableId = Evidence.StableId;
	Result.NodeClassPath = Evidence.NodeClassPath;
	Result.SpawnerSignature = Evidence.SpawnerSignature;
	Result.OwnerPath = Evidence.OwnerPath;
	Result.Query = Evidence.Query;
	Result.MenuName = Evidence.MenuName;
	Result.Category = Evidence.Category;
	return Result;
}

FBlueprintHelperAssetActionProjectedCandidate UGraphWriteActionAdapterUtils::ToAssetCandidate(
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	FBlueprintHelperAssetActionProjectedCandidate Result;
	Result.ActionOwner = Candidate.ActionOwner;
	Result.Spawner = Candidate.Spawner;
	Result.NodeClass = Candidate.NodeClass;
	Result.StableId = Candidate.StableId;
	Result.NodeClassPath = Candidate.NodeClassPath;
	Result.SpawnerSignature = Candidate.SpawnerSignature;
	Result.OwnerPath = Candidate.OwnerPath;
	Result.Query = Candidate.Query;
	Result.MenuName = Candidate.MenuName;
	Result.Category = Candidate.Category;
	return Result;
}

// ===== BlueprintHelperActionDatabaseProjectionService.cpp =====

FString UGraphWriteActionAdapterUtils::NormalizeProjectionText(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd().ToLower();
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT("|"), TEXT(""));
	Result.ReplaceInline(TEXT("/"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	return Result;
}

bool UGraphWriteActionAdapterUtils::MatchesExactEvidence(const FString& Expected, const FString& Actual)
{
	return Expected.IsEmpty() || Expected.Equals(Actual.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionAdapterUtils::MatchesQueryEvidence(
	const FString& Query,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	if (Query.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	const FString NormalizedQuery = NormalizeProjectionText(Query);
	if (NormalizedQuery.IsEmpty())
	{
		return true;
	}

	const FString SearchText = NormalizeProjectionText(FString::Printf(
		TEXT("%s %s %s %s %s"),
		*Candidate.StableId,
		*Candidate.OwnerPath,
		*Candidate.NodeClassPath,
		*Candidate.MenuName,
		*Candidate.Category));
	return SearchText.Contains(NormalizedQuery);
}

bool UGraphWriteActionAdapterUtils::TryBuildCandidate(
	const FBlueprintActionContext& ActionContext,
	const UObject* ActionOwner,
	UBlueprintNodeSpawner* Spawner,
	FBlueprintHelperActionDatabaseProjectedCandidate& OutCandidate)
{
	if (!Spawner)
	{
		return false;
	}

	FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
	UClass* NodeClass = const_cast<UClass*>(ActionInfo.GetNodeClass());
	if (!NodeClass)
	{
		return false;
	}

	const FBlueprintActionUiSpec UiSpec =
		Spawner->GetUiSpec(ActionContext, ActionInfo.GetBindings());

	OutCandidate.ActionOwner = ActionOwner;
	OutCandidate.Spawner = Spawner;
	OutCandidate.NodeClass = NodeClass;
	OutCandidate.StableId = FBlueprintHelperProjectedSpawnerEvidence::MakeAssetActionStableId(ActionOwner, Spawner, NodeClass);
	OutCandidate.NodeClassPath = NodeClass->GetPathName();
	OutCandidate.SpawnerSignature = Spawner->GetSpawnerSignature().ToString();
	OutCandidate.OwnerPath = ActionOwner ? ActionOwner->GetPathName() : FString();
	OutCandidate.MenuName = UiSpec.MenuName.ToString().TrimStartAndEnd();
	OutCandidate.Category = UiSpec.Category.ToString().TrimStartAndEnd();
	return true;
}

bool UGraphWriteActionAdapterUtils::MatchesProjectedEvidence(
	const FBlueprintHelperActionDatabaseProjectionEvidence& Evidence,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate)
{
	if (!MatchesExactEvidence(Evidence.StableId, Candidate.StableId))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.NodeClassPath, Candidate.NodeClassPath))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.SpawnerSignature, Candidate.SpawnerSignature))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.OwnerPath, Candidate.OwnerPath))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.MenuName, Candidate.MenuName))
	{
		return false;
	}
	if (!MatchesExactEvidence(Evidence.Category, Candidate.Category))
	{
		return false;
	}
	return MatchesQueryEvidence(Evidence.Query, Candidate);
}

FBlueprintHelperActionDatabaseProjectionEvidence UGraphWriteActionAdapterUtils::BuildEffectiveEvidence(
	const FBlueprintHelperActionDatabaseProjectionRequest& Request)
{
	FBlueprintHelperActionDatabaseProjectionEvidence Evidence = Request.RequiredEvidence;
	if (Evidence.Query.IsEmpty())
	{
		Evidence.Query = Request.Query.TrimStartAndEnd();
	}
	return Evidence;
}

FBlueprintHelperActionDatabaseProjectionResult UGraphWriteActionAdapterUtils::MakeProjectionFailure(
	EBlueprintHelperActionResolutionStatus Status,
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionDatabaseProjectionResult Result;
	Result.Status = Status;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

// ===== BlueprintHelperActionNodeSpawnerAdapter.cpp =====

UK2Node* UGraphWriteActionAdapterUtils::InvokeNodeSpawnerInternal(
	UEdGraph* TargetGraph,
	UBlueprintNodeSpawner* NodeSpawner,
	const FBlueprintHelperActionResolutionResult* ActionResult,
	const FString& StableId,
	const FVector2D& Location,
	const FBlueprintHelperActionNodeSpawnOptions& Options,
	FString& OutError,
	TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics)
{
	if (!TargetGraph)
	{
		OutError = TEXT("action provider spawn failed: target graph is invalid.");
		return nullptr;
	}
	if (!NodeSpawner)
	{
		OutError = FString::Printf(
			TEXT("action provider spawn failed: resolved spawner is no longer valid: %s."),
			*StableId);
		return nullptr;
	}

	UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(TargetGraph, Options.Bindings, Location);
	UK2Node* K2Node = Cast<UK2Node>(SpawnedNode);
	if (!K2Node)
	{
		OutError = FString::Printf(
			TEXT("action provider spawn failed: spawner did not create a K2 node: %s."),
			*StableId);
		return nullptr;
	}

	K2Node->NodePosX = static_cast<int32>(Location.X);
	K2Node->NodePosY = static_cast<int32>(Location.Y);
	if (Options.bReconstructAfterSpawn && TargetGraph->GetSchema())
	{
		TargetGraph->GetSchema()->ReconstructNode(*K2Node);
	}

	FBlueprintHelperActionNodeSpawnContext Context;
	Context.TargetGraph = TargetGraph;
	Context.ActionResult = ActionResult;
	Context.Location = Location;
	Context.NodeId = Options.NodeId;
	if (Options.NodeConfigurationHook && !Options.NodeConfigurationHook(*K2Node, Context, OutError))
	{
		return nullptr;
	}
	if (Options.PinNormalizationHook)
	{
		Options.PinNormalizationHook(*K2Node, Context);
	}
	else
	{
		FBlueprintHelperActionNodeSpawnerAdapter::NoOpPinNormalization(*K2Node, Context);
	}

	TMap<FString, FString> EffectiveDefaultValues = Options.DefaultValues;
	if (Options.DefaultValueProvider)
	{
		Options.DefaultValueProvider(*K2Node, Context, EffectiveDefaultValues);
	}

	if (Options.bApplyDefaultValues && EffectiveDefaultValues.Num() > 0)
	{
		TArray<FBlueprintGeneratorDiagnostic> Diagnostics =
			FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(K2Node, EffectiveDefaultValues, Options.NodeId);
		if (OutDefaultValueDiagnostics)
		{
			OutDefaultValueDiagnostics->Append(MoveTemp(Diagnostics));
		}
	}
	return K2Node;
}
