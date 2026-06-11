#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MakeContainer.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentWrappers.h"
#include "UObject/SoftObjectPath.h"

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

static bool IsTimerDelegateScheduleOperation(const FString& ScheduleOperation)
{
	return NormalizeScheduleOperationToken(ScheduleOperation) == TEXT("timer_delegate_node");
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
	const FString ResolvedStableId = BoundRequest.ResolvedStableId.TrimStartAndEnd();
	if (!ResolvedStableId.IsEmpty())
	{
		InOutRequest.Semantic.StableId = ResolvedStableId;
	}
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

static void ApplyFieldCapabilityActionRequestOverrides(
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.CapabilityId = BoundRequest.CapabilityId;
	InOutRequest.Semantic.CapabilityFacts.Append(BoundRequest.CapabilityFacts);
	for (const TPair<FString, FString>& FactPair : BoundRequest.CapabilityFacts)
	{
		if (!FactPair.Key.IsEmpty() && !FactPair.Value.TrimStartAndEnd().IsEmpty())
		{
			InOutRequest.ContextEvidence.FindOrAdd(FactPair.Key, FactPair.Value.TrimStartAndEnd());
		}
	}

	const FString TargetPinCategory = BoundRequest.CapabilityFacts.FindRef(TEXT("field.target_pin_type")).TrimStartAndEnd();
	const FString TargetPinObjectPath = BoundRequest.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")).TrimStartAndEnd();
	if (!TargetPinCategory.IsEmpty() || !TargetPinObjectPath.IsEmpty())
	{
		InOutRequest.Semantic.TargetObjectPinType.Category = TargetPinCategory;
		InOutRequest.Semantic.TargetObjectPinType.ObjectPath = TargetPinObjectPath;
	}
}

static FBlueprintHelperCallFunctionPinType MakePinTypeFromCreateToken(const FString& Token)
{
	return FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Token);
}

static FString DescribeCreatePinTypeEvidence(
	const FBlueprintHelperCallFunctionPinType& PinType,
	const FString& Fallback)
{
	if (!PinType.IsValid())
	{
		return Fallback.TrimStartAndEnd();
	}

	TArray<FString> Parts;
	if (!PinType.Category.IsEmpty())
	{
		Parts.Add(PinType.Category);
	}
	if (!PinType.SubCategory.IsEmpty())
	{
		Parts.Add(PinType.SubCategory);
	}
	if (!PinType.ObjectPath.IsEmpty())
	{
		Parts.Add(PinType.ObjectPath);
	}
	if (!PinType.ContainerType.IsEmpty())
	{
		Parts.Add(PinType.ContainerType);
	}
	return FString::Join(Parts, TEXT("|"));
}

static void ApplyCreateActionRequestOverrides(
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.CreateOperation = BoundRequest.CreateOperation.TrimStartAndEnd().ToLower();
	InOutRequest.Semantic.FunctionOperation = BoundRequest.FunctionOperation.TrimStartAndEnd().ToLower();
	InOutRequest.Semantic.ClassPath = BoundRequest.ClassPath.TrimStartAndEnd();
	InOutRequest.Semantic.AssetPath = BoundRequest.AssetPath.TrimStartAndEnd();
	if (!InOutRequest.Semantic.FunctionOperation.IsEmpty())
	{
		InOutRequest.Semantic.DefaultValues.Add(TEXT("function_operation"), InOutRequest.Semantic.FunctionOperation);
	}
	if (InOutRequest.Semantic.FunctionOperation.Equals(TEXT("create_function"), ESearchCase::IgnoreCase)
		&& !BoundRequest.Query.TrimStartAndEnd().IsEmpty())
	{
		InOutRequest.Semantic.Query = BoundRequest.Query.TrimStartAndEnd();
	}
	if (!InOutRequest.Semantic.ClassPath.IsEmpty())
	{
		InOutRequest.Semantic.TargetPath = InOutRequest.Semantic.ClassPath;
	}
	if (!BoundRequest.PinType.TrimStartAndEnd().IsEmpty())
	{
		const FBlueprintHelperCallFunctionPinType ElementPinType = MakePinTypeFromCreateToken(BoundRequest.PinType);
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("element"), DescribeCreatePinTypeEvidence(ElementPinType, BoundRequest.PinType));
		InOutRequest.Semantic.ContainerElementPinType = ElementPinType;
	}
	if (!BoundRequest.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		const FBlueprintHelperCallFunctionPinType KeyPinType = MakePinTypeFromCreateToken(BoundRequest.KeyPinType);
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("key"), DescribeCreatePinTypeEvidence(KeyPinType, BoundRequest.KeyPinType));
		InOutRequest.Semantic.ContainerKeyPinType = KeyPinType;
	}
	if (!BoundRequest.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		const FBlueprintHelperCallFunctionPinType ValuePinType = MakePinTypeFromCreateToken(BoundRequest.ValuePinType);
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("value"), DescribeCreatePinTypeEvidence(ValuePinType, BoundRequest.ValuePinType));
		InOutRequest.Semantic.ContainerValuePinType = ValuePinType;
	}
}

