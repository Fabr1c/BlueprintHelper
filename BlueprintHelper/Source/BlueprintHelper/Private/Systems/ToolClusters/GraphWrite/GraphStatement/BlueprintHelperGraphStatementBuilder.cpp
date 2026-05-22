#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_PromotableOperator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

static void PopulateCallFragmentPins(UK2Node* CallNode, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	if (!CallNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

static void PopulateCommonFragmentMetadata(const FBlueprintHelperGraphFragmentBuildRequest& Request, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), Request.FragmentId);
	OutFragment.ReviewTargets.Add(Request.FragmentId);
	// DEPRECATED_LAYOUT: these x/y hints are legacy spawn metadata only.
	// Final node positions must come from the UE-side GraphLayout system.
	OutFragment.LayoutHints.Add(TEXT("x"), LexToString(Request.Location.X));
	OutFragment.LayoutHints.Add(TEXT("y"), LexToString(Request.Location.Y));
}

static void ApplyCallPatternBindings(FBlueprintHelperGraphFragmentBuildRequest& Request)
{
	FBlueprintHelperGraphPatternRegistry& Registry = FBlueprintHelperGraphPatternRegistry::Get();

	FString ObjectName;
	FString FunctionName;
	if (FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(Request.Query, ObjectName, FunctionName))
	{
		FunctionName = Registry.ResolveAlias(TEXT("call"), FunctionName);
		Request.Query = ObjectName + TEXT(".") + FunctionName;
	}
	else
	{
		Request.Query = Registry.ResolveAlias(TEXT("call"), Request.Query);
	}

	Registry.ApplyPinAliases(TEXT("call"), Request.DefaultValues);
	Registry.ApplyPinAliases(TEXT("call"), Request.ArgumentTypes);
}

static void ApplyCallPatternDefaults(FBlueprintHelperGraphFragmentBuildRequest& Request)
{
	FBlueprintHelperGraphPatternRegistry::Get().ApplyDefaults(TEXT("call"), Request.DefaultValues);
}

static FString MakeCallFunctionResolveQuery(const FBlueprintHelperGraphFragmentBuildRequest& Request)
{
	const FString StableId = Request.ResolvedStableId.TrimStartAndEnd();
	return StableId.IsEmpty() ? Request.Query : StableId;
}

static void AppendCandidateActionGroup(
	const FString& Target,
	const FBlueprintHelperActionResolutionResult& ResolveResult,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (!OutCandidateFunctions || ResolveResult.CandidateActions.Num() == 0)
	{
		return;
	}

	FBlueprintHelperCandidateFunctionGroup Group;
	Group.Target = Target;
	Group.Candidates = ResolveResult.CandidateActions;
	OutCandidateFunctions->Add(MoveTemp(Group));
}

static FString MakeActionContextStatementId(
	const FString& PreferredStatementId,
	const EBlueprintHelperSpawnerClusterKind ClusterKind,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName)
{
	const FString TrimmedStatementId = PreferredStatementId.TrimStartAndEnd();
	if (!TrimmedStatementId.IsEmpty())
	{
		return TrimmedStatementId;
	}

	return FString::Printf(
		TEXT("%s:%s:%s:%s:%s"),
		*FBlueprintHelperActionResolutionCore::ClusterKindToString(ClusterKind),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
		*Query,
		*TargetPath,
		*TypeName);
}

static FString MakeExpressionActionContextStatementId(const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!Expression.ExpressionId.IsEmpty())
	{
		return Expression.ExpressionId;
	}
	return FString::Printf(TEXT("expression:%s"), *Expression.Path);
}

static FBlueprintHelperActionContextDemand BuildSingleActionContextDemand(
	const FString& StatementId,
	const EBlueprintHelperSpawnerClusterKind ClusterKind,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	const TArray<FString>& ArgumentNames)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = MakeActionContextStatementId(
		StatementId,
		ClusterKind,
		SemanticKind,
		Query,
		TargetPath,
		TypeName);
	Demand.ClusterKind = ClusterKind;
	Demand.SemanticKind = SemanticKind;
	Demand.Query = Query;
	Demand.TargetPath = TargetPath;
	Demand.TypeName = TypeName;
	Demand.ArgumentNames = ArgumentNames;
	return Demand;
}

