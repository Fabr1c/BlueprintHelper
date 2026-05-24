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
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

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

static FString MakeActionContextStatementId(
	const FString& PreferredStatementId,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	const FString& FieldOperation = FString(),
	const FString& FieldScope = FString())
{
	const FString TrimmedStatementId = PreferredStatementId.TrimStartAndEnd();
	if (!TrimmedStatementId.IsEmpty())
	{
		return TrimmedStatementId;
	}

	return FString::Printf(
		TEXT("%s:%s:%s:%s:%s:%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
		*FieldOperation,
		*FieldScope,
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

static bool TryBuildProjectedActionRequestFromContext(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	const TArray<FString>& ArgumentNames,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError,
	const FString& FieldOperation = FString(),
	const FString& FieldScope = FString())
{
	OutRequest = FBlueprintHelperActionResolutionRequest();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	const FBlueprintHelperActionContextDemand ContextDemand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			MakeActionContextStatementId(
				StatementId,
				SemanticKind,
				Query,
				TargetPath,
				TypeName,
				FieldOperation,
				FieldScope),
			FString(),
			SemanticKind,
			Query,
			TargetPath,
			PropertyPath,
			TypeName,
			SearchMode,
			AmbiguityPolicy,
			CategoryPriority,
			ArgumentNames,
			FieldOperation,
			FieldScope);

	if (ActionContextScope)
	{
		return ActionContextScope->TryBuildRequest(
			ContextDemand.StatementId,
			Blueprint,
			TargetGraph,
			OutRequest,
			OutError);
	}

	OutError = FString::Printf(
		TEXT("action_context_scope_required: %s"),
		*ContextDemand.StatementId);
	return false;
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

static FBlueprintHelperCallFunctionPinType MakePinTypeFromCreateToken(const FString& Token)
{
	FBlueprintHelperCallFunctionPinType PinType;
	TArray<FString> Parts;
	Token.TrimStartAndEnd().ParseIntoArray(Parts, TEXT("|"), true);
	if (Parts.Num() > 0)
	{
		PinType.Category = Parts[0];
	}
	if (Parts.Num() > 1)
	{
		PinType.ObjectPath = Parts[1];
	}
	return PinType;
}

static void ApplyCreateActionRequestOverrides(
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.CreateOperation = BoundRequest.CreateOperation.TrimStartAndEnd().ToLower();
	InOutRequest.Semantic.ClassPath = BoundRequest.ClassPath.TrimStartAndEnd();
	InOutRequest.Semantic.AssetPath = BoundRequest.AssetPath.TrimStartAndEnd();
	if (!InOutRequest.Semantic.ClassPath.IsEmpty())
	{
		InOutRequest.Semantic.TargetPath = InOutRequest.Semantic.ClassPath;
	}
	if (!BoundRequest.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("element"), BoundRequest.PinType.TrimStartAndEnd());
		InOutRequest.Semantic.ContainerElementPinType = MakePinTypeFromCreateToken(BoundRequest.PinType);
	}
	if (!BoundRequest.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("key"), BoundRequest.KeyPinType.TrimStartAndEnd());
		InOutRequest.Semantic.ContainerKeyPinType = MakePinTypeFromCreateToken(BoundRequest.KeyPinType);
	}
	if (!BoundRequest.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("value"), BoundRequest.ValuePinType.TrimStartAndEnd());
		InOutRequest.Semantic.ContainerValuePinType = MakePinTypeFromCreateToken(BoundRequest.ValuePinType);
	}
}

