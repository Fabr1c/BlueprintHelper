#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallDelegate.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateDebugBundleErrorCodeTest,
	"BlueprintHelper.GraphWrite.EventDelegate.DebugBundle.ErrorCodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateDebugBundleErrorCodeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphWriteCapabilityCaseResult Result;
	Result.CaseName = TEXT("event_delegate_missing_handler");
	Result.Phase = TEXT("GraphWrite");
	Result.Capability = TEXT("EventDelegate");
	Result.SemanticKind = TEXT("delegate");
	Result.ClusterKind = TEXT("EventDelegateAction");
	Result.ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence;
	Result.MissingEvidenceFields.Add(TEXT("missing_handler_evidence"));

	const TSharedRef<FJsonObject> Json =
		FBlueprintHelperGraphWriteCapabilityMetrics::ToDebugBundleFailureSummary(Result);
	const TArray<TSharedPtr<FJsonValue>>* ErrorCodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ReadbackFactKeys = nullptr;
	TestTrue(TEXT("event delegate common error codes field"), Json->TryGetArrayField(TEXT("event_delegate_common_error_codes"), ErrorCodes));
	TestTrue(TEXT("event delegate error codes are populated"), ErrorCodes && ErrorCodes->Num() > 0);
	TestTrue(TEXT("event delegate readback fact keys field"), Json->TryGetArrayField(TEXT("event_delegate_readback_fact_keys"), ReadbackFactKeys));
	TestTrue(TEXT("event delegate readback fact keys are populated"), ReadbackFactKeys && ReadbackFactKeys->Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateReadbackFactsTest,
	"BlueprintHelper.GraphWrite.EventDelegate.DebugBundle.ReadbackFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateReadbackFactsTest::RunTest(const FString& Parameters)
{
	UK2Node_CallDelegate* CallNode = NewObject<UK2Node_CallDelegate>(GetTransientPackage());
	CallNode->CreateNewGuid();
	FEdGraphPinType BoolPinType;
	BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	UEdGraphPin* ArgPin = CallNode->CreatePin(EGPD_Input, BoolPinType, FName(TEXT("bSweep")));
	ArgPin->DefaultValue = TEXT("true");
	UEdGraphNode* SourceNode = NewObject<UEdGraphNode>(GetTransientPackage());
	UEdGraphPin* SourcePin = SourceNode->CreatePin(EGPD_Output, BoolPinType, FName(TEXT("bSource")));
	FBlueprintHelperVersionCompat::MakePinLinkTo(ArgPin, SourcePin, true);

	FBlueprintHelperNodeFragment Fragment;
	Fragment.FragmentId = TEXT("stmt_delegate_call");
	Fragment.SourceStatementId = TEXT("stmt_delegate_call");
	Fragment.PrimaryNode = CallNode;
	Fragment.Nodes.Add(CallNode);
	Fragment.PinBindings.Add(
		TEXT("call_arg.bSweep"),
		FBlueprintHelperFragmentPinRef{ TEXT("call_arg"), TEXT("bSweep"), TEXT("bool"), ArgPin });

	FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
	Evidence.SemanticKind = EBlueprintHelperActionSemanticKind::Delegate;
	Evidence.DelegateOperation = TEXT("call");
	Evidence.DelegateOwnerClassPath = TEXT("/Script/Engine.PrimitiveComponent");
	Evidence.DelegatePropertyPath = TEXT("/Script/Engine.PrimitiveComponent:OnComponentBeginOverlap");
	Evidence.DelegateSignatureFunctionPath = TEXT("/Script/Engine.ComponentBeginOverlapSignature__DelegateSignature");
	Evidence.BindingObjectKind = TEXT("component_ref");
	Evidence.BindingObjectEvidenceId = TEXT("component_ref:CollisionComponent");

	const FBlueprintHelperEventDelegateReadbackFacts Facts =
		FBlueprintHelperEventDelegateReadback::Collect(Fragment, Evidence);
	TestTrue(TEXT("node class fact"), Facts.Facts.FindRef(TEXT("node_class")).Contains(TEXT("K2Node_CallDelegate")));
	TestEqual(TEXT("spawner/factory fact"), Facts.Facts.FindRef(TEXT("spawner_or_factory_kind")), FString(TEXT("ue_delegate_node_spawner")));
	TestEqual(TEXT("binding object kind"), Facts.Facts.FindRef(TEXT("binding_object_kind")), FString(TEXT("component_ref")));
	TestEqual(TEXT("statement id"), Facts.Facts.FindRef(TEXT("statement_id")), FString(TEXT("stmt_delegate_call")));
	TestFalse(TEXT("node guid fact"), Facts.Facts.FindRef(TEXT("node_guid")).IsEmpty());
	TestFalse(TEXT("diagnostic correlation fact"), Facts.Facts.FindRef(TEXT("compile_diagnostic_correlation_key")).IsEmpty());
	TestEqual(TEXT("call arg pin name"), Facts.Facts.FindRef(TEXT("pin.call_arg.bSweep.name")), FString(TEXT("bSweep")));
	TestEqual(TEXT("call arg default"), Facts.Facts.FindRef(TEXT("pin.call_arg.bSweep.default")), FString(TEXT("true")));
	TestEqual(TEXT("call arg linked source"), Facts.Facts.FindRef(TEXT("pin.call_arg.bSweep.linked_source_pin")), FString(TEXT("bSource")));
	TestFalse(TEXT("Review target is not delegate-scoped"), Fragment.ReviewTargets.Contains(TEXT("delegate.call")));
	return true;
}

#endif
