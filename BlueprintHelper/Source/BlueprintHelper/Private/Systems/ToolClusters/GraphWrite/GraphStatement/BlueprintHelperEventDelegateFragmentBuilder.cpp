#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "UObject/UnrealType.h"

namespace
{
static FString GetStatementId(const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!Statement.StatementId.IsEmpty())
	{
		return Statement.StatementId;
	}
	if (!Statement.Path.IsEmpty())
	{
		return Statement.Path;
	}
	return TEXT("event_delegate_statement");
}

static EBlueprintHelperActionSemanticKind ToEventDelegateActionSemanticKind(
	const EBlueprintHelperGraphStatementKind StatementKind)
{
	switch (StatementKind)
	{
	case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
		return EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
	case EBlueprintHelperGraphStatementKind::Delegate:
		return EBlueprintHelperActionSemanticKind::Delegate;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

static bool IsDelegateReferenceOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase);
}

static void AddPinRef(
	FBlueprintHelperNodeFragment& Fragment,
	const FString& NodeId,
	const FString& Key,
	UEdGraphPin* Pin)
{
	if (!Pin || Key.IsEmpty())
	{
		return;
	}

	const FString Type = Pin->PinType.PinCategory.ToString();
	FBlueprintHelperFragmentPinRef PinRef{ NodeId, Pin->PinName.ToString(), Type, Pin };
	Fragment.PinBindings.Add(Key, PinRef);
	if (!Fragment.PinBindings.Contains(Key.ToLower()))
	{
		Fragment.PinBindings.Add(Key.ToLower(), PinRef);
	}

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		return;
	}
	if (Pin->Direction == EGPD_Input)
	{
		Fragment.DataInputs.Add(Key, PinRef);
	}
	else if (Pin->Direction == EGPD_Output)
	{
		Fragment.DataOutputs.Add(Key, PinRef);
	}
}

static void PopulatePrimaryPins(UK2Node* PrimaryNode, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("then"));
	AddPinRef(OutFragment, TEXT("primary"), TEXT("execute"), OutFragment.ExecEntryPin);
	AddPinRef(OutFragment, TEXT("primary"), TEXT("then"), OutFragment.ExecExitPin);

	if (!PrimaryNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : PrimaryNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}
		AddPinRef(OutFragment, TEXT("primary"), Pin->PinName.ToString(), Pin);
	}
}

static void PopulateCommonFragmentMetadata(
	const FString& StatementId,
	const FString& SemanticKind,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), StatementId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), SemanticKind);
	OutFragment.OwnershipTags.Add(TEXT("delegate_name"), Evidence.DelegateName);
	if (!Evidence.DelegateOperation.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("delegate_operation"), Evidence.DelegateOperation);
	}
	if (!Evidence.HandlerName.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("handler_name"), Evidence.HandlerName);
	}
	if (!Evidence.BindingObjectPath.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("binding_object_path"), Evidence.BindingObjectPath);
	}
	if (!Evidence.ComponentPath.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("component_path"), Evidence.ComponentPath);
	}
	OutFragment.LayoutHints.Add(TEXT("x"), TEXT("0"));
	OutFragment.LayoutHints.Add(TEXT("y"), TEXT("0"));
	OutFragment.ReviewTargets.Add(StatementId);
}

static void CollectLiteralDefaultValues(
	const FBlueprintHelperGraphStatementIR& Statement,
	TMap<FString, FString>& OutDefaultValues)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement.Args)
	{
		if (ArgPair.Value.IsValid() && ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
		}
	}
}

static bool BuildActionRequest(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FString& StatementId,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	if (!TargetGraph)
	{
		OutError = TEXT("event_delegate fragment build failed: target graph is invalid.");
		return false;
	}
	if (!ActionContextScope)
	{
		OutError = TEXT("event_delegate fragment build failed: action context scope is required.");
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return ActionContextScope->TryBuildRequest(StatementId, Blueprint, TargetGraph, OutRequest, OutError);
}

static bool ResolveEventDelegateAction(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperActionResolutionResult& OutResult,
	FString& OutError)
{
	OutResult = FBlueprintGraphWriteFacade::ResolveActionForGraph(Request);
	if (OutResult.IsResolved())
	{
		return true;
	}

	OutError = OutResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("event_delegate action resolve failed: semantic=%s cluster=%s"),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(OutResult.ClusterKind))
		: OutResult.Message;
	return false;
}

static UK2Node_AssignDelegate* SpawnAssignDelegateNodeManually(
	UEdGraph* TargetGraph,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	const FVector2D& Location,
	FString& OutError)
{
	if (!TargetGraph || !Evidence.DelegateProperty)
	{
		OutError = TEXT("delegate.assign manual spawn failed: target graph or delegate evidence is invalid.");
		return nullptr;
	}

	UK2Node_AssignDelegate* AssignNode = NewObject<UK2Node_AssignDelegate>(TargetGraph);
	if (!AssignNode)
	{
		OutError = TEXT("delegate.assign manual spawn failed: could not allocate UK2Node_AssignDelegate.");
		return nullptr;
	}

	AssignNode->CreateNewGuid();
	AssignNode->NodePosX = static_cast<int32>(Location.X);
	AssignNode->NodePosY = static_cast<int32>(Location.Y);
	AssignNode->SetFromProperty(Evidence.DelegateProperty, false, Evidence.DelegateProperty->GetOwnerClass());
	AssignNode->SetFlags(RF_Transactional);
	AssignNode->AllocateDefaultPins();
	TargetGraph->Modify();
	TargetGraph->AddNode(AssignNode, /*bFromUI=*/true, /*bSelectNewNode=*/false);
	return AssignNode;
}

