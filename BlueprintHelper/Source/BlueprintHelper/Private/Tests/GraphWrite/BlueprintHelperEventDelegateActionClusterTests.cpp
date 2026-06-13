#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "Shared/BlueprintHelperVersionCompat.h"
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
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Tests/GraphWrite/BlueprintHelperGraphWriteTestUtils.h"

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

static FObjectPropertyBase* FindRequiredObjectProperty(
	FAutomationTestBase& Test,
	UClass* OwnerClass,
	const TCHAR* PropertyName)
{
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(OwnerClass, PropertyName);
	Test.TestNotNull(*FString::Printf(TEXT("object property %s"), PropertyName), Property);
	return Property;
}

static void AddDelegateEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	FMulticastDelegateProperty* DelegateProperty)
{
	const UClass* OwnerClass = Cast<UClass>(DelegateProperty ? DelegateProperty->GetOwnerStruct() : nullptr);
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_name"), DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_signature"), DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetPathName() : TEXT("delegate_signature_missing_for_test"));
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_signature_function_path"), DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetPathName() : TEXT(""));
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_owner_class_path"), OwnerClass ? OwnerClass->GetPathName() : TEXT(""));
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_property_name"), DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_property_path"), DelegateProperty->GetPathName());
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_blueprint_assignable"), DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable) ? TEXT("true") : TEXT("false"));
	Request.ContextEvidence.Add(TEXT("event_delegate.delegate_blueprint_callable"), DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable) ? TEXT("true") : TEXT("false"));
	Request.Semantic.PropertyPath = DelegateProperty->GetName();
}

static void AddComponentEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	FObjectPropertyBase* ComponentProperty)
{
	const UClass* OwnerClass = Cast<UClass>(ComponentProperty ? ComponentProperty->GetOwnerStruct() : nullptr);
	Request.ContextEvidence.Add(TEXT("event_delegate.component_property_name"), ComponentProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.component_path"), ComponentProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.component_binding_owner_class_path"), OwnerClass ? OwnerClass->GetPathName() : TEXT(""));
	Request.ContextEvidence.Add(TEXT("event_delegate.component_binding_field_path"), ComponentProperty->GetPathName());
	Request.ContextEvidence.Add(TEXT("event_delegate.component_class_path"), ComponentProperty->PropertyClass ? ComponentProperty->PropertyClass->GetPathName() : TEXT(""));
}

static void AddHandlerEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	UClass* HandlerScopeClass,
	const TCHAR* HandlerName)
{
	Request.ContextEvidence.Add(TEXT("event_delegate.handler_name"), HandlerName);
	Request.ContextEvidence.Add(TEXT("event_delegate.handler_scope_class_path"), HandlerScopeClass ? HandlerScopeClass->GetPathName() : TEXT(""));
	if (HandlerScopeClass)
	{
		if (UFunction* HandlerFunction = HandlerScopeClass->FindFunctionByName(FName(HandlerName)))
		{
			Request.ContextEvidence.Add(TEXT("event_delegate.handler_function_path"), HandlerFunction->GetPathName());
			Request.ContextEvidence.Add(TEXT("event_delegate.handler_source_cluster"), TEXT("BlueprintSignature"));
			Request.ContextEvidence.Add(TEXT("event_delegate.signature_evidence_id"), FString::Printf(TEXT("signature:handler:%s"), HandlerName));
		}
	}
}