static bool TryBuildProjectedActionRequestFromContext(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	const EBlueprintHelperSpawnerClusterKind ClusterKind,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	const TArray<FString>& ArgumentNames,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	OutRequest = FBlueprintHelperActionResolutionRequest();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	TArray<FBlueprintHelperActionContextDemand> ContextDemands;
	ContextDemands.Add(BuildSingleActionContextDemand(
		StatementId,
		ClusterKind,
		SemanticKind,
		Query,
		TargetPath,
		TypeName,
		ArgumentNames));

	const FBlueprintHelperActionContextDemand& ContextDemand = ContextDemands[0];
	if (ActionContextScope)
	{
		return ActionContextScope->TryBuildRequest(
			ContextDemand.StatementId,
			Blueprint,
			TargetGraph,
			OutRequest,
			OutError);
	}

	FBlueprintHelperActionContextScope LocalScope;
	const FBlueprintHelperActionContextRevisionToken Revision =
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			TargetGraph,
			ContextDemand.StatementId,
			FString::Printf(
				TEXT("%s:%s"),
				TargetGraph ? *TargetGraph->GetPathName() : TEXT(""),
				*ContextDemand.StatementId));
	FString BuildError;
	if (!FBlueprintHelperActionContextScope::Build(
		Blueprint,
		TargetGraph,
		ContextDemands,
		Revision,
		LocalScope,
		BuildError))
	{
		OutError = BuildError;
		return false;
	}

	return LocalScope.TryBuildRequest(
		ContextDemand.StatementId,
		Blueprint,
		TargetGraph,
		OutRequest,
		OutError);
}

static void ApplyCallActionRequestOverrides(
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest,
	const FString& ExplicitTargetObjectName,
	const TArray<FString>& ArgumentNames,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.SearchMode = BoundRequest.SearchMode;
	InOutRequest.Semantic.AmbiguityPolicy = BoundRequest.AmbiguityPolicy;
	InOutRequest.Semantic.CategoryPriority = BoundRequest.CategoryPriority;
	InOutRequest.Semantic.ArgumentTypes = BoundRequest.ArgumentTypes;
	InOutRequest.Semantic.ArgumentPinTypes = BoundRequest.ArgumentPinTypes;
	InOutRequest.Semantic.TargetObjectType = BoundRequest.TargetObjectType;
	InOutRequest.Semantic.TargetObjectPinType = BoundRequest.TargetObjectPinType;
	InOutRequest.Semantic.ExpectedReturnType = BoundRequest.ExpectedReturnType;
	InOutRequest.Semantic.ExpectedReturnPinType = BoundRequest.ExpectedReturnPinType;
	InOutRequest.Semantic.ArgumentNames = ArgumentNames;
	InOutRequest.Semantic.DefaultValues = BoundRequest.DefaultValues;
	if (!ExplicitTargetObjectName.IsEmpty())
	{
		InOutRequest.Semantic.TargetPath = ExplicitTargetObjectName;
	}
}

static void ApplyExpressionActionRequestOverrides(
	const FString& ExpectedReturnType,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.ExpectedReturnType = ExpectedReturnType;
}

static EBlueprintHelperActionSemanticKind ResolveActionSemanticKindForExpressionKind(EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Get:
		return EBlueprintHelperActionSemanticKind::Get;
	case EBlueprintHelperGraphExpressionKind::GetProperty:
		return EBlueprintHelperActionSemanticKind::GetProperty;
	case EBlueprintHelperGraphExpressionKind::Op:
		return EBlueprintHelperActionSemanticKind::Op;
	case EBlueprintHelperGraphExpressionKind::Construct:
		return EBlueprintHelperActionSemanticKind::Construct;
	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return EBlueprintHelperActionSemanticKind::Deconstruct;
	case EBlueprintHelperGraphExpressionKind::Select:
		return EBlueprintHelperActionSemanticKind::Select;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

static EBlueprintHelperSpawnerClusterKind ResolveSpawnerClusterForSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
	case EBlueprintHelperActionSemanticKind::Op:
		return EBlueprintHelperSpawnerClusterKind::FunctionAction;
	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
		return EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
		return EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
		return EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	default:
		return EBlueprintHelperSpawnerClusterKind::Unknown;
	}
}

