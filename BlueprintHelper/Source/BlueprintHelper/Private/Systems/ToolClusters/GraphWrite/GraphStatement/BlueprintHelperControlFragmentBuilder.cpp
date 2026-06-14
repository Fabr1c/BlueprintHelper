#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

static UK2Node_FunctionResult* BlueprintHelperFindReusableFunctionResultNode(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!TargetGraph)
	{
		return nullptr;
	}
	if (Statement.Args.Num() > 0)
	{
		return nullptr;
	}

	UK2Node_FunctionResult* FirstResultNode = nullptr;
	UK2Node_FunctionResult* FirstResultNodeWithDataInput = nullptr;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
		if (!ResultNode)
		{
			continue;
		}

		if (!FirstResultNode)
		{
			FirstResultNode = ResultNode;
		}
		if (!FirstResultNodeWithDataInput &&
			UGraphWriteGraphStatementUtils::FindFirstDataInputPin(ResultNode))
		{
			FirstResultNodeWithDataInput = ResultNode;
		}
	}

	return Statement.Value.IsValid()
		? FirstResultNodeWithDataInput
		: FirstResultNode;
}

static void BlueprintHelperBreakReusableReturnInputLinks(UK2Node_FunctionResult* ReturnNode)
{
	if (!ReturnNode)
	{
		return;
	}

	ReturnNode->Modify();
	for (UEdGraphPin* Pin : ReturnNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			Pin->Modify();
			Pin->BreakAllPinLinks(true);
		}
	}
}

static void BlueprintHelperApplyReusableReturnLiteralDefaults(
	UK2Node_FunctionResult* ReturnNode,
	const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!ReturnNode)
	{
		return;
	}

	TMap<FString, FString> Defaults;
	UGraphWriteGraphStatementUtils::CollectReturnLiteralDefault(ReturnNode, Statement, Defaults);
	if (Defaults.Num() == 0)
	{
		return;
	}

	for (UEdGraphPin* Pin : ReturnNode->Pins)
	{
		if (!Pin
			|| Pin->Direction != EGPD_Input
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString* DefaultValue = Defaults.Find(Pin->PinName.ToString());
		if (!DefaultValue)
		{
			continue;
		}

		Pin->Modify();
		Pin->DefaultValue = *DefaultValue;
	}
}

