#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "Components/PrimitiveComponent.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/TriggerBase.h"
#include "Engine/TriggerBox.h"
#include "GameFramework/Actor.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_RemoveDelegate.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
static FString MakeEventDelegateActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeEventDelegateActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperEventDelegateAction/%s"),
		*MakeEventDelegateActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ATriggerBox::StaticClass(),
		Package,
		*MakeEventDelegateActionTestObjectName(TEXT("BP_EventDelegateAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperEventDelegateActionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetEventDelegateActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeEventDelegateActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeEventDelegateActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("event_delegate_projected_context");
	Request.SemanticConstraintsHash = TEXT("event_delegate_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.MaxCandidates = 8;
	return Request;
}

static int32 CountGraphNodesOfClass(UEdGraph* Graph, const UClass* NodeClass)
{
	int32 Count = 0;
	if (!Graph || !NodeClass)
	{
		return Count;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->IsA(NodeClass))
		{
			++Count;
		}
	}
	return Count;
}

static FMulticastDelegateProperty* FindRequiredDelegateProperty(
	FAutomationTestBase& Test,
	UClass* OwnerClass,
	const TCHAR* PropertyName)
{
	FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(OwnerClass, PropertyName);
	Test.TestNotNull(*FString::Printf(TEXT("delegate property %s"), PropertyName), Property);
	return Property;
}

static FObjectProperty* FindRequiredObjectProperty(
	FAutomationTestBase& Test,
	UClass* OwnerClass,
	const TCHAR* PropertyName)
{
	FObjectProperty* Property = FindFProperty<FObjectProperty>(OwnerClass, PropertyName);
	Test.TestNotNull(*FString::Printf(TEXT("object property %s"), PropertyName), Property);
	return Property;
}

static void AddDelegateEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	FMulticastDelegateProperty* DelegateProperty)
{
	const UClass* OwnerClass = Cast<UClass>(DelegateProperty ? DelegateProperty->GetOwnerStruct() : nullptr);
	Request.ContextEvidence.Add(TEXT("delegate_name"), DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("delegate_signature"), DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetPathName() : TEXT("delegate_signature_missing_for_test"));
	Request.ContextEvidence.Add(TEXT("delegate_owner_class_path"), OwnerClass ? OwnerClass->GetPathName() : TEXT(""));
	Request.ContextEvidence.Add(TEXT("delegate_property_name"), DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("delegate_property_path"), DelegateProperty->GetPathName());
	Request.Semantic.PropertyPath = DelegateProperty->GetName();
}

static void AddComponentEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	FObjectProperty* ComponentProperty)
{
	const UClass* OwnerClass = Cast<UClass>(ComponentProperty ? ComponentProperty->GetOwnerStruct() : nullptr);
	Request.ContextEvidence.Add(TEXT("component_path"), ComponentProperty->GetName());
	Request.ContextEvidence.Add(TEXT("component_binding_owner_class_path"), OwnerClass ? OwnerClass->GetPathName() : TEXT(""));
	Request.ContextEvidence.Add(TEXT("component_binding_field_path"), ComponentProperty->GetPathName());
}

static void AddHandlerEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	UClass* HandlerScopeClass,
	const TCHAR* HandlerName)
{
	Request.ContextEvidence.Add(TEXT("handler_name"), HandlerName);
	Request.ContextEvidence.Add(TEXT("handler_scope_class_path"), HandlerScopeClass ? HandlerScopeClass->GetPathName() : TEXT(""));
}

static bool AssertMissingEvidenceDiagnostic(
	FAutomationTestBase& Test,
	const FString& Label,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedDetail)
{
	bool bPassed = true;
	bPassed &= Test.TestNotEqual(*FString::Printf(TEXT("%s not resolved"), *Label), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s error code"), *Label), Result.ErrorCode, FString(TEXT("missing_required_evidence")));
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s message names missing evidence"), *Label), Result.Message.Contains(ExpectedDetail));
	return bPassed;
}

static FBlueprintHelperGraphStatementIR MakeComponentBoundEventStatement(
	const FString& StatementId,
	const FString& DelegateName,
	const FString& ComponentName,
	const FString& HandlerName)
{
	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = StatementId;
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::ComponentBoundEvent;
	Statement.PatternName = TEXT("component_bound_event");
	Statement.ComponentName = ComponentName;
	Statement.DelegateName = DelegateName;
	Statement.Property = DelegateName;
	Statement.Name = DelegateName;
	Statement.HandlerName = HandlerName;
	return Statement;
}

static FBlueprintHelperGraphStatementIR MakeDelegateStatement(
	const FString& StatementId,
	const FString& DelegateName,
	const FString& Operation,
	const FString& BindingObjectPath,
	const FString& HandlerName = FString(),
	const FString& UnbindMode = FString())
{
	FBlueprintHelperGraphStatementIR Statement;
	Statement.StatementId = StatementId;
	Statement.Path = TEXT("$.statements[0]");
	Statement.Kind = EBlueprintHelperGraphStatementKind::Delegate;
	Statement.PatternName = TEXT("delegate");
	Statement.Target = BindingObjectPath;
	Statement.DelegateName = DelegateName;
	Statement.Property = DelegateName;
	Statement.Name = DelegateName;
	Statement.DelegateOperation = Operation;
	Statement.HandlerName = HandlerName;
	Statement.UnbindMode = UnbindMode;
	return Statement;
}

static bool BuildActionContextScopeForStatement(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(MakeShared<FBlueprintHelperGraphStatementIR>(Statement));

	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);
	Test.TestTrue(TEXT("action context demands exist"), Demands.Num() > 0);
	if (Demands.Num() == 0)
	{
		OutError = TEXT("no_action_context_demands");
		return false;
	}

	return FBlueprintHelperActionContextScope::Build(
		Blueprint,
		Graph,
		Demands,
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			Graph,
			TEXT("event_delegate_fragment_tests"),
			TEXT("gap5_task4")),
		OutScope,
		OutError);
}