static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	EBlueprintHelperSpawnerClusterKind ClusterKind,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FBlueprintHelperActionResolutionResult* OutResult,
	FString& OutError)
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	const TArray<FString> ArgumentNames;
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		StatementId,
		ClusterKind,
		SemanticKind,
		Query,
		TargetPath,
		TypeName,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (ActionResult.IsResolved())
	{
		if (OutResult)
		{
			*OutResult = ActionResult;
		}
		return true;
	}

	OutError = ActionResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("action provider unavailable: semantic=%s cluster=%s"),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(ActionResult.ClusterKind))
		: ActionResult.Message;
	return false;
}

static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	EBlueprintHelperSpawnerClusterKind ClusterKind,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FString& OutError)
{
	return RequireResolvedActionProvider(
		TargetGraph,
		ActionContextScope,
		StatementId,
		ClusterKind,
		SemanticKind,
		Query,
		TargetPath,
		TypeName,
		nullptr,
		OutError);
}

static void PopulateActionProviderFragmentPins(UK2Node* Node, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("then"));
	if (OutFragment.ExecEntryPin)
	{
		OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	}
	if (OutFragment.ExecExitPin)
	{
		OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	}
	if (!Node)
	{
		return;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (!OutFragment.PinBindings.Contains(PinName.ToLower()))
		{
			OutFragment.PinBindings.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
		}
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
			if (!OutFragment.DataInputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataInputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase) && !OutFragment.DataInputs.Contains(TEXT("value")))
			{
				OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
			{
				if (DataInputIndex == 0 && !OutFragment.DataInputs.Contains(TEXT("left")))
				{
					OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					if (!OutFragment.DataInputs.Contains(TEXT("condition")))
					{
						OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
						OutFragment.PinBindings.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
					}
				}
				else if (DataInputIndex == 1 && !OutFragment.DataInputs.Contains(TEXT("right")))
				{
					OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
				}
				++DataInputIndex;
			}
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataOutputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("value")))
			{
				OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

static UEdGraphPin* FindActionProviderDataInputPinByIndex(UK2Node* Node, const int32 RequestedIndex)
{
	if (!Node || RequestedIndex < 0)
	{
		return nullptr;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin
			|| Pin->Direction != EGPD_Input
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			|| Pin->PinName.ToString().Equals(TEXT("self"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (DataInputIndex == RequestedIndex)
		{
			return Pin;
		}
		++DataInputIndex;
	}

	return nullptr;
}

static void AddLiteralDefaultForActionProviderInput(
	UK2Node* Node,
	const int32 InputIndex,
	const FString& SemanticInputName,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	TMap<FString, FString>& InOutDefaults)
{
	if (!Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return;
	}

	const FString LiteralValue = Expression->LiteralValue;
	if (LiteralValue.IsEmpty())
	{
		return;
	}

	if (!SemanticInputName.IsEmpty())
	{
		InOutDefaults.Add(SemanticInputName, LiteralValue);
	}

	if (UEdGraphPin* Pin = FindActionProviderDataInputPinByIndex(Node, InputIndex))
	{
		InOutDefaults.Add(Pin->PinName.ToString(), LiteralValue);
	}
}

static void CollectLiteralDefaultsForActionProviderExpression(
	UK2Node* Node,
	const FBlueprintHelperGraphExpressionIR& Expression,
	TMap<FString, FString>& OutDefaults)
{
	OutDefaults.Reset();
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("left"), Expression.Left, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 1, TEXT("right"), Expression.Right, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("condition"), Expression.Condition, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("value"), Expression.Value, OutDefaults);
}

static bool TryBuildLiteralPromotablePinType(const FString& Type, FEdGraphPinType& OutPinType)
{
	const FString Normalized = Type.TrimStartAndEnd().ToLower();
	if (Normalized.IsEmpty())
	{
		return false;
	}

	if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Normalized == TEXT("int64") || Normalized == TEXT("long"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (Normalized == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (Normalized == TEXT("double") || Normalized == TEXT("real") || Normalized == TEXT("number"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (Normalized == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		return true;
	}
	if (Normalized == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		return true;
	}
	if (Normalized == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		return true;
	}
	return false;
}

static bool TryApplyPromotableOperatorLiteralType(
	UK2Node* Node,
	const int32 InputIndex,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression)
{
	UK2Node_PromotableOperator* OperatorNode = Cast<UK2Node_PromotableOperator>(Node);
	if (!OperatorNode || !Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return false;
	}

	FEdGraphPinType PinType;
	if (!TryBuildLiteralPromotablePinType(Expression->Type, PinType))
	{
		return false;
	}

	UEdGraphPin* InputPin = FindActionProviderDataInputPinByIndex(Node, InputIndex);
	if (!InputPin || !OperatorNode->CanConvertPinType(InputPin))
	{
		return false;
	}

	OperatorNode->ConvertPinType(InputPin, PinType);
	return true;
}

static void ApplyPromotableOperatorLiteralTypes(
	UK2Node* Node,
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!Node || Expression.Kind != EBlueprintHelperGraphExpressionKind::Op)
	{
		return;
	}

	if (TryApplyPromotableOperatorLiteralType(Node, 0, Expression.Left))
	{
		return;
	}
	if (TryApplyPromotableOperatorLiteralType(Node, 1, Expression.Right))
	{
		return;
	}
	TryApplyPromotableOperatorLiteralType(Node, 0, Expression.Value);
}

static bool ResolveActionProviderForExpression(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphExpressionIR& Expression,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FBlueprintHelperActionResolutionResult& OutResult,
	FString& OutError)
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	const TArray<FString> ArgumentNames;
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		MakeExpressionActionContextStatementId(Expression),
		ResolveSpawnerClusterForSemanticKind(SemanticKind),
		SemanticKind,
		Query,
		TargetPath,
		TypeName,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	ApplyExpressionActionRequestOverrides(TypeName, ActionRequest);

	OutResult = FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (OutResult.IsResolved())
	{
		return true;
	}

	OutError = OutResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("action provider unavailable: semantic=%s cluster=%s"),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(OutResult.ClusterKind))
		: OutResult.Message;
	return false;
}

static FString ResolveStructExpressionTypeName(const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!Expression.Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Type.TrimStartAndEnd();
	}
	if (!Expression.Target.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Target.TrimStartAndEnd();
	}
	if (!Expression.Name.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Name.TrimStartAndEnd();
	}
	if (Expression.Value.IsValid() && !Expression.Value->Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Value->Type.TrimStartAndEnd();
	}
	if (Expression.TargetObject.IsValid() && !Expression.TargetObject->Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.TargetObject->Type.TrimStartAndEnd();
	}
	return FString();
}

static void CollectStructExpressionDefaultValues(
	const FBlueprintHelperGraphExpressionIR& Expression,
	TMap<FString, FString>& OutDefaultValues)
{
	OutDefaultValues.Reset();
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& FieldPair : Expression.Fields)
	{
		if (!FieldPair.Value.IsValid())
		{
			continue;
		}

		if (FieldPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(FieldPair.Key, FieldPair.Value->LiteralValue);
		}
	}
}

static void PopulateStructExpressionFragment(
	const FBlueprintHelperGraphExpressionIR& Expression,
	UK2Node* SpawnedNode,
	const FString& SemanticKind,
	FBlueprintHelperNodeFragment& OutFragment)
{
	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	OutFragment.FragmentId = ExpressionId;
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), SemanticKind);
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
}