static bool IsClassBackedCreateOperation(const FString& CreateOperation)
{
	const FString NormalizedOperation = CreateOperation.TrimStartAndEnd().ToLower();
	return NormalizedOperation.Equals(TEXT("create_widget"), ESearchCase::IgnoreCase)
		|| NormalizedOperation.Equals(TEXT("spawn_actor"), ESearchCase::IgnoreCase)
		|| NormalizedOperation.Equals(TEXT("construct_object"), ESearchCase::IgnoreCase);
}

static UClass* ResolveCreateClassPath(const FString& ClassPath)
{
	const FString CleanClassPath = ClassPath.TrimStartAndEnd();
	if (CleanClassPath.IsEmpty())
	{
		return nullptr;
	}
	if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *CleanClassPath))
	{
		return LoadedClass;
	}
	return FSoftClassPath(CleanClassPath).TryLoadClass<UObject>();
}

static bool ConfigureClassBackedCreateNode(
	UK2Node& SpawnedNode,
	const FString& CreateOperation,
	const FString& ClassPath,
	FString& OutError)
{
	if (!IsClassBackedCreateOperation(CreateOperation) || ClassPath.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}

	UK2Node_ConstructObjectFromClass* ConstructNode = Cast<UK2Node_ConstructObjectFromClass>(&SpawnedNode);
	if (!ConstructNode)
	{
		OutError = FString::Printf(
			TEXT("create node class configuration failed: operation '%s' did not spawn a class-backed construct node."),
			*CreateOperation);
		return false;
	}

	UClass* ResolvedClass = ResolveCreateClassPath(ClassPath);
	if (!ResolvedClass)
	{
		OutError = FString::Printf(
			TEXT("create node class configuration failed: could not resolve class_path '%s'."),
			*ClassPath);
		return false;
	}

	UEdGraphPin* ClassPin = ConstructNode->GetClassPin();
	if (!ClassPin)
	{
		OutError = FString::Printf(
			TEXT("create node class configuration failed: operation '%s' spawned node without a class pin."),
			*CreateOperation);
		return false;
	}

	if (UClass* BaseClass = Cast<UClass>(ClassPin->PinType.PinSubCategoryObject.Get()))
	{
		if (!ResolvedClass->IsChildOf(BaseClass))
		{
			OutError = FString::Printf(
				TEXT("create node class configuration failed: class_path '%s' is not a child of required base '%s'."),
				*ResolvedClass->GetPathName(),
				*BaseClass->GetPathName());
			return false;
		}
	}

	ClassPin->DefaultObject = ResolvedClass;
	ClassPin->DefaultValue.Reset();
	ClassPin->DefaultTextValue = FText::GetEmpty();
	ConstructNode->PinDefaultValueChanged(ClassPin);
	return true;
}

static FString NormalizeContainerToken(const FString& Token)
{
	return Token.TrimStartAndEnd().ToLower();
}

static FString FirstNonEmptyContainerType(const FString& Primary, const FString& Secondary)
{
	const FString PrimaryValue = Primary.TrimStartAndEnd();
	return PrimaryValue.IsEmpty() ? Secondary.TrimStartAndEnd() : PrimaryValue;
}

static FString ExtractContainerActionFunctionName(const FString& StableFunctionPath)
{
	int32 ColonIndex = INDEX_NONE;
	if (StableFunctionPath.FindLastChar(TEXT(':'), ColonIndex)
		&& ColonIndex > 0
		&& ColonIndex < StableFunctionPath.Len() - 1)
	{
		return StableFunctionPath.Mid(ColonIndex + 1).TrimStartAndEnd();
	}
	return StableFunctionPath.TrimStartAndEnd();
}

static FString ContainerActionPermittedNodeClassPaths()
{
	return TEXT("/Script/BlueprintGraph.K2Node_CallFunction;/Script/BlueprintGraph.K2Node_CallArrayFunction");
}

