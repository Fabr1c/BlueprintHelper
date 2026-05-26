#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateNamespacedActionContextTest,
	"BlueprintHelper.GraphWrite.EventDelegate.ActionContext.NamespacedProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateNamespacedActionContextTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_delegate_bind");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Delegate;
	Demand.BindingObjectPath = TEXT("self");
	Demand.DelegateName = TEXT("OnDoorStateChanged");
	Demand.DelegateOperation = TEXT("bind");
	Demand.HandlerName = TEXT("HandleDoorStateChanged");
	Demand.HandlerFunctionPath = TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged");
	Demand.HandlerSourceCluster = TEXT("BlueprintSignature");
	Demand.SignatureEvidenceId = TEXT("signature:custom_event:HandleDoorStateChanged");

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	Snapshot.Graph.BlueprintClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("operation key"), Context.Evidence.FindRef(TEXT("event_delegate.operation")), FString(TEXT("bind")));
	TestEqual(TEXT("binding object kind"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_kind")), FString(TEXT("self")));
	TestEqual(TEXT("binding object evidence"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_evidence_id")), FString(TEXT("self:self")));
	TestEqual(TEXT("handler path"), Context.Evidence.FindRef(TEXT("event_delegate.handler_function_path")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged")));
	TestEqual(TEXT("signature id"), Context.Evidence.FindRef(TEXT("event_delegate.signature_evidence_id")), FString(TEXT("signature:custom_event:HandleDoorStateChanged")));
	TestFalse(TEXT("bare delegate_operation not projected"), Context.Evidence.Contains(TEXT("delegate_operation")));
	TestFalse(TEXT("bare binding_object_path not projected"), Context.Evidence.Contains(TEXT("binding_object_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateExplicitBindingObjectProjectionTest,
	"BlueprintHelper.GraphWrite.EventDelegate.ActionContext.BindingObjectProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateExplicitBindingObjectProjectionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_delegate_call");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Delegate;
	Demand.BindingObjectPath = TEXT("CollisionComponent");
	Demand.DelegateName = TEXT("OnComponentBeginOverlap");
	Demand.DelegateOperation = TEXT("call");
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_kind"), TEXT("linked_pin_ref"));
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("linked_pin_ref:source_node:Target"));
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_node_guid"), TEXT("0123456789abcdef0123456789abcdef"));
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_pin_name"), TEXT("Target"));
	Demand.DefaultValues.Add(TEXT("event_delegate.duplicate_policy"), TEXT("return_existing"));

	FBlueprintHelperActionContextSnapshot Snapshot;
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("explicit binding kind wins"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_kind")), FString(TEXT("linked_pin_ref")));
	TestEqual(TEXT("explicit binding evidence id wins"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_evidence_id")), FString(TEXT("linked_pin_ref:source_node:Target")));
	TestEqual(TEXT("linked pin node guid projected"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_node_guid")), FString(TEXT("0123456789abcdef0123456789abcdef")));
	TestEqual(TEXT("linked pin name projected"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_pin_name")), FString(TEXT("Target")));
	TestEqual(TEXT("duplicate policy projected"), Context.Evidence.FindRef(TEXT("event_delegate.duplicate_policy")), FString(TEXT("return_existing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateCrossStatementBindingObjectProjectionTest,
	"BlueprintHelper.GraphWrite.EventDelegate.ActionContext.CrossStatementBindingObjectRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateCrossStatementBindingObjectProjectionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_delegate_call");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Delegate;
	Demand.DelegateName = TEXT("OnComponentBeginOverlap");
	Demand.DelegateOperation = TEXT("call");
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_kind"), TEXT("function_return_ref"));
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_evidence_id"), TEXT("function_return_ref:producer_stmt:return"));
	Demand.DefaultValues.Add(TEXT("event_delegate.binding_object_statement_id"), TEXT("producer_stmt"));

	FBlueprintHelperActionContextSnapshot Snapshot;
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("function return binding kind"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_kind")), FString(TEXT("function_return_ref")));
	TestEqual(TEXT("producer statement id"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_statement_id")), FString(TEXT("producer_stmt")));
	TestEqual(TEXT("cross-statement temporary rejected"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_error")), FString(TEXT("binding_object_cross_statement_unsupported")));
	return true;
}

#endif
