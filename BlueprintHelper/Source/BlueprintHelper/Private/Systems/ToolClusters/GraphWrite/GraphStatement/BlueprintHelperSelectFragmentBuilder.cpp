#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

bool FBlueprintHelperSelectFragmentBuilder::Build(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();
	if (!TargetGraph)
	{
		OutError = TEXT("select fragment build failed: target graph is invalid.");
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	FEdGraphPinType ResultPinType;
	if (!UGraphWriteGraphStatementUtils::ValidateSelectResultTypeProof(Expression, ResultPinType, OutError))
	{
		return false;
	}
	if (!ActionResult.IsResolved())
	{
		OutError = ActionResult.Message.IsEmpty() ? TEXT("select fragment build failed: action provider did not resolve.") : ActionResult.Message;
		return false;
	}
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.NodeConfigurationHook = [&Expression, ResultPinType](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, FString& HookError)
	{
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(&SpawnedNode);
		if (!SelectNode)
		{
			HookError = TEXT("select fragment build failed: spawned node is not UK2Node_Select.");
			return false;
		}

		UGraphWriteGraphStatementUtils::ApplyIndexPinType(SelectNode, Expression);
		if (!UGraphWriteGraphStatementUtils::EnsureOptionPinCount(SelectNode, UGraphWriteGraphStatementUtils::GetDesiredOptionCount(Expression), HookError))
		{
			return false;
		}
		UGraphWriteGraphStatementUtils::ApplyResultPinType(SelectNode, ResultPinType);
		return true;
	};
	SpawnOptions.DefaultValueProvider = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
	{
		UGraphWriteGraphStatementUtils::CollectLiteralDefaultsSelect(Cast<UK2Node_Select>(&SpawnedNode), Expression, InOutDefaults);
	};

	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	UK2Node_Select* SelectNode = Cast<UK2Node_Select>(SpawnedNode);
	if (!SelectNode)
	{
		OutError = TEXT("select fragment build failed: UK2Node_Select spawn failed.");
		return false;
	}

	OutFragment.FragmentId = ExpressionId;
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SelectNode;
	OutFragment.Nodes.Add(SelectNode);
	UGraphWriteGraphStatementUtils::PopulateSelectPins(SelectNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("select"));
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
	return true;
}