static void ApplyContainerActionRequestOverrides(
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest,
	FBlueprintHelperActionResolutionRequest& InOutRequest)
{
	InOutRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
	InOutRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
	InOutRequest.Semantic.FunctionOperation = TEXT("container_action");
	InOutRequest.Semantic.ContainerKind = NormalizeContainerToken(BoundRequest.ContainerKind);
	InOutRequest.Semantic.ContainerOperation = NormalizeContainerToken(BoundRequest.ContainerOperation);
	if (const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(BoundRequest.ContainerKind, BoundRequest.ContainerOperation))
	{
		InOutRequest.Semantic.Query = ExtractContainerActionFunctionName(Spec->StableUFunctionPath);
		InOutRequest.Semantic.StableId = Spec->StableUFunctionPath;
		InOutRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("container.stable_ufunction_path")) = Spec->StableUFunctionPath;
		InOutRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("function.permitted_node_class_paths")) = ContainerActionPermittedNodeClassPaths();
		InOutRequest.Semantic.DefaultValues.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);
		InOutRequest.Semantic.DefaultValues.Add(TEXT("function.permitted_node_class_paths"), ContainerActionPermittedNodeClassPaths());
		InOutRequest.ContextEvidence.Add(TEXT("container_action_operation_id"), Spec->OperationId);
		InOutRequest.ContextEvidence.Add(TEXT("container_action_kind"), Spec->ContainerKind);
		InOutRequest.ContextEvidence.Add(TEXT("container_action_operation"), Spec->ContainerOperation);
		InOutRequest.ContextEvidence.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);
	}
	if (!BoundRequest.Target.TrimStartAndEnd().IsEmpty())
	{
		InOutRequest.Semantic.TargetPath = BoundRequest.Target.TrimStartAndEnd();
	}

	const FString ElementType = FirstNonEmptyContainerType(BoundRequest.ElementType, BoundRequest.PinType);
	if (!ElementType.IsEmpty())
	{
		InOutRequest.Semantic.ElementType = ElementType;
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("element"), ElementType);
		InOutRequest.Semantic.ContainerElementPinType = MakePinTypeFromCreateToken(ElementType);
		InOutRequest.Semantic.ArgumentPinTypes.Add(TEXT("element"), InOutRequest.Semantic.ContainerElementPinType);
	}

	const FString KeyType = FirstNonEmptyContainerType(BoundRequest.KeyType, BoundRequest.KeyPinType);
	if (!KeyType.IsEmpty())
	{
		InOutRequest.Semantic.KeyType = KeyType;
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("key"), KeyType);
		InOutRequest.Semantic.ContainerKeyPinType = MakePinTypeFromCreateToken(KeyType);
		InOutRequest.Semantic.ArgumentPinTypes.Add(TEXT("key"), InOutRequest.Semantic.ContainerKeyPinType);
	}

	const FString ValueType = FirstNonEmptyContainerType(BoundRequest.ValueType, BoundRequest.ValuePinType);
	if (!ValueType.IsEmpty())
	{
		InOutRequest.Semantic.ValueType = ValueType;
		InOutRequest.Semantic.ArgumentTypes.Add(TEXT("value"), ValueType);
		InOutRequest.Semantic.ContainerValuePinType = MakePinTypeFromCreateToken(ValueType);
		InOutRequest.Semantic.ArgumentPinTypes.Add(TEXT("value"), InOutRequest.Semantic.ContainerValuePinType);
	}
}

static void AddPinAlias(
	const FString& Alias,
	const FString& CanonicalPinName,
	TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
	const bool bOverrideExisting = false)
{
	if (Alias.IsEmpty() || CanonicalPinName.IsEmpty())
	{
		return;
	}
	if (const FBlueprintHelperFragmentPinRef* Canonical = PinMap.Find(CanonicalPinName))
	{
		if (PinMap.Contains(Alias) && !bOverrideExisting)
		{
			return;
		}
		FBlueprintHelperFragmentPinRef AliasRef = *Canonical;
		PinMap.Add(Alias, MoveTemp(AliasRef));
		return;
	}
	for (const TPair<FString, FBlueprintHelperFragmentPinRef>& Pair : PinMap)
	{
		if (Pair.Key.Equals(CanonicalPinName, ESearchCase::IgnoreCase))
		{
			if (PinMap.Contains(Alias) && !bOverrideExisting)
			{
				return;
			}
			FBlueprintHelperFragmentPinRef AliasRef = Pair.Value;
			PinMap.Add(Alias, MoveTemp(AliasRef));
			return;
		}
	}
}

static void ApplyContainerActionRolePinAliases(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& InOutFragment)
{
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Request.ContainerKind, Request.ContainerOperation);
	if (!Spec)
	{
		return;
	}
	for (const FBlueprintHelperContainerActionRoleBinding& Binding : Spec->RoleBindings)
	{
		const bool bOutputRole = !Binding.bProjectToCallableRequest;
		AddPinAlias(Binding.RoleName, Binding.FunctionPinName, InOutFragment.PinBindings, true);
		if (bOutputRole)
		{
			AddPinAlias(Binding.RoleName, Binding.FunctionPinName, InOutFragment.DataOutputs, true);
		}
		else
		{
			AddPinAlias(Binding.RoleName, Binding.FunctionPinName, InOutFragment.DataInputs, true);
		}
	}
}

static bool TryBuildLiteralPromotablePinType(const FString& Type, FEdGraphPinType& OutPinType);

static bool TryBuildBasicContainerActionPinType(const FString& Type, FEdGraphPinType& OutPinType)
{
	return TryBuildLiteralPromotablePinType(Type, OutPinType);
}