static UK2Node_CreateDelegate* SpawnCreateDelegateNode(
	UEdGraph* TargetGraph,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	const FString& StatementId,
	const FVector2D& Location,
	FString& OutError)
{
	UBlueprintNodeSpawner* CreateDelegateSpawner = UBlueprintNodeSpawner::Create(UK2Node_CreateDelegate::StaticClass());
	if (!CreateDelegateSpawner)
	{
		OutError = TEXT("create delegate spawn failed: node spawner unavailable.");
		return nullptr;
	}

	FBlueprintHelperActionNodeSpawnOptions Options;
	Options.NodeId = StatementId + TEXT(":create_delegate");
	Options.bReconstructAfterSpawn = false;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
		TargetGraph,
		CreateDelegateSpawner,
		Options.NodeId,
		Location,
		Options,
		OutError);
	UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(SpawnedNode);
	if (!CreateDelegateNode)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("create delegate spawn failed: spawner did not create UK2Node_CreateDelegate.");
		}
		return nullptr;
	}

	CreateDelegateNode->SetFunction(FName(*Evidence.HandlerName));
	return CreateDelegateNode;
}

static bool ConnectCreateDelegateToPrimary(
	UEdGraph* TargetGraph,
	UK2Node_BaseMCDelegate* PrimaryDelegateNode,
	UK2Node_CreateDelegate* CreateDelegateNode,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (!TargetGraph || !TargetGraph->GetSchema())
	{
		OutError = TEXT("create delegate link failed: graph schema is invalid.");
		return false;
	}
	if (!PrimaryDelegateNode || !CreateDelegateNode)
	{
		OutError = TEXT("create delegate link failed: delegate nodes are invalid.");
		return false;
	}

	UEdGraphPin* DelegateOutPin = CreateDelegateNode->GetDelegateOutPin();
	UEdGraphPin* DelegateInPin = PrimaryDelegateNode->GetDelegatePin();
	if (!DelegateOutPin || !DelegateInPin)
	{
		OutError = TEXT("create delegate link failed: delegate pins are missing.");
		return false;
	}
	if (!TargetGraph->GetSchema()->TryCreateConnection(DelegateOutPin, DelegateInPin))
	{
		OutError = TEXT("create delegate link failed: schema rejected delegate pin connection.");
		return false;
	}

	FBlueprintHelperFragmentLink Link;
	Link.From = FBlueprintHelperFragmentPinRef{ TEXT("create_delegate"), DelegateOutPin->PinName.ToString(), DelegateOutPin->PinType.PinCategory.ToString(), DelegateOutPin };
	Link.To = FBlueprintHelperFragmentPinRef{ TEXT("primary"), DelegateInPin->PinName.ToString(), DelegateInPin->PinType.PinCategory.ToString(), DelegateInPin };
	OutFragment.InternalLinks.Add(Link);
	OutFragment.PinBindings.Add(TEXT("create_delegate.event"), Link.From);
	OutFragment.PinBindings.Add(TEXT("delegate.event"), Link.To);
	return true;
}

