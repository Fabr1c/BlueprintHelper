#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphComposerUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/PromotableOperatorNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SelectNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SequenceNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/StructOperationNodeHandler.h"

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

static void PopulateCommonFragmentMetadata(const FParsedNode& NodeData, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), NodeData.Id);
	OutFragment.ReviewTargets.Add(NodeData.Id);
	// DEPRECATED_LAYOUT: these x/y hints are legacy spawn metadata only.
	// Final node positions must come from the UE-side GraphLayout system.
	OutFragment.LayoutHints.Add(TEXT("x"), LexToString(NodeData.X));
	OutFragment.LayoutHints.Add(TEXT("y"), LexToString(NodeData.Y));
}

static void ApplyCallPatternBindings(FParsedNode& NodeData)
{
	FBlueprintHelperGraphPatternRegistry& Registry = FBlueprintHelperGraphPatternRegistry::Get();

	FString ObjectName;
	FString FunctionName;
	if (FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(NodeData.FunctionName, ObjectName, FunctionName))
	{
		FunctionName = Registry.ResolveAlias(TEXT("call"), FunctionName);
		NodeData.FunctionName = ObjectName + TEXT(".") + FunctionName;
	}
	else
	{
		NodeData.FunctionName = Registry.ResolveAlias(TEXT("call"), NodeData.FunctionName);
	}

	Registry.ApplyPinAliases(TEXT("call"), NodeData.DefaultValues);
	Registry.ApplyPinAliases(TEXT("call"), NodeData.ArgumentTypes);
}

static void ApplyCallPatternDefaults(FParsedNode& NodeData)
{
	FBlueprintHelperGraphPatternRegistry::Get().ApplyDefaults(TEXT("call"), NodeData.DefaultValues);
}

static void PopulateK2CallContext(
	FBlueprintHelperCallFunctionResolveRequest& Request,
	UEdGraph* TargetGraph)
{
	Request.Context.Blueprint = Request.Blueprint;
	Request.Context.Graph = Request.Graph;
	Request.Context.Schema = TargetGraph ? TargetGraph->GetSchema() : nullptr;
	Request.Context.SelfClass = Request.Blueprint
		? (Request.Blueprint->GeneratedClass ? Request.Blueprint->GeneratedClass.Get() : Request.Blueprint->SkeletonGeneratedClass.Get())
		: nullptr;
	Request.Context.GraphKind = TargetGraph && TargetGraph->GetClass() ? TargetGraph->GetClass()->GetName() : FString();
	Request.Context.ArgumentNames = Request.ArgumentNames;
	Request.Context.ArgumentTypes = Request.ArgumentTypes;
	Request.Context.ArgumentPinTypes = Request.ArgumentPinTypes;
	Request.Context.TargetObjectType = Request.TargetObjectType;
	Request.Context.TargetObjectPinType = Request.TargetObjectPinType;
}