static void CopyPinTypeToTerminal(const FEdGraphPinType& PinType, FEdGraphTerminalType& OutTerminalType)
{
	OutTerminalType.TerminalCategory = PinType.PinCategory;
	OutTerminalType.TerminalSubCategory = PinType.PinSubCategory;
	OutTerminalType.TerminalSubCategoryObject = PinType.PinSubCategoryObject;
}

static FString ContainerActionElementType(const FBlueprintHelperGraphFragmentBuildRequest& Request)
{
	return FirstNonEmptyContainerType(Request.ElementType, Request.PinType);
}

static bool TryBuildContainerActionTargetPinType(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FEdGraphPinType& OutPinType)
{
	const FString Kind = NormalizeContainerToken(Request.ContainerKind);
	if (Kind == TEXT("array") || Kind == TEXT("set"))
	{
		if (!TryBuildBasicContainerActionPinType(ContainerActionElementType(Request), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = Kind == TEXT("array") ? EPinContainerType::Array : EPinContainerType::Set;
		return true;
	}
	if (Kind == TEXT("map"))
	{
		if (!TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.KeyType, Request.KeyPinType), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = EPinContainerType::Map;

		FEdGraphPinType ValuePinType;
		if (TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.ValueType, Request.ValuePinType), ValuePinType))
		{
			CopyPinTypeToTerminal(ValuePinType, OutPinType.PinValueType);
		}
		return true;
	}
	return false;
}

static bool TryBuildContainerActionReturnPinType(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FEdGraphPinType& OutPinType);

static bool TryBuildContainerActionRolePinType(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FString& RoleName,
	FEdGraphPinType& OutPinType)
{
	const FString Role = NormalizeContainerToken(RoleName);
	if (Role == TEXT("target"))
	{
		return TryBuildContainerActionTargetPinType(Request, OutPinType);
	}
	if (Role == TEXT("item") || Role == TEXT("value"))
	{
		return TryBuildBasicContainerActionPinType(
			Role == TEXT("value")
				? FirstNonEmptyContainerType(Request.ValueType, Request.ValuePinType)
				: ContainerActionElementType(Request),
			OutPinType);
	}
	if (Role == TEXT("items"))
	{
		if (!TryBuildBasicContainerActionPinType(ContainerActionElementType(Request), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Role == TEXT("key"))
	{
		return TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.KeyType, Request.KeyPinType), OutPinType);
	}
	if (Role == TEXT("index"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Role == TEXT("result"))
	{
		return TryBuildContainerActionReturnPinType(Request, OutPinType);
	}
	return false;
}

static bool TryBuildContainerActionReturnPinType(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FEdGraphPinType& OutPinType)
{
	const FString Kind = NormalizeContainerToken(Request.ContainerKind);
	const FString Operation = NormalizeContainerToken(Request.ContainerOperation);
	if (Operation == TEXT("contains"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Operation == TEXT("length") || (Kind == TEXT("array") && Operation == TEXT("find")))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Kind == TEXT("array") && Operation == TEXT("get"))
	{
		return TryBuildBasicContainerActionPinType(ContainerActionElementType(Request), OutPinType);
	}
	if (Kind == TEXT("map") && Operation == TEXT("find"))
	{
		return TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.ValueType, Request.ValuePinType), OutPinType);
	}
	if (Kind == TEXT("map") && Operation == TEXT("keys"))
	{
		if (!TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.KeyType, Request.KeyPinType), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Kind == TEXT("map") && Operation == TEXT("values"))
	{
		if (!TryBuildBasicContainerActionPinType(FirstNonEmptyContainerType(Request.ValueType, Request.ValuePinType), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Kind == TEXT("set") && Operation == TEXT("to_array"))
	{
		if (!TryBuildBasicContainerActionPinType(ContainerActionElementType(Request), OutPinType))
		{
			return false;
		}
		OutPinType.ContainerType = EPinContainerType::Array;
		return true;
	}
	return false;
}

static void ApplyPinTypeToNodePin(UEdGraphPin* Pin, const FEdGraphPinType& PinType)
{
	if (!Pin || PinType.PinCategory.IsNone())
	{
		return;
	}
	Pin->PinType = PinType;
}

static void ApplyContainerActionResolvedPinTypesToNode(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	UK2Node* Node)
{
	if (!Node)
	{
		return;
	}

	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Request.ContainerKind, Request.ContainerOperation);
	if (!Spec)
	{
		return;
	}

	for (const FBlueprintHelperContainerActionRoleBinding& Binding : Spec->RoleBindings)
	{
		FEdGraphPinType PinType;
		if (TryBuildContainerActionRolePinType(Request, Binding.RoleName, PinType))
		{
			ApplyPinTypeToNodePin(Node->FindPin(FName(*Binding.FunctionPinName)), PinType);
		}
	}

	if (Spec->ResultKind == EBlueprintHelperContainerActionResultKind::ReturnValue)
	{
		FEdGraphPinType ReturnPinType;
		if (TryBuildContainerActionReturnPinType(Request, ReturnPinType))
		{
			ApplyPinTypeToNodePin(Node->FindPin(TEXT("ReturnValue")), ReturnPinType);
		}
	}
}