static bool ConnectBindingObjectToPrimaryTarget(
	UEdGraph* TargetGraph,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	UK2Node* PrimaryNode,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (!TargetGraph || !TargetGraph->GetSchema() || !Evidence.ComponentBindingProperty || !PrimaryNode)
	{
		return true;
	}

	UEdGraphPin* TargetPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("target"));
	if (!TargetPin)
	{
		TargetPin = PrimaryNode->FindPin(UEdGraphSchema_K2::PN_Self);
	}
	if (!TargetPin || TargetPin->LinkedTo.Num() > 0)
	{
		return true;
	}

	UK2Node_VariableGet* ComponentGetNode = NewObject<UK2Node_VariableGet>(TargetGraph);
	if (!ComponentGetNode)
	{
		OutError = TEXT("delegate target binding failed: could not allocate component getter.");
		return false;
	}

	TargetGraph->Modify();
	TargetGraph->AddNode(ComponentGetNode, /*bFromUI=*/true, /*bSelectNewNode=*/false);
	ComponentGetNode->CreateNewGuid();
	ComponentGetNode->SetFlags(RF_Transactional);
	ComponentGetNode->VariableReference.SetSelfMember(Evidence.ComponentBindingProperty->GetFName());
	ComponentGetNode->NodePosX = PrimaryNode->NodePosX - 220;
	ComponentGetNode->NodePosY = PrimaryNode->NodePosY - 120;
	ComponentGetNode->PostPlacedNewNode();
	ComponentGetNode->AllocateDefaultPins();

	UEdGraphPin* ComponentOutputPin = ComponentGetNode->GetValuePin();
	if (!ComponentOutputPin)
	{
		OutError = TEXT("delegate target binding failed: component getter output pin is missing.");
		return false;
	}
	if (!TargetGraph->GetSchema()->TryCreateConnection(ComponentOutputPin, TargetPin))
	{
		OutError = TEXT("delegate target binding failed: schema rejected component target connection.");
		return false;
	}

	OutFragment.Nodes.Add(ComponentGetNode);
	AddPinRef(OutFragment, TEXT("binding_object"), Evidence.ComponentPath, ComponentOutputPin);
	AddPinRef(OutFragment, TEXT("delegate"), TEXT("target"), TargetPin);

	FBlueprintHelperFragmentLink Link;
	Link.From = FBlueprintHelperFragmentPinRef{ TEXT("binding_object"), ComponentOutputPin->PinName.ToString(), ComponentOutputPin->PinType.PinCategory.ToString(), ComponentOutputPin };
	Link.To = FBlueprintHelperFragmentPinRef{ TEXT("delegate"), TargetPin->PinName.ToString(), TargetPin->PinType.PinCategory.ToString(), TargetPin };
	OutFragment.InternalLinks.Add(Link);
	OutFragment.PinBindings.Add(TEXT("binding_object.value"), Link.From);
	OutFragment.PinBindings.Add(TEXT("delegate.target"), Link.To);
	return true;
}

static UK2Node* SpawnResolvedPrimaryNode(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	const FBlueprintHelperGraphStatementIR& Statement,
	const FString& StatementId,
	FString& OutError)
{
	const FVector2D PrimaryLocation(0.0, 0.0);
	if (Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
	{
		return SpawnAssignDelegateNodeManually(TargetGraph, Evidence, PrimaryLocation, OutError);
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = StatementId;
	CollectLiteralDefaultValues(Statement, SpawnOptions.DefaultValues);
	if ((Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::Delegate)
		&& Evidence.ComponentBindingProperty)
	{
		SpawnOptions.Bindings.Add(FBindingObject(Evidence.ComponentBindingProperty));
	}

	return FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		PrimaryLocation,
		SpawnOptions,
		OutError);
}
}

bool FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	const EBlueprintHelperActionSemanticKind SemanticKind = ToEventDelegateActionSemanticKind(Statement.Kind);
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Unknown)
	{
		OutError = FString::Printf(TEXT("event_delegate fragment build failed: unsupported statement kind '%s'."), *Statement.PatternName);
		return false;
	}

	const FString StatementId = GetStatementId(Statement);
	FBlueprintHelperActionResolutionRequest ActionRequest;
	if (!BuildActionRequest(TargetGraph, ActionContextScope, StatementId, ActionRequest, OutError))
	{
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!ResolveEventDelegateAction(ActionRequest, ActionResult, OutError))
	{
		return false;
	}

	FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
	FString MissingDetail;
	FString MissingMessage;
	if (!FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
		ActionRequest,
		SemanticKind,
		Evidence,
		MissingDetail,
		MissingMessage))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *MissingDetail, *MissingMessage);
		return false;
	}

	UK2Node* PrimaryNode = SpawnResolvedPrimaryNode(
		TargetGraph,
		ActionResult,
		Evidence,
		Statement,
		StatementId,
		OutError);
	if (!PrimaryNode)
	{
		return false;
	}

	OutFragment.FragmentId = StatementId;
	OutFragment.SourceStatementId = StatementId;
	OutFragment.PrimaryNode = PrimaryNode;
	OutFragment.Nodes.Add(PrimaryNode);
	PopulatePrimaryPins(PrimaryNode, OutFragment);
	PopulateCommonFragmentMetadata(
		StatementId,
		FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
		Evidence,
		OutFragment);

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& !ConnectBindingObjectToPrimaryTarget(TargetGraph, Evidence, PrimaryNode, OutFragment, OutError))
	{
		return false;
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& IsDelegateReferenceOperation(Evidence.DelegateOperation))
	{
		UK2Node_BaseMCDelegate* PrimaryDelegateNode = Cast<UK2Node_BaseMCDelegate>(PrimaryNode);
		if (!PrimaryDelegateNode)
		{
			OutError = TEXT("delegate reference fragment build failed: primary node is not a multicast delegate node.");
			return false;
		}

		UK2Node_CreateDelegate* CreateDelegateNode = SpawnCreateDelegateNode(
			TargetGraph,
			Evidence,
			StatementId,
			FVector2D(-220.0, 120.0),
			OutError);
		if (!CreateDelegateNode)
		{
			return false;
		}

		OutFragment.Nodes.Add(CreateDelegateNode);
		if (!ConnectCreateDelegateToPrimary(TargetGraph, PrimaryDelegateNode, CreateDelegateNode, OutFragment, OutError))
		{
			return false;
		}
	}

	return true;
}