static bool AssertMissingEvidenceDiagnostic(
	FAutomationTestBase& Test,
	const FString& Label,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedDetail)
{
	bool bPassed = true;
	bPassed &= Test.TestNotEqual(*FString::Printf(TEXT("%s not resolved"), *Label), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s error code"), *Label), Result.ErrorCode, ExpectedDetail);
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s message names missing evidence"), *Label), Result.Message.Contains(ExpectedDetail) || Result.ErrorCode == ExpectedDetail);
	return bPassed;
}

static void AddStatementHandlerEvidence(
	FBlueprintHelperGraphStatementIR& Statement,
	UClass* HandlerScopeClass,
	const TCHAR* HandlerName)
{
	if (!HandlerScopeClass || !HandlerName)
	{
		return;
	}
	if (UFunction* HandlerFunction = HandlerScopeClass->FindFunctionByName(FName(HandlerName)))
	{
		Statement.ContextEvidence.Add(TEXT("event_delegate.handler_function_path"), HandlerFunction->GetPathName());
		Statement.ContextEvidence.Add(TEXT("event_delegate.handler_source_cluster"), TEXT("BlueprintSignature"));
		Statement.ContextEvidence.Add(TEXT("event_delegate.signature_evidence_id"), FString::Printf(TEXT("signature:handler:%s"), HandlerName));
	}
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
	AddStatementHandlerEvidence(Statement, AActor::StaticClass(), *HandlerName);
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
	if (!HandlerName.IsEmpty())
	{
		AddStatementHandlerEvidence(Statement, AActor::StaticClass(), *HandlerName);
	}
	return Statement;
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
	FObjectPropertyBase* ComponentProperty =
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
	FObjectPropertyBase* ComponentProperty =
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
		TEXT("missing_handler_evidence"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("missing_handler_evidence"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindMissingHandlerFunctionPathTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.BindMissingHandlerFunctionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindMissingHandlerFunctionPathTest::RunTest(const FString& Parameters)
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	Request.ContextEvidence.Add(TEXT("event_delegate.handler_name"), TEXT("K2_DestroyActor"));
	Request.ContextEvidence.Add(TEXT("event_delegate.handler_scope_class_path"), AActor::StaticClass()->GetPathName());

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing handler function path"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("missing_handler_evidence"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("assign"));
	Request.ContextEvidence.Add(TEXT("event_delegate.assign_factory"), TEXT("ue_delegate_manual_assign_factory"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("unbind"));
	Request.ContextEvidence.Add(TEXT("event_delegate.unbind_mode"), TEXT("single"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("unbind"));
	Request.ContextEvidence.Add(TEXT("event_delegate.unbind_mode"), TEXT("single"));
	AddDelegateEvidence(Request, DelegateProperty);

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate unbind missing handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_required_for_unbind"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("call"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("clear"));
	Request.ContextEvidence.Add(TEXT("event_delegate.unbind_mode"), TEXT("all"));
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
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("clear"));
	Request.ContextEvidence.Add(TEXT("event_delegate.unbind_mode"), TEXT("all"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate clear forbids handler"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("handler_not_allowed_for_clear"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateMissingBindingObjectTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.MissingBindingObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateMissingBindingObjectTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!Blueprint || !Graph || !DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	return AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing binding object"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("missing_binding_object_evidence"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateMissingSignatureTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Delegate.MissingSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateMissingSignatureTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!Blueprint || !Graph || !DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	Request.ContextEvidence.Remove(TEXT("event_delegate.delegate_signature_function_path"));
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	return AssertMissingEvidenceDiagnostic(
		*this,
		TEXT("delegate bind missing signature"),
		FBlueprintHelperActionResolutionCore::Resolve(Request),
		TEXT("missing_signature_evidence"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateIncompatibleGraphTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Policy.IncompatibleGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateIncompatibleGraphTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* InvalidGraph = NewObject<UEdGraph>(GetTransientPackage());
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!Blueprint || !InvalidGraph || !DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, InvalidGraph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("bind"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("incompatible graph error"), Result.ErrorCode, FString(TEXT("incompatible_graph_type")));
	TestNotEqual(TEXT("incompatible graph not resolved"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateDuplicatePolicyTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Policy.DuplicatePolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateDuplicatePolicyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	FObjectPropertyBase* ComponentProperty =
		FindRequiredObjectProperty(*this, ATriggerBase::StaticClass(), TEXT("CollisionComponent"));
	if (!Blueprint || !Graph || !DelegateProperty || !ComponentProperty)
	{
		return false;
	}

	auto MakeRequest = [&]()
	{
		FBlueprintHelperActionResolutionRequest Request =
			MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::ComponentBoundEvent, DelegateProperty->GetName());
		AddDelegateEvidence(Request, DelegateProperty);
		AddComponentEvidence(Request, ComponentProperty);
		AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));
		return Request;
	};

	FBlueprintHelperActionResolutionRequest DuplicateFail = MakeRequest();
	DuplicateFail.ContextEvidence.Add(TEXT("event_delegate.duplicate_policy"), TEXT("fail"));
	DuplicateFail.ContextEvidence.Add(TEXT("event_delegate.existing_binding"), TEXT("true"));
	TestEqual(TEXT("duplicate fail code"), FBlueprintHelperActionResolutionCore::Resolve(DuplicateFail).ErrorCode, FString(TEXT("delegate_duplicate_binding")));

	FBlueprintHelperActionResolutionRequest DuplicateReturnExisting = MakeRequest();
	DuplicateReturnExisting.ContextEvidence.Add(TEXT("event_delegate.duplicate_policy"), TEXT("return_existing"));
	DuplicateReturnExisting.ContextEvidence.Add(TEXT("event_delegate.existing_binding_evidence_id"), TEXT("existing:overlap"));
	const FBlueprintHelperActionResolutionResult ReturnExistingResult =
		FBlueprintHelperActionResolutionCore::Resolve(DuplicateReturnExisting);
	TestEqual(TEXT("return existing status"), ReturnExistingResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("return existing stable id"), ReturnExistingResult.SelectedStableId, FString(TEXT("existing:overlap")));
	if (ReturnExistingResult.CandidateActions.Num() == 1)
	{
		TestEqual(TEXT("return existing reason"), ReturnExistingResult.CandidateActions[0].MatchReason, FString(TEXT("existing_component_bound_event_binding")));
	}

	for (const TCHAR* Policy : { TEXT("replace"), TEXT("merge") })
	{
		FBlueprintHelperActionResolutionRequest Blocked = MakeRequest();
		Blocked.ContextEvidence.Add(TEXT("event_delegate.duplicate_policy"), Policy);
		TestEqual(
			FString::Printf(TEXT("duplicate %s blocked"), Policy),
			FBlueprintHelperActionResolutionCore::Resolve(Blocked).ErrorCode,
			FString(TEXT("duplicate_mutation_policy_blocked")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateAssignSideEffectBlockedTest,
	"BlueprintHelper.GraphWrite.ActionResolution.EventDelegate.Policy.AssignSideEffectBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateAssignSideEffectBlockedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeEventDelegateActionTestBlueprint();
	UEdGraph* Graph = GetEventDelegateActionTestGraph(Blueprint);
	FMulticastDelegateProperty* DelegateProperty =
		FindRequiredDelegateProperty(*this, UPrimitiveComponent::StaticClass(), TEXT("OnComponentBeginOverlap"));
	if (!Blueprint || !Graph || !DelegateProperty)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeEventDelegateActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Delegate, DelegateProperty->GetName());
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_kind"), TEXT("component_ref"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("component_ref:CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.binding_object_path"), TEXT("CollisionComponent"));
	Request.ContextEvidence.Add(TEXT("event_delegate.operation"), TEXT("assign"));
	Request.ContextEvidence.Add(TEXT("event_delegate.assign_factory"), TEXT("ue_assign_spawner"));
	AddDelegateEvidence(Request, DelegateProperty);
	AddHandlerEvidence(Request, AActor::StaticClass(), TEXT("K2_DestroyActor"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("assign side effect blocked"), Result.ErrorCode, FString(TEXT("assign_side_effect_blocked")));
	TestNotEqual(TEXT("assign side effect not resolved"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
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
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_ComponentBoundEvent>(*this, Fragment, TEXT("component-bound event node"));
	TestNotNull(TEXT("component-bound primary node"), Fragment.PrimaryNode);
	TestEqual(TEXT("fragment node count"), Fragment.Nodes.Num(), 1);
	TestEqual(TEXT("fragment source statement id"), Fragment.SourceStatementId, Statement.StatementId);
	TestTrue(TEXT("fragment review target includes statement id"), Fragment.ReviewTargets.Contains(Statement.StatementId));
	TestEqual(TEXT("ownership statement id"), Fragment.OwnershipTags.FindRef(TEXT("statement_id")), Statement.StatementId);
	TestEqual(TEXT("ownership semantic kind"), Fragment.OwnershipTags.FindRef(TEXT("semantic_kind")), FString(TEXT("component_bound_event")));
	if (EventNode)
	{
#if BLUEPRINTHELPER_UE_HAS_COMPONENT_BOUND_EVENT_GETTER
		TestEqual(TEXT("component property name"), EventNode->GetComponentPropertyName(), FName(TEXT("CollisionComponent")));
#else
		TestEqual(TEXT("component property name"), EventNode->ComponentPropertyName, FName(TEXT("CollisionComponent")));
#endif
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
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_AssignDelegate>(*this, Fragment, TEXT("assign node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
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
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_AddDelegate>(*this, Fragment, TEXT("bind node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
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
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_RemoveDelegate>(*this, Fragment, TEXT("unbind node"));
	UK2Node_CreateDelegate* CreateDelegateNode =
		FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_CreateDelegate>(*this, Fragment, TEXT("create delegate node"));
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

	FBlueprintHelperGraphStatementIR Statement = MakeDelegateStatement(
		TEXT("stmt_delegate_call_fragment"),
		TEXT("OnComponentBeginOverlap"),
		TEXT("call"),
		TEXT("CollisionComponent"));
	TSharedPtr<FBlueprintHelperGraphExpressionIR> SweepArg = MakeShared<FBlueprintHelperGraphExpressionIR>();
	SweepArg->Kind = EBlueprintHelperGraphExpressionKind::Literal;
	SweepArg->LiteralValue = TEXT("true");
	SweepArg->Type = TEXT("bool");
	Statement.Args.Add(TEXT("bFromSweep"), SweepArg);

	FBlueprintHelperActionContextScope ActionContextScope;
	FString ScopeError;
	TestTrue(
		TEXT("build call action context scope"),
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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

	FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_CallDelegate>(*this, Fragment, TEXT("call node"));
	TestEqual(TEXT("call fragment node count"), Fragment.Nodes.Num(), 1);
	TestEqual(TEXT("call fragment internal link count"), Fragment.InternalLinks.Num(), 0);
	TestEqual(TEXT("call ownership delegate operation"), Fragment.OwnershipTags.FindRef(TEXT("delegate_operation")), FString(TEXT("call")));
	TestTrue(TEXT("call arg pin is recorded"), Fragment.PinBindings.Contains(TEXT("call_arg.bFromSweep")));
	if (const FBlueprintHelperFragmentPinRef* CallArgPin = Fragment.PinBindings.Find(TEXT("call_arg.bFromSweep")))
	{
		TestEqual(TEXT("call arg pin default is applied"), CallArgPin->Pin ? CallArgPin->Pin->DefaultValue : FString(), FString(TEXT("true")));
	}
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
		FBlueprintHelperGraphWriteTestUtils::BuildActionContextScopeForStatement(*this, Blueprint, Graph, Statement, TEXT("action context demands exist"), TEXT("event_delegate_fragment_tests"), TEXT("gap5_task4"), ActionContextScope, ScopeError));
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

	FBlueprintHelperGraphWriteTestUtils::FindSingleFragmentNode<UK2Node_ClearDelegate>(*this, Fragment, TEXT("clear node"));
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