static void ApplyContainerActionResolvedPinTypes(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& InOutFragment)
{
	ApplyContainerActionResolvedPinTypesToNode(Request, InOutFragment.PrimaryNode);
}

static void ApplyContainerActionRoleBindingDefaults(
	FBlueprintHelperGraphFragmentBuildRequest& InOutRequest)
{
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(InOutRequest.ContainerKind, InOutRequest.ContainerOperation);
	if (!Spec)
	{
		return;
	}
	for (const FBlueprintHelperContainerActionRoleBinding& Binding : Spec->RoleBindings)
	{
		if (const FString* DefaultValue = InOutRequest.DefaultValues.Find(Binding.RoleName))
		{
			InOutRequest.DefaultValues.FindOrAdd(Binding.FunctionPinName, *DefaultValue);
		}
		if (const FString* ArgumentType = InOutRequest.ArgumentTypes.Find(Binding.RoleName))
		{
			InOutRequest.ArgumentTypes.FindOrAdd(Binding.FunctionPinName, *ArgumentType);
		}
		if (const FBlueprintHelperCallFunctionPinType* ArgumentPinType = InOutRequest.ArgumentPinTypes.Find(Binding.RoleName))
		{
			InOutRequest.ArgumentPinTypes.FindOrAdd(Binding.FunctionPinName, *ArgumentPinType);
		}
	}
}

static void FillExpressionMapDefaultsAndTypes(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
	FBlueprintHelperGraphFragmentBuildRequest& InOutRequest)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Args)
	{
		if (!ArgPair.Value.IsValid())
		{
			continue;
		}
		if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			InOutRequest.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
		}
		if (!ArgPair.Value->Type.TrimStartAndEnd().IsEmpty())
		{
			InOutRequest.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type.TrimStartAndEnd());
		}
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
	case EBlueprintHelperGraphExpressionKind::ContainerAction:
		return EBlueprintHelperActionSemanticKind::ContainerAction;
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

static bool TryBuildCreateElementPinType(const FString& Type, FEdGraphPinType& OutPinType)
{
	const FBlueprintHelperCallFunctionPinType ParsedType = MakePinTypeFromCreateToken(Type);
	const FString ElementType = ParsedType.Category.IsEmpty() ? Type.TrimStartAndEnd() : ParsedType.Category;
	return TryBuildLiteralPromotablePinType(ElementType, OutPinType);
}