static EBlueprintHelperActionSemanticKind ResolveActionSemanticKindForExpressionKind(EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Field:
		return EBlueprintHelperActionSemanticKind::Field;
	case EBlueprintHelperGraphExpressionKind::Op:
		return EBlueprintHelperActionSemanticKind::Op;
	case EBlueprintHelperGraphExpressionKind::Construct:
		return EBlueprintHelperActionSemanticKind::Construct;
	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return EBlueprintHelperActionSemanticKind::Deconstruct;
	case EBlueprintHelperGraphExpressionKind::Select:
		return EBlueprintHelperActionSemanticKind::Select;
	case EBlueprintHelperGraphExpressionKind::Create:
		return EBlueprintHelperActionSemanticKind::Create;
	case EBlueprintHelperGraphExpressionKind::Convert:
		return EBlueprintHelperActionSemanticKind::Convert;
	case EBlueprintHelperGraphExpressionKind::Schedule:
		return EBlueprintHelperActionSemanticKind::Schedule;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	FBlueprintHelperActionResolutionResult* OutResult,
	FString& OutError,
	const FString& FieldOperation = FString(),
	const FString& FieldScope = FString())
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	const TArray<FString> ArgumentNames;
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		StatementId,
		SemanticKind,
		Query,
		TargetPath,
		PropertyPath,
		TypeName,
		SearchMode,
		AmbiguityPolicy,
		CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError,
		FieldOperation,
		FieldScope))
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
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	FString& OutError,
	const FString& FieldOperation = FString(),
	const FString& FieldScope = FString())
{
	return RequireResolvedActionProvider(
		TargetGraph,
		ActionContextScope,
		StatementId,
		SemanticKind,
		Query,
		TargetPath,
		PropertyPath,
		TypeName,
		SearchMode,
		AmbiguityPolicy,
		CategoryPriority,
		nullptr,
		OutError,
		FieldOperation,
		FieldScope);
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
	const FString& PropertyPath,
	const FString& TypeName,
	FBlueprintHelperActionResolutionResult& OutResult,
	FString& OutError,
	const FString& FieldOperation = FString(),
	const FString& FieldScope = FString())
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	const TArray<FString> ArgumentNames;
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		MakeExpressionActionContextStatementId(Expression),
		SemanticKind,
		Query,
		TargetPath,
		PropertyPath,
		TypeName,
		Expression.SearchMode,
		Expression.AmbiguityPolicy,
		Expression.CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError,
		FieldOperation,
		FieldScope))
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
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
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
		FString(),
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
		FString(),
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
		EBlueprintHelperActionSemanticKind::Call,
		MakeCallFunctionResolveQuery(BoundRequest),
		ExplicitTargetObjectName,
		BoundRequest.PropertyPath,
		BoundRequest.ExpectedReturnType,
		BoundRequest.SearchMode,
		BoundRequest.AmbiguityPolicy,
		BoundRequest.CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	ApplyCallActionRequestOverrides(BoundRequest, ExplicitTargetObjectName, ArgumentNames, ActionRequest);
	ActionRequest.ContextEvidence.Append(BoundRequest.ContextEvidence);
	ApplyCallPatternDefaults(BoundRequest);

	FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
	CoordinatorRequest.TargetGraph = TargetGraph;
	CoordinatorRequest.BuildRequest = &BoundRequest;
	CoordinatorRequest.ActionRequest = ActionRequest;
	CoordinatorRequest.SemanticKind = EBlueprintHelperActionSemanticKind::Call;
	CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::Call;
	CoordinatorRequest.CandidateGroupTarget = BoundRequest.Query;
	CoordinatorRequest.FailurePrefix = TEXT("call_function resolve failed");
	CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;

	return FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
		CoordinatorRequest,
		OutFragment,
		OutError,
		OutCandidateFunctions);
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
		EBlueprintHelperActionSemanticKind::Field,
		Request.Target,
		Request.Target,
		Request.PropertyPath,
		Request.ExpectedReturnType,
		Request.SearchMode,
		Request.AmbiguityPolicy,
		Request.CategoryPriority,
		&ActionResult,
		OutError,
		TEXT("set"),
		TEXT("variable")))
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
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);
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
		EBlueprintHelperActionSemanticKind::Field,
		Request.Target,
		Request.Target,
		Request.PropertyPath,
		Request.ExpectedReturnType,
		Request.SearchMode,
		Request.AmbiguityPolicy,
		Request.CategoryPriority,
		&ActionResult,
		OutError,
		TEXT("set"),
		TEXT("property_path")))
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
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildCreateFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();

	const FString CreateTarget = !Request.ClassPath.TrimStartAndEnd().IsEmpty()
		? Request.ClassPath.TrimStartAndEnd()
		: Request.Target.TrimStartAndEnd();

	FBlueprintHelperActionResolutionRequest ActionRequest;
	TArray<FString> ArgumentNames;
	Request.DefaultValues.GetKeys(ArgumentNames);
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		Request.ActionContextStatementId.IsEmpty()
			? Request.FragmentId
			: Request.ActionContextStatementId,
		EBlueprintHelperActionSemanticKind::Create,
		Request.CreateOperation,
		CreateTarget,
		Request.PropertyPath,
		Request.TypeName,
		Request.SearchMode,
		Request.AmbiguityPolicy,
		Request.CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	ApplyCreateActionRequestOverrides(Request, ActionRequest);
	ActionRequest.ContextEvidence.Append(Request.ContextEvidence);

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (!ActionResult.IsResolved())
	{
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("create resolve failed: %s"), *Request.CreateOperation)
			: ActionResult.Message;
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
	OutFragment.SourceStatementId = Request.SourceStatementId.IsEmpty() ? Request.FragmentId : Request.SourceStatementId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("create"));
	OutFragment.OwnershipTags.Add(TEXT("create_operation"), Request.CreateOperation);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildActionProviderFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	EBlueprintHelperActionSemanticKind SemanticKind,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();

	FBlueprintHelperActionResolutionRequest ActionRequest;
	TArray<FString> ArgumentNames;
	Request.DefaultValues.GetKeys(ArgumentNames);
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		Request.ActionContextStatementId.IsEmpty()
			? Request.FragmentId
			: Request.ActionContextStatementId,
		SemanticKind,
		Request.Query,
		Request.Target,
		Request.PropertyPath,
		Request.TypeName,
		Request.SearchMode,
		Request.AmbiguityPolicy,
		Request.CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	ActionRequest.ContextEvidence.Append(Request.ContextEvidence);

	FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
	CoordinatorRequest.TargetGraph = TargetGraph;
	CoordinatorRequest.BuildRequest = &Request;
	CoordinatorRequest.ActionRequest = ActionRequest;
	CoordinatorRequest.SemanticKind = SemanticKind;
	CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::ActionProvider;
	CoordinatorRequest.CandidateGroupTarget = Request.Query;
	CoordinatorRequest.FailurePrefix = FString::Printf(
		TEXT("action provider unavailable: semantic=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
	CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;

	return FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
		CoordinatorRequest,
		OutFragment,
		OutError,
		nullptr);
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
			FString(),
			Expression.Type,
			ActionResult,
			OutError))
		{
			return false;
		}
		return FBlueprintHelperSelectFragmentBuilder::Build(TargetGraph, Expression, ActionResult, OutFragment, OutError);
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Create)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromExpression(Expression);
		Request.FragmentId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		Request.SourceStatementId = Expression.ExpressionId;
		Request.ActionContextStatementId = MakeExpressionActionContextStatementId(Expression);
		Request.Query = Expression.CreateOperation;
		Request.Target = !Expression.ClassPath.IsEmpty() ? Expression.ClassPath : Expression.Target;
		Request.TypeName = Expression.Type;
		if (!BuildCreateFragment(TargetGraph, Request, OutFragment, OutError, ActionContextScope))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		return true;
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ResolveActionSemanticKindForExpressionKind(Expression.Kind);
	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Field)
	{
		const FString FieldScope = Expression.FieldScope.IsEmpty() ? TEXT("variable") : Expression.FieldScope;
		const bool bPropertyPathField = FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase);
		const FString FieldName = !Expression.ResolvedTarget.Member.IsEmpty()
			? Expression.ResolvedTarget.Member
			: (!Expression.Property.IsEmpty() ? Expression.Property : Expression.Target);
		const FString TargetPath = bPropertyPathField && !Expression.ResolvedTarget.Raw.IsEmpty()
			? Expression.ResolvedTarget.Raw
			: FieldName;
		const FString PropertyPath = bPropertyPathField
			? (!Expression.ResolvedTarget.PropertyPath.IsEmpty() ? Expression.ResolvedTarget.PropertyPath : Expression.Property)
			: Expression.ResolvedTarget.PropertyPath;
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!RequireResolvedActionProvider(
			TargetGraph,
			ActionContextScope,
			MakeExpressionActionContextStatementId(Expression),
			EBlueprintHelperActionSemanticKind::Field,
			FieldName,
			TargetPath,
			PropertyPath,
			Expression.Type,
			Expression.SearchMode,
			Expression.AmbiguityPolicy,
			Expression.CategoryPriority,
			&ActionResult,
			OutError,
			TEXT("get"),
			FieldScope))
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
		FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
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
			Expression.ResolvedTarget.PropertyPath,
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
		FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
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
