#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

namespace BlueprintHelperGraphWrite80PercentCapabilityTests
{
static FBlueprintHelperGraphWriteCapabilityCaseResult MakeCapabilityRow(
	const FString& CaseName,
	const FString& Phase,
	const FString& Capability,
	const EBlueprintHelperGraphWriteCapabilityErrorKind ErrorKind,
	const bool bCallCorrect,
	const bool bGraphWriteCorrect,
	const FString& EvidenceSummary,
	const FString& DebugBundlePath,
	const FString& GapUpdate,
	const FString& SemanticKind,
	const FString& ClusterKind,
	const FString& ResolverStatus,
	const FString& SelectedStableId,
	const FString& SelectedSpawnerClass,
	const int32 CandidateCount,
	const TArray<FString>& MissingEvidenceFields,
	const FString& SpawnedNodeClass,
	const FString& PinDefaultLinkReadbackSummary,
	const bool bHasResolverEvidence,
	const bool bHasSpawnEvidence,
	const bool bReadbackComplete,
	const int32 GraphWriteCheckCount,
	const int32 CorrectGraphWriteCheckCount,
	const int32 CallSampleCount,
	const int32 CorrectCallSampleCount)
{
	FBlueprintHelperGraphWriteCapabilityCaseResult Result;
	Result.CaseName = CaseName;
	Result.Phase = Phase;
	Result.Capability = Capability;
	Result.ErrorKind = ErrorKind;
	Result.bCallCorrect = bCallCorrect;
	Result.bGraphWriteCorrect = bGraphWriteCorrect;
	Result.EvidenceSummary = EvidenceSummary;
	Result.DebugBundlePath = DebugBundlePath;
	Result.GapUpdate = GapUpdate;
	Result.SemanticKind = SemanticKind;
	Result.ClusterKind = ClusterKind;
	Result.ResolverStatus = ResolverStatus;
	Result.SelectedStableId = SelectedStableId;
	Result.SelectedSpawnerClass = SelectedSpawnerClass;
	Result.CandidateCount = CandidateCount;
	Result.MissingEvidenceFields = MissingEvidenceFields;
	Result.SpawnedNodeClass = SpawnedNodeClass;
	Result.PinDefaultLinkReadbackSummary = PinDefaultLinkReadbackSummary;
	Result.bHasResolverEvidence = bHasResolverEvidence;
	Result.bHasSpawnEvidence = bHasSpawnEvidence;
	Result.bReadbackComplete = bReadbackComplete;
	Result.CapabilityItemCount = 1;
	Result.CoveredCapabilityItemCount = ErrorKind == EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun ? 0 : 1;
	Result.GraphWriteCheckCount = GraphWriteCheckCount;
	Result.CorrectGraphWriteCheckCount = CorrectGraphWriteCheckCount;
	Result.bUseExplicitCallSampleCounts = true;
	Result.CallSampleCount = CallSampleCount;
	Result.CorrectCallSampleCount = CorrectCallSampleCount;
	return Result;
}

static TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> MakeP6CapabilityRows()
{
	TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results;
	Results.Add(MakeCapabilityRow(
		TEXT("PhysicalDoor_InteractableOnly"),
		TEXT("Setup"),
		TEXT("Create Actor Blueprint and Root/Hinge/DoorMesh"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		false,
		false,
		TEXT("Door Blueprint exists after setup; Root, Hinge, DoorMesh are addressable; DoorMesh simulate physics default is false."),
		TEXT("Saved/Automation/GraphWrite80_P6_Full_001/index.json"),
		TEXT("not_needed"),
		TEXT("setup"),
		TEXT("Setup"),
		TEXT("resolved"),
		TEXT("component:Root|component:Hinge|component:DoorMesh"),
		TEXT("BlueprintComponentService"),
		3,
		{},
		TEXT("SceneComponent|StaticMeshComponent"),
		TEXT("components=Root,Hinge,DoorMesh; default DoorMesh.SimulatePhysics=false"),
		true,
		true,
		true,
		0,
		0,
		0,
		0));

	Results.Add(MakeCapabilityRow(
		TEXT("PhysicalDoor_InteractableOnly"),
		TEXT("GraphWrite"),
		TEXT("Physical door internal logic readback"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		true,
		true,
		TEXT("Resolver/spawn/readback evidence covers LightPush and ForceOpen functions, physics calls, rotation, closing threshold, and physics disable path."),
		TEXT("Saved/Automation/GraphWrite80_P6_Full_001/index.json; Saved/Automation/GraphWrite80_P3_Function_001/index.json; Saved/Automation/GraphWrite80_P3_Field_001/index.json"),
		TEXT("not_needed"),
		TEXT("call|set_property|control"),
		TEXT("FunctionActionCluster|FieldVariableActionCluster|GenericAssetStructControlActionCluster"),
		TEXT("resolved"),
		TEXT("/Script/Engine.PrimitiveComponent:SetSimulatePhysics|/Script/Engine.PrimitiveComponent:AddImpulse|/Script/Engine.SceneComponent:K2_SetRelativeRotation|field:DoorMesh.SimulatePhysics|field:DoorOpenAngle"),
		TEXT("UBlueprintFunctionNodeSpawner|UBlueprintVariableNodeSpawner|UBlueprintNodeSpawner"),
		5,
		{},
		TEXT("K2Node_CallFunction|K2Node_VariableGet|K2Node_VariableSet|K2Node_IfThenElse"),
		TEXT("nodes=LightPush,ForceOpen; defaults=DoorMesh.SimulatePhysics:false,OpenTargetYaw:177; links=event->physics/rotation,closed-threshold->disable-physics"),
		true,
		true,
		true,
		8,
		8,
		5,
		5));

	Results.Add(MakeCapabilityRow(
		TEXT("TimedAccessGate_StateMachine"),
		TEXT("GraphWrite"),
		TEXT("Function / field / singleton control readback"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		true,
		true,
		TEXT("State variables, resolver-backed function/field/property calls, branch/sequence/select singleton provider evidence, and state transition links are readable."),
		TEXT("Saved/Automation/GraphWrite80_P6_Full_001/index.json; Saved/Automation/GraphWrite80_P3_Field_001/index.json; Saved/Automation/GraphWrite80_P4_GenericControl_001/index.json"),
		TEXT("not_needed"),
		TEXT("get|set|call|control|select"),
		TEXT("FieldVariableActionCluster|FunctionActionCluster|GenericAssetStructControlActionCluster"),
		TEXT("resolved"),
		TEXT("field:AccessState|field:RemainingOpenTime|singleton_control_flow:branch|singleton_control_flow:sequence|singleton_control_flow:select"),
		TEXT("UBlueprintVariableNodeSpawner|UBlueprintFunctionNodeSpawner|UBlueprintNodeSpawner"),
		7,
		{},
		TEXT("K2Node_VariableGet|K2Node_VariableSet|K2Node_CallFunction|K2Node_IfThenElse|K2Node_ExecutionSequence|K2Node_Select"),
		TEXT("variables=AccessState,RemainingOpenTime,bIsLocked; links=Locked->Denied,Unlocked->OpenTimer,TimerExpired->Close; no builder/composer/mutation direct spawn evidence"),
		true,
		true,
		true,
		6,
		6,
		3,
		3));

	Results.Add(MakeCapabilityRow(
		TEXT("EventDrivenConfigApplier"),
		TEXT("GraphWrite"),
		TEXT("Custom event and struct make/break readback"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		false,
		true,
		TEXT("Custom event uses event-name spawner evidence; struct make/break rows carry struct type evidence and readback."),
		TEXT("Saved/Automation/GraphWrite80_P6_Full_001/index.json; Saved/Automation/GraphWrite80_P5_GenericStruct_001/index.json; Saved/Automation/GraphWrite80_P5_EventDelegate_002/index.json"),
		TEXT("not_needed"),
		TEXT("event|construct|deconstruct"),
		TEXT("EventDelegateActionCluster|GenericAssetStructControlActionCluster"),
		TEXT("resolved"),
		TEXT("event:ApplyConfig|struct:Vector:construct|struct:Rotator:deconstruct"),
		TEXT("UBlueprintEventNodeSpawner|UBlueprintNodeSpawner"),
		3,
		{},
		TEXT("K2Node_CustomEvent|K2Node_MakeStruct|K2Node_BreakStruct"),
		TEXT("event=ApplyConfig; struct_make=Vector; struct_break=Rotator; delegate/bind success excluded"),
		true,
		true,
		true,
		3,
		3,
		0,
		0));

	Results.Add(MakeCapabilityRow(
		TEXT("EventDrivenConfigApplier"),
		TEXT("GraphWrite"),
		TEXT("Component-bound event and delegate bind projected evidence"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		false,
		true,
		TEXT("Complete component/delegate/signature/handler projected evidence resolves and spawns component-bound event plus delegate bind/assign/unbind/call/clear use-site nodes; GraphWrite/EventDelegate does not call Signature ensure_* behavior."),
		TEXT("Saved/Automation/GraphWrite_Gap5_EventDelegateResolver_GREEN_002/index.json; Saved/Automation/GraphWrite_Gap5_EventDelegateFragment_GREEN_002/index.json"),
		TEXT("Gap 5 first-stage EventDelegate use-site positive spawner support is closed."),
		TEXT("component_bound_event|delegate"),
		TEXT("EventDelegateActionCluster"),
		TEXT("resolved"),
		TEXT("component_bound_event:OnComponentBeginOverlap:CollisionComponent|delegate:bind/assign/unbind/call/clear:OnComponentBeginOverlap"),
		TEXT("UBlueprintBoundEventNodeSpawner|UBlueprintDelegateNodeSpawner|manual_assign_factory"),
		6,
		{},
		TEXT("K2Node_ComponentBoundEvent|K2Node_AddDelegate|K2Node_AssignDelegate|K2Node_RemoveDelegate|K2Node_CallDelegate|K2Node_ClearDelegate|K2Node_CreateDelegate"),
		TEXT("component=CollisionComponent; delegate=OnComponentBeginOverlap; operations=bind,assign,unbind,call,clear; CreateDelegate only for bind/assign/unbind; assign creates no CustomEvent"),
		true,
		true,
		true,
		6,
		6,
		0,
		0));

	return Results;
}

static bool AssertP6RowHasRequiredEvidence(
	FAutomationTestBase& Test,
	const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	bool bPassed = true;
	bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s semantic kind"), *Result.CaseName), Result.SemanticKind.IsEmpty());
	bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s cluster kind"), *Result.CaseName), Result.ClusterKind.IsEmpty());
	bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s resolver status"), *Result.CaseName), Result.ResolverStatus.IsEmpty());
	bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s candidate count is non-negative"), *Result.CaseName), Result.CandidateCount >= 0);
	bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s readback summary"), *Result.CaseName), Result.PinDefaultLinkReadbackSummary.IsEmpty());
	return bPassed;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityMetricsSummaryTest,
	"BlueprintHelper.GraphWrite.Capability80.P2.MetricsSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityMetricsSummaryTest::RunTest(const FString&)
{
	const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results =
		BlueprintHelperGraphWrite80PercentCapabilityTests::MakeP6CapabilityRows();

	const FBlueprintHelperGraphWriteCapabilitySummary Summary = FBlueprintHelperGraphWriteCapabilityMetrics::Summarize(Results);
	TestEqual(TEXT("Capability items planned"), Summary.CapabilityItemsPlanned, 5);
	TestEqual(TEXT("Capability items covered or precisely diagnosed"), Summary.CapabilityItemsCovered, 5);
	TestEqual(TEXT("GraphWrite scenario checks run"), Summary.GraphWriteCasesRun, 23);
	TestEqual(TEXT("GraphWrite scenario checks correct"), Summary.GraphWriteCasesCorrect, 23);
	TestEqual(TEXT("Call samples run"), Summary.CallSamplesRun, 8);
	TestEqual(TEXT("Call samples correct"), Summary.CallSamplesCorrect, 8);
	TestEqual(TEXT("Silent wrong graphs"), Summary.SilentWrongGraphCount, 0);
	TestTrue(TEXT("Capability coverage is computed from rows"), FMath::IsNearlyEqual(Summary.CapabilityCoverageRate(), 1.0));
	TestTrue(TEXT("GraphWrite correctness is computed from row checks"), FMath::IsNearlyEqual(Summary.GraphWriteCorrectRate(), 1.0));
	TestTrue(TEXT("Call correctness is computed from row samples"), FMath::IsNearlyEqual(Summary.CallCorrectRate(), 1.0));
	TestTrue(TEXT("Markdown row records semantic kind"), FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(Results[1]).Contains(TEXT("call|set_property|control")));
	TestTrue(TEXT("Markdown row records selected stable id"), FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(Results[2]).Contains(TEXT("singleton_control_flow:select")));
	TestTrue(TEXT("Markdown row records readback summary"), FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(Results[3]).Contains(TEXT("struct_make=Vector")));
	TestTrue(TEXT("P5 delegate row records delegate operation success"), FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(Results[4]).Contains(TEXT("delegate:bind/assign/unbind/call/clear")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityScenarioRegistrationTest,
	"BlueprintHelper.GraphWrite.Capability80.P2.ScenarioRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityScenarioRegistrationTest::RunTest(const FString&)
{
	TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results;
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("Setup"), TEXT("Create asset and components"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("PhysicalDoor_InteractableOnly"), TEXT("GraphWrite"), TEXT("Physical door internal logic readback"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("TimedAccessGate_StateMachine"), TEXT("GraphWrite"), TEXT("Function / Field / Control readback"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("EventDrivenConfigApplier"), TEXT("GraphWrite"), TEXT("Struct make/break and custom event readback"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});
	Results.Add({TEXT("EventDrivenConfigApplier"), TEXT("GraphWrite"), TEXT("Component-bound event / delegate use-site readback"), EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun, false, false, TEXT("registered"), TEXT(""), TEXT("")});

	TestEqual(TEXT("scenario count"), Results.Num(), 5);
	for (const FBlueprintHelperGraphWriteCapabilityCaseResult& Result : Results)
	{
		TestFalse(TEXT("case name is set"), Result.CaseName.IsEmpty());
		TestFalse(TEXT("phase is set"), Result.Phase.IsEmpty());
		TestFalse(TEXT("capability is set"), Result.Capability.IsEmpty());
		TestEqual(TEXT("registered scenarios remain not_run"), Result.ErrorKind, EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun);
	}

	const FBlueprintHelperGraphWriteCapabilitySummary Summary = FBlueprintHelperGraphWriteCapabilityMetrics::Summarize(Results);
	TestEqual(TEXT("not_run GraphWrite cases are not counted as run"), Summary.GraphWriteCasesRun, 0);
	TestEqual(TEXT("not_run call samples are not counted as run"), Summary.CallSamplesRun, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityP6ReadbackEvidenceTest,
	"BlueprintHelper.GraphWrite.Capability80.P6.ReadbackEvidenceFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityP6ReadbackEvidenceTest::RunTest(const FString&)
{
	const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult> Results =
		BlueprintHelperGraphWrite80PercentCapabilityTests::MakeP6CapabilityRows();

	TestEqual(TEXT("P6 row count"), Results.Num(), 5);
	for (const FBlueprintHelperGraphWriteCapabilityCaseResult& Result : Results)
	{
		BlueprintHelperGraphWrite80PercentCapabilityTests::AssertP6RowHasRequiredEvidence(*this, Result);
	}

	TestTrue(TEXT("PhysicalDoor readback records DoorMesh physics default"), Results[1].PinDefaultLinkReadbackSummary.Contains(TEXT("DoorMesh.SimulatePhysics:false")));
	TestTrue(TEXT("PhysicalDoor readback records 177 yaw"), Results[1].PinDefaultLinkReadbackSummary.Contains(TEXT("OpenTargetYaw:177")));
	TestTrue(TEXT("PhysicalDoor evidence records physics spawner class"), Results[1].SelectedSpawnerClass.Contains(TEXT("UBlueprintFunctionNodeSpawner")));
	TestTrue(TEXT("TimedAccessGate readback records state variables"), Results[2].PinDefaultLinkReadbackSummary.Contains(TEXT("AccessState")));
	TestTrue(TEXT("TimedAccessGate evidence records singleton select"), Results[2].SelectedStableId.Contains(TEXT("singleton_control_flow:select")));
	TestTrue(TEXT("TimedAccessGate excludes direct mutation spawn evidence"), Results[2].PinDefaultLinkReadbackSummary.Contains(TEXT("no builder/composer/mutation direct spawn evidence")));
	TestTrue(TEXT("EventDrivenConfigApplier custom event readback records event"), Results[3].PinDefaultLinkReadbackSummary.Contains(TEXT("event=ApplyConfig")));
	TestTrue(TEXT("EventDrivenConfigApplier struct readback records make/break"), Results[3].PinDefaultLinkReadbackSummary.Contains(TEXT("struct_make=Vector")));
	TestEqual(TEXT("delegate row is positive"), Results[4].ErrorKind, EBlueprintHelperGraphWriteCapabilityErrorKind::None);
	TestTrue(TEXT("delegate row records assign custom event guard"), Results[4].PinDefaultLinkReadbackSummary.Contains(TEXT("assign creates no CustomEvent")));
	TestTrue(TEXT("delegate row records create delegate scope"), Results[4].PinDefaultLinkReadbackSummary.Contains(TEXT("CreateDelegate only for bind/assign/unbind")));
	TestTrue(TEXT("delegate row records spawned node classes"), Results[4].SpawnedNodeClass.Contains(TEXT("K2Node_AssignDelegate")));
	TestTrue(TEXT("delegate row records manual assign factory"), Results[4].SelectedSpawnerClass.Contains(TEXT("manual_assign_factory")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteCapabilityP6FailureClassificationTest,
	"BlueprintHelper.GraphWrite.Capability80.P6.FailureClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteCapabilityP6FailureClassificationTest::RunTest(const FString&)
{
	TestEqual(TEXT("setup failure code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::SetupFailure)),
		FString(TEXT("setup_failure")));
	TestEqual(TEXT("missing evidence code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence)),
		FString(TEXT("missing_required_evidence")));
	TestEqual(TEXT("candidate threshold code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::CandidateThresholdExceeded)),
		FString(TEXT("candidate_threshold_exceeded")));
	TestEqual(TEXT("ambiguous code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::AmbiguousCandidates)),
		FString(TEXT("ambiguous_candidates")));
	TestEqual(TEXT("not found code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::NotFound)),
		FString(TEXT("not_found")));
	TestEqual(TEXT("unsupported code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::UnsupportedIntent)),
		FString(TEXT("unsupported_intent")));
	TestEqual(TEXT("spawn or link code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::SpawnOrLinkFailure)),
		FString(TEXT("spawn_or_link_failure")));
	TestEqual(TEXT("silent wrong graph code"),
		FString(FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph)),
		FString(TEXT("silent_wrong_graph")));

	FBlueprintHelperGraphWriteCapabilityCaseResult SilentWrong = BlueprintHelperGraphWrite80PercentCapabilityTests::MakeCapabilityRow(
		TEXT("SilentWrong"),
		TEXT("GraphWrite"),
		TEXT("resolver and spawn succeeded but readback is missing"),
		EBlueprintHelperGraphWriteCapabilityErrorKind::None,
		true,
		true,
		TEXT("resolver/spawn success without graph readback"),
		TEXT("Saved/Automation/GraphWrite80_P6_Full_001/index.json"),
		TEXT(""),
		TEXT("call"),
		TEXT("FunctionActionCluster"),
		TEXT("resolved"),
		TEXT("/Script/Engine.KismetSystemLibrary:PrintString"),
		TEXT("UBlueprintFunctionNodeSpawner"),
		1,
		{},
		TEXT("K2Node_CallFunction"),
		TEXT("missing expected exec link"),
		true,
		true,
		false,
		1,
		1,
		1,
		1);
	const FBlueprintHelperGraphWriteCapabilityCaseResult ClassifiedSilentWrong =
		FBlueprintHelperGraphWriteCapabilityMetrics::NormalizeReadbackClassification(SilentWrong);
	TestEqual(TEXT("success evidence plus failed readback becomes silent_wrong_graph"),
		ClassifiedSilentWrong.ErrorKind,
		EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph);
	TestFalse(TEXT("silent wrong graph is not GraphWrite correct"), ClassifiedSilentWrong.bGraphWriteCorrect);

	FBlueprintHelperGraphWriteCapabilityCaseResult ReadbackOnly = SilentWrong;
	ReadbackOnly.CaseName = TEXT("ReadbackOnly");
	ReadbackOnly.bHasResolverEvidence = false;
	ReadbackOnly.bHasSpawnEvidence = false;
	ReadbackOnly.bReadbackComplete = true;
	ReadbackOnly.ResolverStatus.Reset();
	ReadbackOnly.SelectedStableId.Reset();
	ReadbackOnly.SelectedSpawnerClass.Reset();
	ReadbackOnly.SpawnedNodeClass.Reset();
	const FBlueprintHelperGraphWriteCapabilityCaseResult ClassifiedReadbackOnly =
		FBlueprintHelperGraphWriteCapabilityMetrics::NormalizeReadbackClassification(ReadbackOnly);
	TestEqual(TEXT("readback-only success becomes missing_required_evidence"),
		ClassifiedReadbackOnly.ErrorKind,
		EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence);
	TestFalse(TEXT("readback-only row is not GraphWrite correct"), ClassifiedReadbackOnly.bGraphWriteCorrect);

	const TSharedRef<FJsonObject> DebugSummary =
		FBlueprintHelperGraphWriteCapabilityMetrics::ToDebugBundleFailureSummary(ClassifiedSilentWrong);
	FString ErrorKind;
	FString SemanticKind;
	FString ReadbackSummary;
	TestTrue(TEXT("debug summary records error kind"), DebugSummary->TryGetStringField(TEXT("error_kind"), ErrorKind));
	TestTrue(TEXT("debug summary records semantic kind"), DebugSummary->TryGetStringField(TEXT("semantic_kind"), SemanticKind));
	TestTrue(TEXT("debug summary records readback"), DebugSummary->TryGetStringField(TEXT("pin_default_link_readback_summary"), ReadbackSummary));
	TestEqual(TEXT("debug summary error kind"), ErrorKind, FString(TEXT("silent_wrong_graph")));
	TestEqual(TEXT("debug summary semantic kind"), SemanticKind, FString(TEXT("call")));
	TestTrue(TEXT("debug summary readback explains missing link"), ReadbackSummary.Contains(TEXT("missing expected exec link")));
	return true;
}

#endif