static void ApplyMakeContainerCreatePinType(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	UK2Node* Node)
{
	UK2Node_MakeContainer* MakeContainer = Cast<UK2Node_MakeContainer>(Node);
	if (!MakeContainer)
	{
		return;
	}

	const FString Operation = NormalizeContainerToken(Request.CreateOperation);
	if (Operation != TEXT("make_array") && Operation != TEXT("make_set"))
	{
		return;
	}

	FEdGraphPinType ElementPinType;
	if (!TryBuildCreateElementPinType(FirstNonEmptyContainerType(Request.ElementType, Request.PinType), ElementPinType))
	{
		return;
	}

	UEdGraphPin* OutputPin = MakeContainer->GetOutputPin();
	const EPinContainerType OutputContainerType = OutputPin ? OutputPin->PinType.ContainerType : EPinContainerType::None;
	if (OutputPin)
	{
		OutputPin->PinType = ElementPinType;
		OutputPin->PinType.ContainerType = OutputContainerType == EPinContainerType::None
			? (Operation == TEXT("make_array") ? EPinContainerType::Array : EPinContainerType::Set)
			: OutputContainerType;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (UEdGraphPin* Pin : MakeContainer->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input || Pin->ParentPin)
		{
			continue;
		}

		Pin->PinType = ElementPinType;
		if (Schema)
		{
			Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
		}
	}

	MakeContainer->NodeConnectionListChanged();
	if (UEdGraph* Graph = MakeContainer->GetGraph())
	{
		Graph->NotifyNodeChanged(MakeContainer);
	}
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

static void ApplyExpressionNodePolicies(
	UK2Node* Node,
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	ApplyPromotableOperatorLiteralTypes(Node, Expression);

	if (Expression.Kind != EBlueprintHelperGraphExpressionKind::Convert
		|| !Expression.TransformOperation.Equals(TEXT("dynamic_cast"), ESearchCase::IgnoreCase))
	{
		return;
	}

	if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
	{
		CastNode->SetPurity(true);
	}
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

static bool IsSelfReceiverExpression(const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString FieldScope = Expression.FieldScope.IsEmpty() ? TEXT("variable") : Expression.FieldScope;
	return Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		&& FieldScope.Equals(TEXT("variable"), ESearchCase::IgnoreCase)
		&& (Expression.Target.Equals(TEXT("self"), ESearchCase::IgnoreCase)
			|| Expression.ResolvedTarget.Member.Equals(TEXT("self"), ESearchCase::IgnoreCase));
}

static bool BuildSelfReceiverExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (!TargetGraph)
	{
		OutError = TEXT("self receiver fragment build failed: target graph is null.");
		return false;
	}

	UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(TargetGraph);
	if (!SelfNode)
	{
		OutError = TEXT("self receiver fragment build failed: could not allocate UK2Node_Self.");
		return false;
	}

	TargetGraph->Modify();
	TargetGraph->AddNode(SelfNode, true, false);
	SelfNode->CreateNewGuid();
	SelfNode->SetFlags(RF_Transactional);
	SelfNode->PostPlacedNewNode();
	SelfNode->AllocateDefaultPins();

	OutFragment = FBlueprintHelperNodeFragment();
	OutFragment.FragmentId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SelfNode;
	OutFragment.Nodes.Add(SelfNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SelfNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("self_receiver"));
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
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

bool FBlueprintHelperGraphStatementBuilder::ValidateCallFunctionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FString& OutError,
	FString* OutResolvedStableId,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
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

	return FBlueprintHelperActionFragmentSpawnCoordinator::ValidateResolvedActionFragment(
		CoordinatorRequest,
		OutError,
		OutResolvedStableId,
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
	const FString CreateQuery = !Request.Query.TrimStartAndEnd().IsEmpty()
		? Request.Query.TrimStartAndEnd()
		: Request.CreateOperation;
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		Request.ActionContextStatementId.IsEmpty()
			? Request.FragmentId
			: Request.ActionContextStatementId,
		EBlueprintHelperActionSemanticKind::Create,
		CreateQuery,
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
	SpawnOptions.NodeConfigurationHook =
		[CreateOperation = Request.CreateOperation, ClassPath = Request.ClassPath](
			UK2Node& SpawnedNode,
			const FBlueprintHelperActionNodeSpawnContext&,
			FString& HookError)
	{
		return ConfigureClassBackedCreateNode(SpawnedNode, CreateOperation, ClassPath, HookError);
	};
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
	ApplyMakeContainerCreatePinType(Request, SpawnedNode);
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
	FBlueprintHelperGraphFragmentBuildRequest ContainerBoundRequest;
	const FBlueprintHelperGraphFragmentBuildRequest* EffectiveRequest = &Request;
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		ContainerBoundRequest = Request;
		ApplyContainerActionRoleBindingDefaults(ContainerBoundRequest);
		EffectiveRequest = &ContainerBoundRequest;
	}
	const FBlueprintHelperGraphFragmentBuildRequest& BoundRequest = *EffectiveRequest;

	FBlueprintHelperActionResolutionRequest ActionRequest;
	TArray<FString> ArgumentNames;
	BoundRequest.DefaultValues.GetKeys(ArgumentNames);
	if (!TryBuildProjectedActionRequestFromContext(
		TargetGraph,
		ActionContextScope,
		BoundRequest.ActionContextStatementId.IsEmpty()
			? BoundRequest.FragmentId
			: BoundRequest.ActionContextStatementId,
		SemanticKind,
		BoundRequest.Query,
		BoundRequest.Target,
		BoundRequest.PropertyPath,
		BoundRequest.TypeName,
		BoundRequest.SearchMode,
		BoundRequest.AmbiguityPolicy,
		BoundRequest.CategoryPriority,
		ArgumentNames,
		ActionRequest,
		OutError))
	{
		return false;
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		ApplyContainerActionRequestOverrides(BoundRequest, ActionRequest);
	}
	ActionRequest.ContextEvidence.Append(Request.ContextEvidence);

	FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
	CoordinatorRequest.TargetGraph = TargetGraph;
	CoordinatorRequest.BuildRequest = &Request;
	if (EffectiveRequest != &Request)
	{
		CoordinatorRequest.BuildRequest = EffectiveRequest;
	}
	CoordinatorRequest.ActionRequest = ActionRequest;
	CoordinatorRequest.SemanticKind = SemanticKind;
	CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::ActionProvider;
	CoordinatorRequest.CandidateGroupTarget = BoundRequest.Query;
	CoordinatorRequest.FailurePrefix = FString::Printf(
		TEXT("action provider unavailable: semantic=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
	CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		CoordinatorRequest.SpawnOptions.PinNormalizationHook =
			[ContainerRequest = BoundRequest](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&)
			{
				ApplyContainerActionResolvedPinTypesToNode(ContainerRequest, &SpawnedNode);
			};
	}

	const bool bBuilt = FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
		CoordinatorRequest,
		OutFragment,
		OutError,
		nullptr);
	if (bBuilt && SemanticKind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		const FString ScheduleOperation = NormalizeScheduleOperationToken(FirstNonEmptyString(
			BoundRequest.ScheduleOperation,
			ActionRequest.Semantic.ScheduleOperation,
			ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("schedule_operation"))));
		AddOwnershipTagIfPresent(OutFragment, TEXT("schedule_operation"), ScheduleOperation);

		if (IsTimerDelegateScheduleOperation(ScheduleOperation))
		{
			FBlueprintHelperDelegateLinkRequest LinkRequest;
			LinkRequest.FragmentId = BoundRequest.FragmentId;
			LinkRequest.HandlerName = ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("handler_name"));
			LinkRequest.DiagnosticPrefix = TEXT("timer_delegate");
			LinkRequest.DelegateInputPinName = FirstNonEmptyString(
				ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("schedule_delegate_pin_name")),
				ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("delegate_pin_name")));
			LinkRequest.CreateDelegateLocation = FVector2D(BoundRequest.Location.X + 240.0, BoundRequest.Location.Y);
			if (!FBlueprintHelperDelegateLinkFragmentUtils::AttachCreateDelegateToPrimary(
				TargetGraph,
				OutFragment.PrimaryNode,
				LinkRequest,
				OutFragment,
				OutError))
			{
				return false;
			}

			AddOwnershipTagIfPresent(OutFragment, TEXT("handler_name"), LinkRequest.HandlerName);
			AddOwnershipTagIfPresent(
				OutFragment,
				TEXT("handler_source_cluster"),
				ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("handler_source_cluster")));
			AddOwnershipTagIfPresent(
				OutFragment,
				TEXT("signature_evidence_id"),
				ContextEvidenceValue(ActionRequest.ContextEvidence, TEXT("signature_evidence_id")));
		}
	}
	if (bBuilt && SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		ApplyContainerActionRolePinAliases(BoundRequest, OutFragment);
		ApplyContainerActionResolvedPinTypes(BoundRequest, OutFragment);
		OutFragment.OwnershipTags.Add(TEXT("container_kind"), NormalizeContainerToken(BoundRequest.ContainerKind));
		OutFragment.OwnershipTags.Add(TEXT("container_operation"), NormalizeContainerToken(BoundRequest.ContainerOperation));
	}
	return bBuilt;
}