static bool TryParseExplicitObjectCall(
	const FBlueprintHelperCallFunctionResolveResult& ResolveResult,
	const FString& FunctionQuery,
	FString& OutObjectName,
	FString& OutFunctionName)
{
	OutObjectName.Reset();
	OutFunctionName.Reset();

	if (!ResolveResult.ErrorCode.Equals(TEXT("explicit_member_call_not_supported"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (!FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(FunctionQuery, OutObjectName, OutFunctionName))
	{
		return false;
	}

	OutObjectName.TrimStartAndEndInline();
	OutFunctionName.TrimStartAndEndInline();
	return !OutObjectName.IsEmpty()
		&& !OutFunctionName.IsEmpty()
		&& !OutObjectName.StartsWith(TEXT("/Script/"));
}

static UEdGraphPin* FindFirstDataOutputPin(UK2Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindFirstDataInputPin(UK2Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static bool IsObjectLikeInputPin(const UEdGraphPin* Pin)
{
	return Pin
		&& Pin->Direction == EGPD_Input
		&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
		&& (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface);
}

static bool CanConnectObjectOutputToTargetPin(const UEdGraphPin* ObjectOutputPin, const UEdGraphPin* TargetPin)
{
	if (!ObjectOutputPin || !TargetPin)
	{
		return false;
	}

	const UEdGraph* Graph = TargetPin->GetOwningNode() ? TargetPin->GetOwningNode()->GetGraph() : nullptr;
	const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
	if (!Schema)
	{
		return false;
	}

	return Schema->CanCreateConnection(ObjectOutputPin, TargetPin).Response != CONNECT_RESPONSE_DISALLOW;
}

static UEdGraphPin* FindCallTargetPin(UK2Node* CallNode, UEdGraphPin* ObjectOutputPin = nullptr)
{
	if (!CallNode)
	{
		return nullptr;
	}

	if (ObjectOutputPin)
	{
		for (UEdGraphPin* Pin : CallNode->Pins)
		{
			if (IsObjectLikeInputPin(Pin) && CanConnectObjectOutputToTargetPin(ObjectOutputPin, Pin))
			{
				return Pin;
			}
		}
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (IsObjectLikeInputPin(Pin))
		{
			return Pin;
		}
	}

	const TCHAR* AliasCandidates[] = {
		TEXT("self"),
		TEXT("target"),
		TEXT("Target"),
		TEXT("object"),
		TEXT("Object"),
	};
	for (const TCHAR* Alias : AliasCandidates)
	{
		if (UEdGraphPin* Pin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, Alias))
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				if (ObjectOutputPin && !CanConnectObjectOutputToTargetPin(ObjectOutputPin, Pin))
				{
					continue;
				}
				return Pin;
			}
		}
	}
	return nullptr;
}

static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromPin(const UEdGraphPin* Pin)
{
	FBlueprintHelperCallFunctionPinType Result;
	if (!Pin)
	{
		return Result;
	}

	Result.Category = Pin->PinType.PinCategory.ToString();
	Result.SubCategory = Pin->PinType.PinSubCategory.ToString();
	if (Pin->PinType.PinSubCategoryObject.IsValid())
	{
		Result.ObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
	}
	Result.bIsReference = Pin->PinType.bIsReference;
	Result.bIsConst = Pin->PinType.bIsConst;
	return Result;
}

static bool TryConnectDataPins(
	UEdGraph* TargetGraph,
	UEdGraphPin* FromPin,
	UEdGraphPin* ToPin,
	const FString& Context,
	FString& OutError)
{
	if (!FromPin || !ToPin)
	{
		OutError = FString::Printf(TEXT("%s failed: source or target pin is invalid."), *Context);
		return false;
	}

	FString FailureReason;
	if (FBlueprintHelperGraphComposerUtils::TryCreateSchemaDataConnection(FromPin, ToPin, FailureReason)
		&& ToPin->LinkedTo.Num() > 0)
	{
		return true;
	}

	OutError = FailureReason.IsEmpty()
		? FString::Printf(TEXT("%s rejected: %s -> %s."), *Context, *FromPin->PinName.ToString(), *ToPin->PinName.ToString())
		: FString::Printf(TEXT("%s rejected: %s"), *Context, *FailureReason);
	return false;
}

static void AppendCandidateFunctionGroup(
	const FString& Target,
	const FBlueprintHelperCallFunctionResolveResult& ResolveResult,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (!OutCandidateFunctions || ResolveResult.CandidateFunctions.Num() == 0)
	{
		return;
	}

	FBlueprintHelperCandidateFunctionGroup Group;
	Group.Target = Target;
	Group.Candidates = ResolveResult.CandidateFunctions;
	OutCandidateFunctions->Add(MoveTemp(Group));
}

static bool SpawnExplicitObjectCallFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	const FString& ObjectName,
	const FString& FunctionName,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	FParsedNode BoundNodeData = NodeData;

	FParsedNode ObjectGetterData;
	ObjectGetterData.Id = BoundNodeData.Id + TEXT("_target");
	ObjectGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
	ObjectGetterData.SourceType = TEXT("K2Node_VariableGet");
	ObjectGetterData.X = BoundNodeData.X - 260.0f;
	ObjectGetterData.Y = BoundNodeData.Y;
	ObjectGetterData.VariableReference.ScopeType = TEXT("member");
	ObjectGetterData.VariableReference.VariableName = ObjectName;
	ObjectGetterData.VariableReference.bSelfContext = true;

	FString ObjectGetterError;
	UK2Node* ObjectGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, ObjectGetterData, ObjectGetterError);
	if (!ObjectGetterNode)
	{
		OutError = ObjectGetterError.IsEmpty()
			? FString::Printf(TEXT("explicit object call target not found: %s"), *ObjectName)
			: ObjectGetterError;
		return false;
	}

	UEdGraphPin* ObjectOutputPin = FindFirstDataOutputPin(ObjectGetterNode);
	if (!ObjectOutputPin)
	{
		ObjectGetterNode->DestroyNode();
		OutError = FString::Printf(TEXT("explicit object call target output pin not found: %s"), *ObjectName);
		return false;
	}

	FBlueprintHelperCallFunctionResolveRequest ObjectCallRequest;
	ObjectCallRequest.Graph = TargetGraph;
	ObjectCallRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ObjectCallRequest.Query = FunctionName;
	ObjectCallRequest.SearchMode = BoundNodeData.SearchMode;
	ObjectCallRequest.AmbiguityPolicy = BoundNodeData.AmbiguityPolicy;
	ObjectCallRequest.CategoryPriority = BoundNodeData.CategoryPriority;
	ObjectCallRequest.ArgumentTypes = BoundNodeData.ArgumentTypes;
	ObjectCallRequest.ArgumentPinTypes = BoundNodeData.ArgumentPinTypes;
	ObjectCallRequest.TargetObjectType = BoundNodeData.TargetObjectType;
	ObjectCallRequest.TargetObjectPinType = MakeCallFunctionPinTypeFromPin(ObjectOutputPin);
	BoundNodeData.DefaultValues.GetKeys(ObjectCallRequest.ArgumentNames);
	PopulateK2CallContext(ObjectCallRequest, TargetGraph);
	const FBlueprintHelperCallFunctionResolveResult ObjectCallResolveResult =
		FBlueprintHelperCallFunctionResolver::Resolve(ObjectCallRequest);
	if (!ObjectCallResolveResult.IsResolved())
	{
		ObjectGetterNode->DestroyNode();
		AppendCandidateFunctionGroup(FunctionName, ObjectCallResolveResult, OutCandidateFunctions);
		OutError = ObjectCallResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("explicit object call resolve failed: %s"), *FunctionName)
			: ObjectCallResolveResult.Message;
		return false;
	}

	UK2Node* CallNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ObjectCallResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);
	if (!CallNode)
	{
		ObjectGetterNode->DestroyNode();
		return false;
	}

	ApplyCallPatternDefaults(BoundNodeData);
	FBlueprintGraphWriteFacade::ApplyDefaultValues(CallNode, BoundNodeData.DefaultValues, BoundNodeData.Id);

	UEdGraphPin* CallTargetPin = FindCallTargetPin(CallNode, ObjectOutputPin);
	if (!ObjectOutputPin || !CallTargetPin)
	{
		OutError = FString::Printf(
			TEXT("explicit object call target pin not found: %s.%s"),
			*ObjectName,
			*FunctionName);
		return false;
	}

	FString ConnectionFailureReason;
	if (!FBlueprintHelperGraphComposerUtils::TryCreateSchemaDataConnection(ObjectOutputPin, CallTargetPin, ConnectionFailureReason)
		|| CallTargetPin->LinkedTo.Num() == 0)
	{
		OutError = ConnectionFailureReason.IsEmpty()
			? FString::Printf(TEXT("explicit object call connection rejected: %s.%s"), *ObjectName, *FunctionName)
			: FString::Printf(TEXT("explicit object call connection rejected: %s"), *ConnectionFailureReason);
		return false;
	}

	OutFragment.FragmentId = BoundNodeData.Id;
	OutFragment.SourceStatementId = BoundNodeData.Id;
	OutFragment.PrimaryNode = CallNode;
	OutFragment.Nodes.Add(ObjectGetterNode);
	OutFragment.Nodes.Add(CallNode);
	PopulateCallFragmentPins(CallNode, OutFragment);
	PopulateCommonFragmentMetadata(BoundNodeData, OutFragment);
	OutFragment.DataInputs.Add(ObjectName, FBlueprintHelperFragmentPinRef{ ObjectGetterData.Id, ObjectName, TEXT("object"), ObjectOutputPin });
	OutFragment.InternalLinks.Add(FBlueprintHelperFragmentLink{
		FBlueprintHelperFragmentPinRef{ ObjectGetterData.Id, ObjectName, TEXT("object"), ObjectOutputPin },
		FBlueprintHelperFragmentPinRef{ BoundNodeData.Id, TEXT("target"), TEXT("object"), CallTargetPin }
	});
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();
	FParsedNode BoundNodeData = NodeData;
	ApplyCallPatternBindings(BoundNodeData);

	const FString ExplicitTargetObjectName = BoundNodeData.TargetObjectName.TrimStartAndEnd();
	if (!ExplicitTargetObjectName.IsEmpty())
	{
		return SpawnExplicitObjectCallFragment(
			TargetGraph,
			BoundNodeData,
			ExplicitTargetObjectName,
			BoundNodeData.FunctionName,
			OutFragment,
			OutError,
			OutCandidateFunctions);
	}

	FBlueprintHelperCallFunctionResolveRequest ResolveRequest;
	ResolveRequest.Graph = TargetGraph;
	ResolveRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ResolveRequest.Query = BoundNodeData.FunctionName;
	ResolveRequest.SearchMode = BoundNodeData.SearchMode;
	ResolveRequest.AmbiguityPolicy = BoundNodeData.AmbiguityPolicy;
	ResolveRequest.CategoryPriority = BoundNodeData.CategoryPriority;
	ResolveRequest.ArgumentTypes = BoundNodeData.ArgumentTypes;
	ResolveRequest.ArgumentPinTypes = BoundNodeData.ArgumentPinTypes;
	ResolveRequest.TargetObjectType = BoundNodeData.TargetObjectType;
	ResolveRequest.TargetObjectPinType = BoundNodeData.TargetObjectPinType;
	BoundNodeData.DefaultValues.GetKeys(ResolveRequest.ArgumentNames);
	PopulateK2CallContext(ResolveRequest, TargetGraph);
	const FBlueprintHelperCallFunctionResolveResult ResolveResult =
		FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);

	if (!ResolveResult.IsResolved())
	{
		FString ObjectName;
		FString FunctionName;
		if (TryParseExplicitObjectCall(ResolveResult, BoundNodeData.FunctionName, ObjectName, FunctionName))
		{
			return SpawnExplicitObjectCallFragment(TargetGraph, BoundNodeData, ObjectName, FunctionName, OutFragment, OutError, OutCandidateFunctions);
		}

		AppendCandidateFunctionGroup(BoundNodeData.FunctionName, ResolveResult, OutCandidateFunctions);
		OutError = ResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *BoundNodeData.FunctionName)
			: ResolveResult.Message;
		return false;
	}

	ApplyCallPatternDefaults(BoundNodeData);

	UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ResolveResult.Selected,
		FVector2D(BoundNodeData.X, BoundNodeData.Y),
		OutError);

	if (!SpawnedNode)
	{
		return false;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(SpawnedNode, BoundNodeData.DefaultValues, BoundNodeData.Id);

	OutFragment.FragmentId = BoundNodeData.Id;
	OutFragment.SourceStatementId = BoundNodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateCallFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(BoundNodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();

	if (NodeData.VariableReference.IsLocalVariable() && NodeData.VariableReference.bEnsureExists)
	{
		FParsedLocalVariableDeclaration LocalDeclaration;
		LocalDeclaration.Name = NodeData.VariableReference.VariableName;
		LocalDeclaration.PinType = NodeData.VariableReference.PinType;
		LocalDeclaration.DefaultValue = NodeData.VariableReference.DefaultValue;
		LocalDeclaration.bEnsureExists = true;
		FBlueprintGraphWriteFacade::EnsureLocalVariableExists(TargetGraph, LocalDeclaration, OutError);
		if (!OutError.IsEmpty())
		{
			return false;
		}
	}

	UK2Node* SpawnedNode = FBlueprintGraphWriteFacade::SpawnVariableSetNode(TargetGraph, NodeData, OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	UEdGraphPin* ValuePin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, NodeData.VariableReference.VariableName);
	if (!ValuePin)
	{
		ValuePin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("value"));
	}
	OutFragment.DataInputs.Add(NodeData.VariableReference.VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, NodeData.VariableReference.VariableName, TEXT("value"), ValuePin });
	OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), TEXT("value"), ValuePin });
	OutFragment.PinBindings.Add(NodeData.VariableReference.VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, NodeData.VariableReference.VariableName, TEXT("value"), ValuePin });
	OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), TEXT("value"), ValuePin });
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();

	FParsedNode NodeData;
	NodeData.Id = FragmentId.IsEmpty() ? TEXT("semantic_sequence") : FragmentId;
	NodeData.NodeType = EParsedBlueprintNodeType::Sequence;
	NodeData.SourceType = TEXT("K2Node_ExecutionSequence");

	FSequenceNodeHandler Handler;
	UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Get)
	{
		const FString VariableName = !Expression.ResolvedTarget.Member.IsEmpty()
			? Expression.ResolvedTarget.Member
			: Expression.Target;
		if (VariableName.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("get expression fragment build failed: target is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::VariableGet;
		NodeData.SourceType = TEXT("K2Node_VariableGet");
		NodeData.VariableReference.ScopeType = TEXT("member");
		NodeData.VariableReference.VariableName = VariableName;
		NodeData.VariableReference.bSelfContext = true;

		UK2Node* SpawnedNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		OutFragment.DataOutputs.Add(VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, VariableName, Expression.Type, OutputPin });
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, VariableName, Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::GetProperty)
	{
		FString OwnerName = Expression.ResolvedTarget.Owner;
		FString PropertyPath = Expression.ResolvedTarget.PropertyPath;
		if (OwnerName.IsEmpty() || PropertyPath.IsEmpty())
		{
			FString ParsedOwner;
			FString ParsedPath;
			if (Expression.Target.Split(TEXT("."), &ParsedOwner, &ParsedPath))
			{
				OwnerName = OwnerName.IsEmpty() ? ParsedOwner : OwnerName;
				PropertyPath = PropertyPath.IsEmpty() ? ParsedPath : PropertyPath;
			}
		}

		OwnerName.TrimStartAndEndInline();
		PropertyPath.TrimStartAndEndInline();
		if (OwnerName.IsEmpty() || PropertyPath.IsEmpty())
		{
			OutError = TEXT("get_property expression fragment build failed: target must be Owner.PropertyPath.");
			return false;
		}

		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			OutError = TEXT("get_property expression fragment build failed: property path is empty.");
			return false;
		}

		const FString BaseId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		FParsedNode OwnerGetterData;
		OwnerGetterData.Id = BaseId + TEXT("_owner");
		OwnerGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
		OwnerGetterData.SourceType = TEXT("K2Node_VariableGet");
		OwnerGetterData.X = -300.0f;
		OwnerGetterData.Y = 0.0f;
		OwnerGetterData.VariableReference.ScopeType = TEXT("member");
		OwnerGetterData.VariableReference.VariableName = OwnerName;
		OwnerGetterData.VariableReference.bSelfContext = true;

		UK2Node* OwnerGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, OwnerGetterData, OutError);
		if (!OwnerGetterNode)
		{
			return false;
		}

		TArray<UEdGraphNode*> Nodes;
		TArray<FBlueprintHelperFragmentLink> InternalLinks;
		Nodes.Add(OwnerGetterNode);

		UEdGraphPin* CurrentOutputPin = FindFirstDataOutputPin(OwnerGetterNode);
		if (!CurrentOutputPin)
		{
			OutError = FString::Printf(TEXT("get_property expression fragment build failed: owner output pin not found: %s."), *OwnerName);
			return false;
		}

		UK2Node* LastNode = OwnerGetterNode;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FString Segment = Segments[SegmentIndex].TrimStartAndEnd();
			if (Segment.IsEmpty())
			{
				OutError = FString::Printf(TEXT("get_property expression fragment build failed: empty segment in path '%s'."), *PropertyPath);
				return false;
			}

			if (CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				UScriptStruct* StructType = Cast<UScriptStruct>(CurrentOutputPin->PinType.PinSubCategoryObject.Get());
				if (!StructType)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: struct type is missing before segment '%s'."), *Segment);
					return false;
				}

				FParsedNode BreakNodeData;
				BreakNodeData.Id = FString::Printf(TEXT("%s_break_%d"), *BaseId, SegmentIndex);
				BreakNodeData.NodeType = EParsedBlueprintNodeType::BreakStruct;
				BreakNodeData.SourceType = TEXT("K2Node_BreakStruct");
				BreakNodeData.X = static_cast<float>(SegmentIndex * 260);
				BreakNodeData.Y = 0.0f;
				BreakNodeData.StructReference.StructPath = StructType->GetPathName();

				FStructOperationNodeHandler Handler;
				UK2Node* BreakNode = Handler.Spawn(TargetGraph, BreakNodeData, OutError);
				if (!BreakNode)
				{
					return false;
				}

				UEdGraphPin* StructInputPin = FindFirstDataInputPin(BreakNode);
				if (!TryConnectDataPins(TargetGraph, CurrentOutputPin, StructInputPin, TEXT("get_property struct access"), OutError))
				{
					return false;
				}

				UEdGraphPin* SegmentOutputPin = FBlueprintGraphWriteFacade::FindPinByAlias(BreakNode, Segment);
				if (!SegmentOutputPin)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: struct output pin not found: %s.%s."), *StructType->GetName(), *Segment);
					return false;
				}

				InternalLinks.Add(FBlueprintHelperFragmentLink{
					FBlueprintHelperFragmentPinRef{ LastNode->GetName(), CurrentOutputPin->PinName.ToString(), CurrentOutputPin->PinType.PinCategory.ToString(), CurrentOutputPin },
					FBlueprintHelperFragmentPinRef{ BreakNodeData.Id, StructInputPin ? StructInputPin->PinName.ToString() : FString(), StructInputPin ? StructInputPin->PinType.PinCategory.ToString() : FString(), StructInputPin }
				});
				Nodes.Add(BreakNode);
				LastNode = BreakNode;
				CurrentOutputPin = SegmentOutputPin;
				continue;
			}

			if (CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
				|| CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
			{
				UClass* OwnerClass = Cast<UClass>(CurrentOutputPin->PinType.PinSubCategoryObject.Get());
				if (!OwnerClass)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: object class is missing before segment '%s'."), *Segment);
					return false;
				}

				FParsedNode PropertyGetterData;
				PropertyGetterData.Id = FString::Printf(TEXT("%s_prop_%d"), *BaseId, SegmentIndex);
				PropertyGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
				PropertyGetterData.SourceType = TEXT("K2Node_VariableGet");
				PropertyGetterData.X = static_cast<float>(SegmentIndex * 260);
				PropertyGetterData.Y = 0.0f;
				PropertyGetterData.VariableReference.ScopeType = TEXT("member");
				PropertyGetterData.VariableReference.VariableName = Segment;
				PropertyGetterData.VariableReference.OwnerClassPath = OwnerClass->GetPathName();
				PropertyGetterData.VariableReference.bSelfContext = false;

				UK2Node* PropertyGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, PropertyGetterData, OutError);
				if (!PropertyGetterNode)
				{
					return false;
				}

				UEdGraphPin* TargetPin = FindCallTargetPin(PropertyGetterNode, CurrentOutputPin);
				if (!TryConnectDataPins(TargetGraph, CurrentOutputPin, TargetPin, TEXT("get_property object access"), OutError))
				{
					return false;
				}

				UEdGraphPin* SegmentOutputPin = FindFirstDataOutputPin(PropertyGetterNode);
				if (!SegmentOutputPin)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: property output pin not found: %s.%s."), *OwnerClass->GetName(), *Segment);
					return false;
				}

				InternalLinks.Add(FBlueprintHelperFragmentLink{
					FBlueprintHelperFragmentPinRef{ LastNode->GetName(), CurrentOutputPin->PinName.ToString(), CurrentOutputPin->PinType.PinCategory.ToString(), CurrentOutputPin },
					FBlueprintHelperFragmentPinRef{ PropertyGetterData.Id, TargetPin ? TargetPin->PinName.ToString() : FString(), TargetPin ? TargetPin->PinType.PinCategory.ToString() : FString(), TargetPin }
				});
				Nodes.Add(PropertyGetterNode);
				LastNode = PropertyGetterNode;
				CurrentOutputPin = SegmentOutputPin;
				continue;
			}

			OutError = FString::Printf(
				TEXT("get_property expression fragment build failed: unsupported owner pin category '%s' before segment '%s'."),
				*CurrentOutputPin->PinType.PinCategory.ToString(),
				*Segment);
			return false;
		}

		OutFragment.FragmentId = BaseId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = LastNode;
		OutFragment.Nodes = MoveTemp(Nodes);
		OutFragment.InternalLinks = MoveTemp(InternalLinks);
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ BaseId, TEXT("value"), Expression.Type, CurrentOutputPin });
		OutFragment.DataOutputs.Add(PropertyPath, FBlueprintHelperFragmentPinRef{ BaseId, PropertyPath, Expression.Type, CurrentOutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ BaseId, TEXT("value"), Expression.Type, CurrentOutputPin });
		OutFragment.PinBindings.Add(PropertyPath, FBlueprintHelperFragmentPinRef{ BaseId, PropertyPath, Expression.Type, CurrentOutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Literal)
	{
		OutError = TEXT("literal expression fragment build skipped: literals are expected to bind as pin defaults.");
		return false;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Call)
	{
		if (Expression.Target.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("call expression fragment build failed: target is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::CallFunction;
		NodeData.SourceType = TEXT("K2Node_CallFunction");
		NodeData.FunctionName = Expression.Target;
		NodeData.SearchMode = Expression.SearchMode;
		NodeData.AmbiguityPolicy = Expression.AmbiguityPolicy;
		NodeData.CategoryPriority = Expression.CategoryPriority;
		if (Expression.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::ComponentMemberFunction)
		{
			NodeData.TargetObjectType = Expression.ResolvedTarget.Type;
		}
		if (Expression.TargetObject.IsValid())
		{
			NodeData.TargetObjectName = !Expression.TargetObject->ResolvedTarget.Member.IsEmpty()
				? Expression.TargetObject->ResolvedTarget.Member
				: (!Expression.TargetObject->Target.IsEmpty() ? Expression.TargetObject->Target : Expression.TargetObject->Name);
			NodeData.TargetObjectType = Expression.TargetObject->Type;
		}
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (!ArgPair.Value.IsValid())
			{
				continue;
			}
			if (!ArgPair.Value->Type.IsEmpty())
			{
				NodeData.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
			}
			if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
		}

		if (!BuildCallFunctionFragment(TargetGraph, NodeData, OutFragment, OutError, OutCandidateFunctions))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Compare)
	{
		if (Expression.Operator.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("compare expression fragment build failed: operator is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::PromotableOperator;
		NodeData.SourceType = TEXT("K2Node_PromotableOperator");
		NodeData.FunctionName = FBlueprintHelperGraphStatementTypeUtils::ResolveCompareOperatorFunctionName(Expression);
		if (Expression.Left.IsValid() && Expression.Left->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("A"), Expression.Left->LiteralValue);
		}
		if (Expression.Right.IsValid() && Expression.Right->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("B"), Expression.Right->LiteralValue);
		}

		FPromotableOperatorNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* LeftPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("A"));
		UEdGraphPin* RightPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("B"));
		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("A"), Expression.Left.IsValid() ? Expression.Left->Type : FString(), LeftPin });
		OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("B"), Expression.Right.IsValid() ? Expression.Right->Type : FString(), RightPin });
		OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), TEXT("bool"), OutputPin });
		OutFragment.PinBindings.Add(TEXT("A"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("A"), Expression.Left.IsValid() ? Expression.Left->Type : FString(), LeftPin });
		OutFragment.PinBindings.Add(TEXT("B"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("B"), Expression.Right.IsValid() ? Expression.Right->Type : FString(), RightPin });
		OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), TEXT("bool"), OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Select)
	{
		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::Select;
		NodeData.SourceType = TEXT("K2Node_Select");
		NodeData.SelectReference.NumOptions = FMath::Max(2, Expression.Options.Num());
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ConditionExpressionPtr = Expression.Args.Find(TEXT("condition"));
		if (!ConditionExpressionPtr)
		{
			ConditionExpressionPtr = Expression.Args.Find(TEXT("index"));
		}
		const TSharedPtr<FBlueprintHelperGraphExpressionIR> ConditionExpression =
			ConditionExpressionPtr ? *ConditionExpressionPtr : nullptr;
		if (ConditionExpression.IsValid() && ConditionExpression->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("Index"), ConditionExpression->LiteralValue);
		}
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num(); ++OptionIndex)
		{
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression.Options[OptionIndex];
			if (Option.IsValid() && Option->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(FString::Printf(TEXT("Option%d"), OptionIndex), Option->LiteralValue);
			}
		}

		FSelectNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);

		UEdGraphPin* IndexPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("Index"));
		OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("Index"), ConditionExpression.IsValid() ? ConditionExpression->Type : FString(), IndexPin });
		OutFragment.PinBindings.Add(TEXT("Index"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("Index"), ConditionExpression.IsValid() ? ConditionExpression->Type : FString(), IndexPin });
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num(); ++OptionIndex)
		{
			const FString OptionPinName = FString::Printf(TEXT("Option%d"), OptionIndex);
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression.Options[OptionIndex];
			UEdGraphPin* OptionPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, OptionPinName);
			OutFragment.DataInputs.Add(OptionPinName, FBlueprintHelperFragmentPinRef{ NodeData.Id, OptionPinName, Option.IsValid() ? Option->Type : FString(), OptionPin });
			OutFragment.PinBindings.Add(OptionPinName, FBlueprintHelperFragmentPinRef{ NodeData.Id, OptionPinName, Option.IsValid() ? Option->Type : FString(), OptionPin });
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::MakeStruct)
	{
		if (Expression.Type.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("make_struct expression fragment build failed: type is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::MakeStruct;
		NodeData.SourceType = TEXT("K2Node_MakeStruct");
		NodeData.StructReference.StructPath = Expression.Type;

		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (ArgPair.Value.IsValid() && ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
		}

		FStructOperationNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (ArgPair.Value.IsValid() && ArgPair.Value->Kind != EBlueprintHelperGraphExpressionKind::Literal)
			{
				UEdGraphPin* FieldPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, ArgPair.Key);
				OutFragment.DataInputs.Add(ArgPair.Key, FBlueprintHelperFragmentPinRef{ NodeData.Id, ArgPair.Key, ArgPair.Value->Type, FieldPin });
				OutFragment.PinBindings.Add(ArgPair.Key, FBlueprintHelperFragmentPinRef{ NodeData.Id, ArgPair.Key, ArgPair.Value->Type, FieldPin });
			}
		}
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	OutError = FString::Printf(
		TEXT("expression fragment pattern is not implemented yet: %s."),
		*Expression.PatternName);
	return false;
}