static bool BuildConstructExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	const FString TypeName = ResolveStructExpressionTypeName(Expression);
	if (TypeName.IsEmpty())
	{
		OutError = TEXT("construct fragment build failed: struct type is required.");
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!ResolveActionProviderForExpression(
		TargetGraph,
		ActionContextScope,
		Expression,
		EBlueprintHelperActionSemanticKind::Construct,
		TypeName,
		TypeName,
		TypeName,
		ActionResult,
		OutError))
	{
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	TMap<FString, FString> DefaultValues;
	CollectStructExpressionDefaultValues(Expression, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	PopulateStructExpressionFragment(Expression, SpawnedNode, TEXT("construct"), OutFragment);
	return true;
}

static bool BuildDeconstructExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	const FString TypeName = ResolveStructExpressionTypeName(Expression);
	if (TypeName.IsEmpty())
	{
		OutError = TEXT("deconstruct fragment build failed: struct type is required.");
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!ResolveActionProviderForExpression(
		TargetGraph,
		ActionContextScope,
		Expression,
		EBlueprintHelperActionSemanticKind::Deconstruct,
		TypeName,
		TypeName,
		TypeName,
		ActionResult,
		OutError))
	{
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	TMap<FString, FString> DefaultValues;
	CollectStructExpressionDefaultValues(Expression, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	PopulateStructExpressionFragment(Expression, SpawnedNode, TEXT("deconstruct"), OutFragment);
	return true;
}
bool FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();
	FBlueprintHelperGraphFragmentBuildRequest BoundRequest = Request;
	ApplyCallPatternBindings(BoundRequest);

	const FString ExplicitTargetObjectName = BoundRequest.Target.TrimStartAndEnd();

	FBlueprintHelperActionResolutionRequest ActionRequest;
	TArray<FString> ArgumentNames;
	BoundRequest.DefaultValues.GetKeys(ArgumentNames);
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		BoundRequest.ActionContextStatementId.IsEmpty()
			? BoundRequest.FragmentId
			: BoundRequest.ActionContextStatementId,
		EBlueprintHelperSpawnerClusterKind::FunctionAction,
		EBlueprintHelperActionSemanticKind::Call,
		MakeCallFunctionResolveQuery(BoundRequest),
		ExplicitTargetObjectName,
		BoundRequest.ExpectedReturnType,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	ApplyCallActionRequestOverrides(BoundRequest, ExplicitTargetObjectName, ArgumentNames, ActionRequest);
	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);

	if (!ActionResult.IsResolved())
	{
		AppendCandidateActionGroup(BoundRequest.Query, ActionResult, OutCandidateFunctions);
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *BoundRequest.Query)
			: ActionResult.Message;
		return false;
	}

	ApplyCallPatternDefaults(BoundRequest);

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = BoundRequest.FragmentId;
	SpawnOptions.DefaultValues = BoundRequest.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(BoundRequest.Location.X, BoundRequest.Location.Y),
		SpawnOptions,
		OutError);

	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = BoundRequest.FragmentId;
	OutFragment.SourceStatementId = BoundRequest.FragmentId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateCallFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(BoundRequest, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!RequireResolvedActionProvider(
		TargetGraph,
		ActionContextScope,
		Request.ActionContextStatementId.IsEmpty()
			? Request.FragmentId
			: Request.ActionContextStatementId,
		EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
		EBlueprintHelperActionSemanticKind::Set,
		Request.Target,
		Request.Target,
		Request.ExpectedReturnType,
		&ActionResult,
		OutError))
	{
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = Request.FragmentId;
	SpawnOptions.DefaultValues = Request.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(Request.Location.X, Request.Location.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = Request.FragmentId;
	OutFragment.SourceStatementId = Request.FragmentId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(Request, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSetPropertyFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (Request.Target.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("set_property fragment build failed: graph-body property target is empty.");
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!RequireResolvedActionProvider(
		TargetGraph,
		ActionContextScope,
		Request.ActionContextStatementId.IsEmpty()
			? Request.FragmentId
			: Request.ActionContextStatementId,
		EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
		EBlueprintHelperActionSemanticKind::SetProperty,
		Request.Target,
		Request.Target,
		Request.ExpectedReturnType,
		&ActionResult,
		OutError))
	{
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = Request.FragmentId;
	SpawnOptions.DefaultValues = Request.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(Request.Location.X, Request.Location.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = Request.FragmentId;
	OutFragment.SourceStatementId = Request.FragmentId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(Request, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return FBlueprintHelperControlFragmentBuilder::BuildSequence(
		TargetGraph,
		FragmentId,
		OutFragment,
		OutError);
}

bool FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Literal)
	{
		OutError = TEXT("literal expression does not create a graph fragment.");
		return false;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Call)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request;
		Request.FragmentId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		Request.ActionContextStatementId = MakeExpressionActionContextStatementId(Expression);
		Request.Query = Expression.Target;
		Request.SearchMode = Expression.SearchMode;
		Request.AmbiguityPolicy = Expression.AmbiguityPolicy;
		Request.CategoryPriority = Expression.CategoryPriority;
		Request.ExpectedReturnType = Expression.Type;
		if (Expression.TargetObject.IsValid())
		{
			Request.Target = Expression.TargetObject->Target;
		}
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (!ArgPair.Value.IsValid())
			{
				continue;
			}
			if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				Request.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
			if (!ArgPair.Value->Type.TrimStartAndEnd().IsEmpty())
			{
				Request.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
			}
		}
		if (!BuildCallFunctionFragment(TargetGraph, Request, OutFragment, OutError, OutCandidateFunctions, ActionContextScope))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Construct)
	{
		return BuildConstructExpressionFragment(TargetGraph, ActionContextScope, Expression, OutFragment, OutError);
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Deconstruct)
	{
		return BuildDeconstructExpressionFragment(TargetGraph, ActionContextScope, Expression, OutFragment, OutError);
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Select)
	{
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!ResolveActionProviderForExpression(
			TargetGraph,
			ActionContextScope,
			Expression,
			EBlueprintHelperActionSemanticKind::Select,
			TEXT("select"),
			Expression.Target,
			Expression.Type,
			ActionResult,
			OutError))
		{
			return false;
		}
		return FBlueprintHelperSelectFragmentBuilder::Build(TargetGraph, Expression, ActionResult, OutFragment, OutError);
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ResolveActionSemanticKindForExpressionKind(Expression.Kind);
	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Get)
	{
		const FString VariableName = !Expression.ResolvedTarget.Member.IsEmpty()
			? Expression.ResolvedTarget.Member
			: Expression.Target;
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!RequireResolvedActionProvider(
			TargetGraph,
			ActionContextScope,
			MakeExpressionActionContextStatementId(Expression),
			EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
			EBlueprintHelperActionSemanticKind::Get,
			VariableName,
			VariableName,
			Expression.Type,
			&ActionResult,
			OutError))
		{
			return false;
		}

		UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
			TargetGraph,
			ActionResult,
			FVector2D::ZeroVector,
			OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		OutFragment.FragmentId = ExpressionId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
		return true;
	}

	if (SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
	{
		const FString Query = Expression.Kind == EBlueprintHelperGraphExpressionKind::Op
			? Expression.Operator
			: Expression.Target;
		const FString TargetPath = !Expression.ResolvedTarget.PropertyPath.IsEmpty()
			? Expression.ResolvedTarget.PropertyPath
			: Expression.Target;
		const FString TypeName = Expression.Type;
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!ResolveActionProviderForExpression(
			TargetGraph,
			ActionContextScope,
			Expression,
			SemanticKind,
			Query,
			TargetPath,
			TypeName,
			ActionResult,
			OutError))
		{
			return false;
		}

		const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
		SpawnOptions.NodeId = ExpressionId;
		SpawnOptions.PinNormalizationHook = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&)
		{
			ApplyPromotableOperatorLiteralTypes(&SpawnedNode, Expression);
		};
		SpawnOptions.DefaultValueProvider = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
		{
			CollectLiteralDefaultsForActionProviderExpression(&SpawnedNode, Expression, InOutDefaults);
		};
		UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
			TargetGraph,
			ActionResult,
			FVector2D::ZeroVector,
			SpawnOptions,
			OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		OutFragment.FragmentId = ExpressionId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
		OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
		OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	OutError = FString::Printf(
		TEXT("expression fragment pattern is not implemented yet: %s."),
		*Expression.PatternName);
	return false;
}