bool FBlueprintHelperGraphStatementBuilder::BuildFieldCapabilityFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	const FBlueprintHelperActionContextScope* ActionContextScope)
{
	OutFragment = FBlueprintHelperNodeFragment();

	const FBlueprintHelperFieldCapabilitySpec* Spec =
		FBlueprintHelperFieldCapabilityRegistry::FindById(Request.CapabilityId);
	if (!Spec || !Spec->bFirstClassStatement)
	{
		OutError = Request.CapabilityId.IsEmpty()
			? FString(TEXT("unknown_field_capability"))
			: FString::Printf(TEXT("unknown_field_capability: %s"), *Request.CapabilityId);
		OutFragment.OwnershipTags.Add(TEXT("field.failure_reason"), TEXT("unknown_field_capability"));
		OutFragment.OwnershipTags.Add(TEXT("field.success_claim"), TEXT("false"));
		return false;
	}

	const FString ExpectedNodeFamily = Spec->ExpectedNodeFamily;
	bool bBuilt = false;
	if (ExpectedNodeFamily == TEXT("variable_get")
		|| ExpectedNodeFamily == TEXT("component_variable_get")
		|| ExpectedNodeFamily == TEXT("variable_get_target")
		|| ExpectedNodeFamily == TEXT("variable_set")
		|| ExpectedNodeFamily == TEXT("component_variable_set")
		|| ExpectedNodeFamily == TEXT("variable_set_target"))
	{
		FBlueprintHelperActionResolutionRequest ActionRequest;
		TArray<FString> ArgumentNames;
		Request.DefaultValues.GetKeys(ArgumentNames);
		if (!TryBuildProjectedActionRequestFromContext(
			TargetGraph,
			ActionContextScope,
			Request.ActionContextStatementId.IsEmpty()
				? Request.FragmentId
				: Request.ActionContextStatementId,
			EBlueprintHelperActionSemanticKind::Field,
			Request.Query,
			Request.Target,
			Request.PropertyPath,
			Request.ExpectedReturnType,
			Request.SearchMode,
			Request.AmbiguityPolicy,
			Request.CategoryPriority,
			ArgumentNames,
			ActionRequest,
			OutError,
			Spec->FieldOperation,
			Spec->FieldScope))
		{
			OutFragment.OwnershipTags.Add(TEXT("field.capability_id"), Spec->Id);
			OutFragment.OwnershipTags.Add(TEXT("field.failure_reason"), OutError);
			OutFragment.OwnershipTags.Add(TEXT("field.success_claim"), TEXT("false"));
			return false;
		}
		ApplyFieldCapabilityActionRequestOverrides(Request, ActionRequest);
		ActionRequest.ContextEvidence.Append(Request.ContextEvidence);

		FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
		CoordinatorRequest.TargetGraph = TargetGraph;
		CoordinatorRequest.BuildRequest = &Request;
		CoordinatorRequest.ActionRequest = ActionRequest;
		CoordinatorRequest.SemanticKind = EBlueprintHelperActionSemanticKind::Field;
		CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::ActionProvider;
		CoordinatorRequest.CandidateGroupTarget = Request.Query;
		CoordinatorRequest.FailurePrefix = FString::Printf(TEXT("field capability resolve failed: %s"), *Spec->Id);
		CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;

		bBuilt = FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
			CoordinatorRequest,
			OutFragment,
			OutError,
			nullptr);
	}
	else
	{
		FBlueprintHelperFieldFragmentPlan Plan;
		Plan.CapabilityId = Spec->Id;
		Plan.FieldName = Request.Query;
		Plan.CapabilityFacts = Request.CapabilityFacts;
		if (ExpectedNodeFamily == TEXT("break_struct"))
		{
			bBuilt = FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(TargetGraph, Plan, OutFragment, OutError);
		}
		else if (ExpectedNodeFamily == TEXT("set_fields_in_struct"))
		{
			bBuilt = FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(TargetGraph, Plan, OutFragment, OutError);
		}
		else if (ExpectedNodeFamily == TEXT("property_path_fragment"))
		{
			bBuilt = FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(TargetGraph, Plan, OutFragment, OutError);
		}
		else
		{
			OutError = FString::Printf(TEXT("unsupported_field_node_family: %s"), *ExpectedNodeFamily);
		}
	}

	if (!bBuilt)
	{
		OutFragment.OwnershipTags.Add(TEXT("field.capability_id"), Spec->Id);
		OutFragment.OwnershipTags.Add(TEXT("field.failure_reason"), OutError);
		OutFragment.OwnershipTags.Add(TEXT("field.success_claim"), TEXT("false"));
		return false;
	}

	OutFragment.OwnershipTags.Add(TEXT("field.capability_id"), Spec->Id);
	OutFragment.OwnershipTags.Add(TEXT("field.success_claim"), TEXT("true"));
	for (UEdGraphNode* Node : OutFragment.Nodes)
	{
		FBlueprintHelperFieldActionReadback Readback =
			FBlueprintHelperFieldActionReadbackCollector::CollectFromNode(Spec->Id, Node);
		TMap<FString, FString> FlatFacts;
		Readback.AppendFlatFacts(FlatFacts);
		for (const TPair<FString, FString>& FactPair : FlatFacts)
		{
			if (!FactPair.Key.IsEmpty() && !FactPair.Value.TrimStartAndEnd().IsEmpty())
			{
				OutFragment.OwnershipTags.FindOrAdd(FactPair.Key, FactPair.Value.TrimStartAndEnd());
			}
		}
	}
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

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::ContainerAction)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromExpression(Expression);
		Request.FragmentId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		Request.SourceStatementId = Expression.ExpressionId;
		Request.ActionContextStatementId = MakeExpressionActionContextStatementId(Expression);
		Request.Query = NormalizeContainerToken(Expression.ContainerKind) + TEXT(".") + NormalizeContainerToken(Expression.ContainerOperation);
		Request.Target = Expression.TargetObject.IsValid()
			? (!Expression.TargetObject->Target.IsEmpty() ? Expression.TargetObject->Target : Expression.TargetObject->Name)
			: Expression.Target;
		Request.TypeName = Expression.Type;
		Request.ExpectedReturnType = Expression.Type;
		FillExpressionMapDefaultsAndTypes(Expression.Args, Request);
		if (!BuildActionProviderFragment(TargetGraph, Request, EBlueprintHelperActionSemanticKind::ContainerAction, OutFragment, OutError, ActionContextScope))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ResolveActionSemanticKindForExpressionKind(Expression.Kind);
	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Field)
	{
		if (IsSelfReceiverExpression(Expression))
		{
			return BuildSelfReceiverExpressionFragment(TargetGraph, Expression, OutFragment, OutError);
		}

		if (!Expression.CapabilityId.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromExpression(Expression);
			Request.FragmentId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
			Request.SourceStatementId = Expression.ExpressionId;
			Request.ActionContextStatementId = MakeExpressionActionContextStatementId(Expression);
			Request.Query = !Expression.Property.IsEmpty() ? Expression.Property : Expression.Target;
			Request.Target = !Expression.ResolvedTarget.Raw.IsEmpty() ? Expression.ResolvedTarget.Raw : Request.Query;
			Request.PropertyPath = !Expression.ResolvedTarget.PropertyPath.IsEmpty() ? Expression.ResolvedTarget.PropertyPath : Expression.Property;
			Request.ExpectedReturnType = Expression.Type;
			if (!BuildFieldCapabilityFragment(TargetGraph, Request, OutFragment, OutError, ActionContextScope))
			{
				return false;
			}
			OutFragment.SourceStatementId = Expression.ExpressionId;
			OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
			OutFragment.ReviewTargets.Add(Expression.ExpressionId);
			return true;
		}

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
			ApplyExpressionNodePolicies(&SpawnedNode, Expression);
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