bool FBlueprintHelperControlFragmentBuilder::BuildSequence(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	const FString CleanFragmentId = UGraphWriteGraphStatementUtils::SanitizeFragmentIdPart(FragmentId);
	FBlueprintHelperActionResolutionResult ActionResult;
	if (!UGraphWriteGraphStatementUtils::ResolveControlActionProvider(TargetGraph, ActionContextScope, CleanFragmentId, TEXT("sequence"), CleanFragmentId, ActionResult, OutError))
	{
		return false;
	}

	UK2Node_ExecutionSequence* SequenceNode = UGraphWriteGraphStatementUtils::SpawnTypedControlNode<UK2Node_ExecutionSequence>(
		TargetGraph,
		ActionResult,
		TEXT("sequence"),
		CleanFragmentId,
		OutError);
	if (!SequenceNode)
	{
		return false;
	}

	TArray<UEdGraphPin*> OutputPins;
	if (!UGraphWriteGraphStatementUtils::EnsureSequenceOutputCount(SequenceNode, 2, OutputPins, OutError))
	{
		return false;
	}

	UGraphWriteGraphStatementUtils::PopulateCommonControlMetadata(CleanFragmentId, CleanFragmentId, TEXT("sequence"), SequenceNode, OutFragment);
	UGraphWriteGraphStatementUtils::PopulateSequencePins(SequenceNode, OutputPins, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildSequence(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return BuildSequence(TargetGraph, nullptr, FragmentId, OutFragment, OutError);
}

bool FBlueprintHelperControlFragmentBuilder::BuildBranch(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Statement.Kind != EBlueprintHelperGraphStatementKind::Branch)
	{
		OutError = TEXT("branch control fragment build failed: statement kind is not Branch.");
		return false;
	}

	const FString FragmentId = UGraphWriteGraphStatementUtils::ResolveStatementFragmentId(Statement);
	const FString StatementContextId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
	FBlueprintHelperActionResolutionResult ActionResult;
	if (!UGraphWriteGraphStatementUtils::ResolveControlActionProvider(TargetGraph, ActionContextScope, StatementContextId, TEXT("branch"), FragmentId, ActionResult, OutError))
	{
		return false;
	}

	TMap<FString, FString> DefaultValues;
	UGraphWriteGraphStatementUtils::CollectBranchLiteralDefaults(Statement, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = FragmentId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node_IfThenElse* BranchNode = UGraphWriteGraphStatementUtils::SpawnTypedControlNode<UK2Node_IfThenElse>(
		TargetGraph,
		ActionResult,
		TEXT("branch"),
		FragmentId,
		SpawnOptions,
		OutError);
	if (!BranchNode)
	{
		return false;
	}

	UGraphWriteGraphStatementUtils::PopulateCommonControlMetadata(FragmentId, Statement.StatementId, TEXT("branch"), BranchNode, OutFragment);
	UGraphWriteGraphStatementUtils::PopulateBranchPins(BranchNode, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildReturn(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Statement.Kind != EBlueprintHelperGraphStatementKind::Return)
	{
		OutError = TEXT("return control fragment build failed: statement kind is not Return.");
		return false;
	}

	const FString FragmentId = UGraphWriteGraphStatementUtils::ResolveStatementFragmentId(Statement);
	const FString StatementContextId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;

	if (UK2Node_FunctionResult* ReusableReturnNode =
		BlueprintHelperFindReusableFunctionResultNode(TargetGraph, Statement))
	{
		BlueprintHelperBreakReusableReturnInputLinks(ReusableReturnNode);
		BlueprintHelperApplyReusableReturnLiteralDefaults(ReusableReturnNode, Statement);
		UGraphWriteGraphStatementUtils::PopulateCommonControlMetadata(FragmentId, Statement.StatementId, TEXT("return"), ReusableReturnNode, OutFragment);
		UGraphWriteGraphStatementUtils::PopulateReturnPins(ReusableReturnNode, OutFragment);
		return true;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!UGraphWriteGraphStatementUtils::ResolveControlActionProvider(TargetGraph, ActionContextScope, StatementContextId, TEXT("return"), FragmentId, ActionResult, OutError))
	{
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = FragmentId;
	SpawnOptions.DefaultValueProvider = [&Statement](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
	{
		if (UK2Node_FunctionResult* FunctionResultNode = Cast<UK2Node_FunctionResult>(&SpawnedNode))
		{
			UGraphWriteGraphStatementUtils::CollectReturnLiteralDefault(FunctionResultNode, Statement, InOutDefaults);
		}
	};
	UK2Node_FunctionResult* ReturnNode = UGraphWriteGraphStatementUtils::SpawnTypedControlNode<UK2Node_FunctionResult>(
		TargetGraph,
		ActionResult,
		TEXT("return"),
		FragmentId,
		SpawnOptions,
		OutError);
	if (!ReturnNode)
	{
		return false;
	}

	UGraphWriteGraphStatementUtils::PopulateCommonControlMetadata(FragmentId, Statement.StatementId, TEXT("return"), ReturnNode, OutFragment);
	UGraphWriteGraphStatementUtils::PopulateReturnPins(ReturnNode, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	switch (Statement.Kind)
	{
	case EBlueprintHelperGraphStatementKind::Branch:
		return BuildBranch(TargetGraph, ActionContextScope, Statement, OutFragment, OutError);
	case EBlueprintHelperGraphStatementKind::Sequence:
		return BuildSequence(TargetGraph, ActionContextScope, UGraphWriteGraphStatementUtils::ResolveStatementFragmentId(Statement), OutFragment, OutError);
	case EBlueprintHelperGraphStatementKind::Return:
		return BuildReturn(TargetGraph, ActionContextScope, Statement, OutFragment, OutError);
	default:
		OutFragment = FBlueprintHelperNodeFragment();
		OutError = FString::Printf(
			TEXT("control fragment build failed: unsupported statement kind '%s'."),
			*Statement.PatternName);
		return false;
	}
}