template <typename TNode>
static TNode* FindSingleFragmentNode(
	FAutomationTestBase& Test,
	const FBlueprintHelperNodeFragment& Fragment,
	const TCHAR* Label)
{
	TNode* Result = nullptr;
	int32 MatchCount = 0;
	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		if (TNode* TypedNode = Cast<TNode>(Node))
		{
			Result = TypedNode;
			++MatchCount;
		}
	}

	Test.TestEqual(FString::Printf(TEXT("%s count"), Label), MatchCount, 1);
	return MatchCount == 1 ? Result : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateComponentBoundPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.ComponentBound.Positive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateComponentBoundPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	FObjectProperty* ComponentProperty =
		FindRequiredObjectProperty(*this, ATriggerBase::StaticClass(), TEXT("CollisionComponent"));
	if (!DelegateProperty || !ComponentProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, DelegateProperty->GetName());
	AddDelegateEvidence(Request, DelegateProperty);
	AddComponentEvidence(Request, ComponentProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("component_bound_event:%s:%s"), *DelegateProperty->GetPathName(), *ComponentProperty->GetPathName()));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_bound_event_node_spawner")));
		TestTrue(
			TEXT("node class path contains component bound event node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_ComponentBoundEvent")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateComponentBoundMissingHandlerTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.ComponentBound.MissingHandler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateComponentBoundMissingHandlerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	FObjectProperty* ComponentProperty =
		FindRequiredObjectProperty(*this, ATriggerBase::StaticClass(), TEXT("CollisionComponent"));
	if (!DelegateProperty || !ComponentProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, DelegateProperty->GetName());
	AddDelegateEvidence(Request, DelegateProperty);
	AddComponentEvidence(Request, ComponentProperty);

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("component bound event missing handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_missing"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.BindPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("delegate:bind:%s:%s"), *DelegateProperty->GetPathName(), TEXT("K2_DestroyActor")));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_delegate_node_spawner")));
		TestTrue(
			TEXT("node class path contains add delegate node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_AddDelegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindMissingHandlerTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.BindMissingHandler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindMissingHandlerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_missing"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateAssignPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.AssignPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateAssignPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("assign"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNull(TEXT("assign uses manual factory instead of UE assign spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("delegate:assign:%s:%s"), *DelegateProperty->GetPathName(), TEXT("K2_DestroyActor")));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_delegate_manual_assign_factory")));
		TestTrue(
			TEXT("node class path contains assign delegate node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_AssignDelegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateUnbindPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.UnbindPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateUnbindPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("unbind"));
	Request.ContextEvidence.Add(TEXT("unbind_mode"), TEXT("single"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("delegate:unbind:%s:%s"), *DelegateProperty->GetPathName(), TEXT("K2_DestroyActor")));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_delegate_node_spawner")));
		TestTrue(
			TEXT("node class path contains remove delegate node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_RemoveDelegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateUnbindMissingHandlerTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.UnbindMissingHandler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateUnbindMissingHandlerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("unbind"));
	Request.ContextEvidence.Add(TEXT("unbind_mode"), TEXT("single"));
	AddDelegateEvidence(Request, DelegateProperty);

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate unbind missing handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_missing"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateCallPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.CallPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateCallPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("call"));
	AddDelegateEvidence(Request, DelegateProperty);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("delegate:call:%s"), *DelegateProperty->GetPathName()));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_delegate_node_spawner")));
		TestTrue(
			TEXT("node class path contains call delegate node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_CallDelegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateClearPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.ClearPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateClearPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("clear"));
	Request.ContextEvidence.Add(TEXT("unbind_mode"), TEXT("all"));
	AddDelegateEvidence(Request, DelegateProperty);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("selected spawner"), Result.SelectedSpawner.Get());
	TestEqual(
		TEXT("stable id"),
		Result.SelectedStableId,
		FString::Printf(TEXT("delegate:clear:%s"), *DelegateProperty->GetPathName()));
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("match reason"), Result.CandidateActions[0].MatchReason, FString(TEXT("ue_delegate_node_spawner")));
		TestTrue(
			TEXT("node class path contains clear delegate node"),
			Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_ClearDelegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateClearHandlerForbiddenTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.ClearHandlerForbidden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateClearHandlerForbiddenTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("delegate_operation"), TEXT("clear"));
	Request.ContextEvidence.Add(TEXT("unbind_mode"), TEXT("all"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate clear forbids handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("delegate_clear_handler_forbidden"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateComponentBoundFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.ComponentBound.FragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateComponentBoundFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeComponentBoundEventStatement(
		TEXT("stmt_component_bound_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("CollisionComponent"),
		TEXT("K2_DestroyActor"));

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build component-bound action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("component-bound scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("component-bound fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("component-bound build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_ComponentBoundEvent* EventNode =
		FindSingleFragmentNode<UK2Node_ComponentBoundEvent>(*this, Fragment, TEXT("component-bound event node"));
	TestNotNull(TEXT("component-bound primary node"), Fragment.PrimaryNode);
	TestEqual(TEXT("fragment node count"), Fragment.Nodes.Num(), 1);
	TestEqual(TEXT("fragment source statement id"), Fragment.SourceStatementId, Statement.StatementId);
	TestTrue(TEXT("fragment review target includes statement id"), Fragment.ReviewTargets.Contains(Statement.StatementId));
	TestEqual(TEXT("ownership statement id"), Fragment.OwnershipTags.FindRef(TEXT("statement_id")), Statement.StatementId);
	TestEqual(TEXT("ownership semantic kind"), Fragment.OwnershipTags.FindRef(TEXT("semantic_kind")), FString(TEXT("component_bound_event")));
	if (EventNode)
	{
		TestEqual(TEXT("component property name"), EventNode->GetComponentPropertyName(), FName(TEXT("CollisionComponent")));
		TestEqual(TEXT("delegate property name"), EventNode->DelegatePropertyName, FName(TEXT("OnComponentBeginOverlap")));
	}
	return bBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateAssignFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.Delegate.AssignFragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateAssignFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_assign_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("assign"),
		TEXT("CollisionComponent"),
		TEXT("K2_DestroyActor"));

	const int32 CustomEventCountBeforeBuild = CountGraphNodesOfClass(Graph, UK2Node_CustomEvent::StaticClass());

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build assign action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("assign scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("assign fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("assign build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_AssignDelegate* AssignNode =
		FindSingleFragmentNode<UK2Node_AssignDelegate>(*this, Fragment, TEXT("assign node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
	TestEqual(TEXT("assign fragment node count"), Fragment.Nodes.Num(), 2);
	TestEqual(TEXT("assign fragment internal link count"), Fragment.InternalLinks.Num(), 1);
	TestTrue(TEXT("assign fragment review target includes statement id"), Fragment.ReviewTargets.Contains(Statement.StatementId));
	TestEqual(TEXT("assign ownership semantic kind"), Fragment.OwnershipTags.FindRef(TEXT("semantic_kind")), FString(TEXT("delegate")));
	TestEqual(TEXT("assign ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("assign")));
	if (CreateDelegateNode)
	{
		TestEqual(TEXT("create delegate selected function"), CreateDelegateNode->GetFunctionName(), FName(TEXT("K2_DestroyActor")));
	}
	if (AssignNode)
	{
		UEdGraphPin* DelegatePin = AssignNode->GetDelegatePin();
		TestNotNull(TEXT("assign delegate input pin"), DelegatePin);
		if (DelegatePin)
		{
			TestEqual(TEXT("assign delegate input has one link"), DelegatePin->LinkedTo.Num(), 1);
			if (CreateDelegateNode)
			{
				TestEqual(TEXT("assign link source is create delegate output"), DelegatePin->LinkedTo[0], CreateDelegateNode->GetDelegateOutPin());
			}
		}
	}

	const int32 CustomEventCountAfterBuild = CountGraphNodesOfClass(Graph, UK2Node_CustomEvent::StaticClass());
	TestEqual(TEXT("assign build does not auto-generate custom event on graph"), CustomEventCountAfterBuild, CustomEventCountBeforeBuild);
	return bBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.Delegate.BindFragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_bind_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("bind"),
		TEXT("CollisionComponent"),
		TEXT("K2_DestroyActor"));

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build bind action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("bind scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("bind fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("bind build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_AddDelegate* BindNode =
		FindSingleFragmentNode<UK2Node_AddDelegate>(*this, Fragment, TEXT("bind node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
	TestEqual(TEXT("bind fragment node count"), Fragment.Nodes.Num(), 2);
	TestEqual(TEXT("bind fragment internal link count"), Fragment.InternalLinks.Num(), 1);
	TestEqual(TEXT("bind ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("bind")));
	if (CreateDelegateNode)
	{
		TestEqual(TEXT("bind create delegate selected function"), CreateDelegateNode->GetFunctionName(), FName(TEXT("K2_DestroyActor")));
	}
	if (BindNode && CreateDelegateNode)
	{
		UEdGraphPin* DelegatePin = BindNode->GetDelegatePin();
		TestNotNull(TEXT("bind delegate input pin"), DelegatePin);
		if (DelegatePin)
		{
			TestEqual(TEXT("bind delegate input has one link"), DelegatePin->LinkedTo.Num(), 1);
			TestEqual(TEXT("bind link source is create delegate output"), DelegatePin->LinkedTo[0], CreateDelegateNode->GetDelegateOutPin());
		}
	}
	return bBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateUnbindFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.Delegate.UnbindFragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateUnbindFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_unbind_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("unbind"),
		TEXT("CollisionComponent"),
		TEXT("K2_DestroyActor"),
		TEXT("single"));

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build unbind action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("unbind scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("unbind fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("unbind build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	UK2Node_RemoveDelegate* UnbindNode =
		FindSingleFragmentNode<UK2Node_RemoveDelegate>(*this, Fragment, TEXT("unbind node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
	TestEqual(TEXT("unbind fragment node count"), Fragment.Nodes.Num(), 2);
	TestEqual(TEXT("unbind fragment internal link count"), Fragment.InternalLinks.Num(), 1);
	TestEqual(TEXT("unbind ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("unbind")));
	if (CreateDelegateNode)
	{
		TestEqual(TEXT("unbind create delegate selected function"), CreateDelegateNode->GetFunctionName(), FName(TEXT("K2_DestroyActor")));
	}
	if (UnbindNode && CreateDelegateNode)
	{
		UEdGraphPin* DelegatePin = UnbindNode->GetDelegatePin();
		TestNotNull(TEXT("unbind delegate input pin"), DelegatePin);
		if (DelegatePin)
		{
			TestEqual(TEXT("unbind delegate input has one link"), DelegatePin->LinkedTo.Num(), 1);
			TestEqual(TEXT("unbind link source is create delegate output"), DelegatePin->LinkedTo[0], CreateDelegateNode->GetDelegateOutPin());
		}
	}
	return bBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateCallFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.Delegate.CallFragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateCallFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_call_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("call"),
		TEXT("CollisionComponent"));

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build call action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("call scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("call fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("call build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	FindSingleFragmentNode<UK2Node_CallDelegate>(*this, Fragment, TEXT("call node"));
	TestEqual(TEXT("call fragment node count"), Fragment.Nodes.Num(), 1);
	TestEqual(TEXT("call fragment internal link count"), Fragment.InternalLinks.Num(), 0);
	TestEqual(TEXT("call ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("call")));
	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		TestFalse(TEXT("call fragment does not create delegate node"), Node->IsA<UK2Node_CreateDelegate>());
	}
	return bBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateClearFragmentBuildTest,
	"BlueprintHelper.GraphWrite.GraphStatement.EventDelegate.Delegate.ClearFragmentBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateClearFragmentBuildTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_clear_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("clear"),
		TEXT("CollisionComponent"),
		FString(),
		TEXT("all"));

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build clear action context scope"),
		BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, ActionContextScope, ScopeError));
	if (!ScopeError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("clear scope error: %s"), *ScopeError));
	}

	FBlueprintHelperNodeFragment Fragment;
	FString BuildError;
	const bool bBuilt = FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		Graph,
		&ActionContextScope,
		Statement,
		Fragment,
		BuildError);
	TestTrue(TEXT("clear fragment build succeeds"), bBuilt);
	if (!BuildError.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("clear build error: %s"), *BuildError));
	}
	if (!bBuilt)
	{
		return false;
	}

	FindSingleFragmentNode<UK2Node_ClearDelegate>(*this, Fragment, TEXT("clear node"));
	TestEqual(TEXT("clear fragment node count"), Fragment.Nodes.Num(), 1);
	TestEqual(TEXT("clear fragment internal link count"), Fragment.InternalLinks.Num(), 0);
	TestEqual(TEXT("clear ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("clear")));
	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		TestFalse(TEXT("clear fragment does not create delegate node"), Node->IsA<UK2Node_CreateDelegate>());
	}
	return bBuilt;
}

#endif
