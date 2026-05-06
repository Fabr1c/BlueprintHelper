#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Services/Review/BlueprintHelperReviewActionService.h"
#include "Services/Review/BlueprintHelperReviewStoreService.h"
#include "Structure/Review/BlueprintHelperReviewTypes.h"
#include "Widgets/Review/BlueprintHelperReviewGraphBounds.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Review/SBlueprintHelperReviewPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewColorMappingTest,
	"BlueprintHelper.Review.VisibleChange.ColorMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewColorMappingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("added changes render green"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::Added)),
		FString(TEXT("green")));
	TestEqual(TEXT("removed changes render red"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::Removed)),
		FString(TEXT("red")));
	TestEqual(TEXT("variable modifications render yellow"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::VariableModified)),
		FString(TEXT("yellow")));
	TestEqual(TEXT("signature modifications render yellow"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::SignatureModified)),
		FString(TEXT("yellow")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceClassificationTest,
	"BlueprintHelper.Review.VisibleChange.SurfaceClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewSurfaceClassificationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange ComponentChange;
	ComponentChange.LocationKey = TEXT("component:FakeDiffComponent");
	TestTrue(TEXT("component changes render in Components"),
		BlueprintHelperReviewShouldShowInComponents(ComponentChange));
	TestFalse(TEXT("component changes do not render as My Blueprint entries"),
		BlueprintHelperReviewShouldShowInMyBlueprint(ComponentChange));

	FBlueprintHelperReviewVisibleChange GraphChange;
	GraphChange.GraphName = TEXT("FakeDiffGraph");
	GraphChange.LocationKey = TEXT("graph:FakeDiffGraph/node:PrintString");
	TestTrue(TEXT("graph changes render in the graph page"),
		BlueprintHelperReviewShouldShowInGraph(GraphChange));
	TestFalse(TEXT("graph changes do not cover My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(GraphChange));

	FBlueprintHelperReviewAtomicTarget GraphTarget;
	GraphTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphTarget.TargetKey = TEXT("node:PrintString");
	GraphChange.AtomicTargets.Add(GraphTarget);
	TestFalse(TEXT("explicit graph atoms stay out of My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(GraphChange));

	FBlueprintHelperReviewVisibleChange EventGraphChange;
	EventGraphChange.GraphName = TEXT("EventGraph");
	EventGraphChange.LocationKey = TEXT("graph:EventGraph/node:BeginPlay");
	TestFalse(TEXT("EventGraph node changes do not match top-level event rows"),
		BlueprintHelperReviewShouldShowInMyBlueprint(EventGraphChange));

	FBlueprintHelperReviewVisibleChange SignatureChange;
	SignatureChange.LocationKey = TEXT("function:FakeDiffFunction:signature");
	SignatureChange.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	TestTrue(TEXT("signature changes render in My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(SignatureChange));
	TestTrue(TEXT("signature changes render in Details"),
		BlueprintHelperReviewShouldShowInDetails(SignatureChange));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRenameExpansionTest,
	"BlueprintHelper.Review.VisibleChange.RenameRendersAsDeleteAndAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRenameExpansionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput Rename;
	Rename.TransactionId = TEXT("tx_rename");
	Rename.AssetPath = TEXT("/Game/BP_Door");
	Rename.LocationKey = TEXT("my_blueprint:function:OpenDoor");
	Rename.ChangeKind = EBlueprintHelperReviewChangeKind::Renamed;
	Rename.DisplayLabel = TEXT("OpenDoor -> OpenDoorFast");
	Rename.BeforeSummary = TEXT("OpenDoor");
	Rename.AfterSummary = TEXT("OpenDoorFast");

	TArray<FBlueprintHelperReviewTransactionInput> RenameInputs;
	RenameInputs.Add(Rename);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(RenameInputs);

	TestEqual(TEXT("rename expands to delete plus add"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestEqual(TEXT("first rename half is removed"), Changes[0].ChangeKind, EBlueprintHelperReviewChangeKind::Removed);
		TestEqual(TEXT("second rename half is added"), Changes[1].ChangeKind, EBlueprintHelperReviewChangeKind::Added);
		TestEqual(TEXT("removed side keeps before name"), Changes[0].BeforeSummary, FString(TEXT("OpenDoor")));
		TestEqual(TEXT("added side keeps after name"), Changes[1].AfterSummary, FString(TEXT("OpenDoorFast")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewVisibleChangeCollapseTest,
	"BlueprintHelper.Review.VisibleChange.CollapsesSupersededTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewVisibleChangeCollapseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.GraphName = TEXT("EventGraph");
	T1.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	T1.DisplayLabel = TEXT("PrintString InString");
	T1.BeforeSummary = TEXT("Open");
	T1.AfterSummary = TEXT("Opening");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("Opening");
	T2.AfterSummary = TEXT("Door Opened");

	TArray<FBlueprintHelperReviewTransactionInput> CollapseInputs;
	CollapseInputs.Add(T1);
	CollapseInputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(CollapseInputs);

	TestEqual(TEXT("one final visible change remains"), Changes.Num(), 1);
	if (Changes.Num() == 1)
	{
		const FBlueprintHelperReviewVisibleChange& Change = Changes[0];
		TestEqual(TEXT("latest transaction wins"), Change.LatestTransactionId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("baseline before summary is preserved"), Change.BeforeSummary, FString(TEXT("Open")));
		TestEqual(TEXT("final after summary is preserved"), Change.AfterSummary, FString(TEXT("Door Opened")));
		TestEqual(TEXT("source transaction chain kept"), Change.SourceTransactionIds.Num(), 2);
		if (Change.SourceTransactionIds.Num() == 2)
		{
			TestEqual(TEXT("first source is T1"), Change.SourceTransactionIds[0], FString(TEXT("tx_t1")));
			TestEqual(TEXT("second source is T2"), Change.SourceTransactionIds[1], FString(TEXT("tx_t2")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAtomicIntersectionCollapseTest,
	"BlueprintHelper.Review.VisibleChange.AtomicIntersectionUsesLatestTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAtomicIntersectionCollapseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget T1Node1;
	T1Node1.Surface = EBlueprintHelperReviewSurface::Graph;
	T1Node1.GraphName = TEXT("EventGraph");
	T1Node1.TargetKey = TEXT("node:N1");
	T1Node1.VisualGroupKey = TEXT("graph:EventGraph/block:DoorFlow");
	T1Node1.DisplayLabel = TEXT("Door flow");

	FBlueprintHelperReviewAtomicTarget T1Node2 = T1Node1;
	T1Node2.TargetKey = TEXT("node:N2");

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.GraphName = TEXT("EventGraph");
	T1.LocationKey = TEXT("graph:EventGraph/block:DoorFlow");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	T1.DisplayLabel = TEXT("Door flow");
	T1.BeforeSummary = TEXT("A");
	T1.AfterSummary = TEXT("B");
	T1.AtomicTargets.Add(T1Node1);
	T1.AtomicTargets.Add(T1Node2);

	FBlueprintHelperReviewAtomicTarget T2Node2 = T1Node2;
	FBlueprintHelperReviewAtomicTarget T2Node3 = T1Node1;
	T2Node3.TargetKey = TEXT("node:N3");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("B");
	T2.AfterSummary = TEXT("C");
	T2.AtomicTargets.Reset();
	T2.AtomicTargets.Add(T2Node2);
	T2.AtomicTargets.Add(T2Node3);

	TArray<FBlueprintHelperReviewTransactionInput> Inputs;
	Inputs.Add(T1);
	Inputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(Inputs);

	TestEqual(TEXT("one visual block remains"), Changes.Num(), 1);
	if (Changes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& Change = Changes[0];
	TestEqual(TEXT("merged block keeps three atomic targets"), Change.AtomicTargets.Num(), 3);
	TestTrue(TEXT("leaf records both latest transactions"),
		Change.LatestTransactionIds.Contains(TEXT("tx_t1"))
		&& Change.LatestTransactionIds.Contains(TEXT("tx_t2")));

	const FBlueprintHelperReviewAtomicTarget* Node1 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N1");
		});
	const FBlueprintHelperReviewAtomicTarget* Node2 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N2");
		});
	const FBlueprintHelperReviewAtomicTarget* Node3 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N3");
		});

	TestNotNull(TEXT("N1 remains visible"), Node1);
	TestNotNull(TEXT("N2 remains visible"), Node2);
	TestNotNull(TEXT("N3 remains visible"), Node3);
	if (Node1 && Node2 && Node3)
	{
		TestEqual(TEXT("N1 belongs to T1"), Node1->LatestTransactionId, FString(TEXT("tx_t1")));
		TestEqual(TEXT("N2 intersection belongs to T2"), Node2->LatestTransactionId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("N3 belongs to T2"), Node3->LatestTransactionId, FString(TEXT("tx_t2")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMultiSurfaceTreeModelTest,
	"BlueprintHelper.Review.VisibleChange.MultiSurfaceLeafTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMultiSurfaceTreeModelTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput ComponentChange;
	ComponentChange.TransactionId = TEXT("tx_component");
	ComponentChange.AssetPath = TEXT("/Game/BP_Door");
	ComponentChange.LocationKey = TEXT("component:DoorMesh");
	ComponentChange.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	ComponentChange.DisplayLabel = TEXT("DoorMesh");
	FBlueprintHelperReviewAtomicTarget ComponentTarget;
	ComponentTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ComponentTarget.TargetKey = TEXT("component:DoorMesh");
	ComponentTarget.ComponentPath = TEXT("DefaultSceneRoot/DoorMesh");
	ComponentTarget.VisualGroupKey = TEXT("component:DoorMesh");
	ComponentChange.AtomicTargets.Add(ComponentTarget);

	FBlueprintHelperReviewTransactionInput PropertyChange;
	PropertyChange.TransactionId = TEXT("tx_property");
	PropertyChange.AssetPath = TEXT("/Game/BP_Door");
	PropertyChange.LocationKey = TEXT("property:SmokeValue");
	PropertyChange.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	PropertyChange.DisplayLabel = TEXT("SmokeValue");
	FBlueprintHelperReviewAtomicTarget PropertyTarget;
	PropertyTarget.Surface = EBlueprintHelperReviewSurface::Details;
	PropertyTarget.TargetKey = TEXT("property:SmokeValue");
	PropertyTarget.PropertyPath = TEXT("Smoke.SmokeValue");
	PropertyTarget.VisualGroupKey = TEXT("property:SmokeValue");
	PropertyChange.AtomicTargets.Add(PropertyTarget);

	TArray<FBlueprintHelperReviewTransactionInput> Inputs;
	Inputs.Add(ComponentChange);
	Inputs.Add(PropertyChange);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(Inputs);

	TestEqual(TEXT("two leaves for one asset"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestTrue(TEXT("component leaf routes to Components"),
			BlueprintHelperReviewShouldShowInComponents(Changes[0])
			|| BlueprintHelperReviewShouldShowInComponents(Changes[1]));
		TestTrue(TEXT("property leaf routes to Details"),
			BlueprintHelperReviewShouldShowInDetails(Changes[0])
			|| BlueprintHelperReviewShouldShowInDetails(Changes[1]));
		TestEqual(TEXT("both leaves have the same asset"), Changes[0].AssetPath, FString(TEXT("/Game/BP_Door")));
		TestEqual(TEXT("both leaves have the same asset"), Changes[1].AssetPath, FString(TEXT("/Game/BP_Door")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptVisibleChangeTest,
	"BlueprintHelper.Review.Action.AcceptUsesFinalVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAcceptVisibleChangeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewActionService ActionService;

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.LocationKey = TEXT("function:OpenDoor:signature");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	T1.DisplayLabel = TEXT("OpenDoor signature");
	T1.BeforeSummary = TEXT("OpenDoor()");
	T1.AfterSummary = TEXT("OpenDoor(Input)");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.AfterSummary = TEXT("OpenDoor(Input, Speed)");

	TArray<FBlueprintHelperReviewTransactionInput> CollapseInputs;
	CollapseInputs.Add(T1);
	CollapseInputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(CollapseInputs);
	TestEqual(TEXT("one final visible change"), Changes.Num(), 1);
	if (Changes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptVisibleChange(Changes[0]);
	TestTrue(TEXT("accept succeeds for final visible change"), Result.bSucceeded);
	TestEqual(TEXT("accept targets latest transaction"), Result.TargetTransactionId, FString(TEXT("tx_t2")));
	TestEqual(TEXT("accept marks change accepted"), Result.NewStatus, EBlueprintHelperReviewChangeStatus::Accepted);
	TestTrue(TEXT("superseded transaction data is compaction eligible"), Result.bSupersededDataCompactionEligible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeTest,
	"BlueprintHelper.Review.Action.RejectRequestsArchiveBaselineRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMixedLatestAcceptTest,
	"BlueprintHelper.Review.Action.MixedLatestLeafAcceptsWholeLeaf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMixedLatestAcceptTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_t2");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LatestTransactionId = TEXT("tx_t2");
	Change.LatestTransactionIds.Add(TEXT("tx_t1"));
	Change.LatestTransactionIds.Add(TEXT("tx_t2"));
	Change.SourceTransactionIds.Add(TEXT("tx_t1"));
	Change.SourceTransactionIds.Add(TEXT("tx_t2"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptVisibleChange(Change);

	TestTrue(TEXT("mixed latest leaf accepts as one unit"), Result.bSucceeded);
	TestFalse(TEXT("mixed latest leaf does not compact still-owned atom chains"), Result.bSupersededDataCompactionEligible);
	return true;
}

bool FBlueprintHelperReviewRejectVisibleChangeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_t2");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
	Change.LatestTransactionId = TEXT("tx_t2");
	Change.SourceTransactionIds.Add(TEXT("tx_t1"));
	Change.SourceTransactionIds.Add(TEXT("tx_t2"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	Change.BeforeSummary = TEXT("Open");
	Change.AfterSummary = TEXT("Door Opened");

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change);

	TestFalse(TEXT("first slice does not fake a completed rollback"), Result.bSucceeded);
	TestEqual(TEXT("reject enters needs action until archive rollback backend is wired"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("reject uses archive baseline rollback mode"),
		Result.RollbackMode,
		FString(TEXT("archive_baseline")));
	TestTrue(TEXT("needs action reason explains missing rollback backend"), !Result.Message.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelConstructsTest,
	"BlueprintHelper.Review.UI.PanelConstructsWithSyntheticVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelConstructsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_visible");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("function:OpenDoor:input:Input");
	Change.LatestTransactionId = TEXT("tx_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	Change.DisplayLabel = TEXT("OpenDoor Input");
	Change.BeforeSummary = TEXT("OpenDoor()");
	Change.AfterSummary = TEXT("OpenDoor(Input)");

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelEmptyConstructsTest,
	"BlueprintHelper.Review.UI.PanelConstructsWithEmptyVisibleChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelEmptyConstructsTest::RunTest(const FString& Parameters)
{
	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("empty review panel widget is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsTargetKeyTest,
	"BlueprintHelper.Review.UI.GraphBounds.UsesTargetKeyCommentStyleBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsTargetKeyTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CallFunction_1")));
	Node->NodePosX = 100;
	Node->NodePosY = 40;
	Node->NodeWidth = 240;
	Node->NodeHeight = 88;
	Graph->AddNode(Node, false, false);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("graph:EventGraph/node:K2Node_CallFunction_1");

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	const bool bBuilt = BlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("EventGraph"),
		nullptr,
		Position,
		Size);

	TestTrue(TEXT("target key matches graph node"), bBuilt);
	TestEqual(TEXT("comment-style bounds use 50px left padding"), Position.X, 50.0f);
	TestEqual(TEXT("comment-style bounds use 50px top padding"), Position.Y, -10.0f);
	TestEqual(TEXT("comment-style width wraps node plus padding"), Size.X, 340.0f);
	TestEqual(TEXT("comment-style height wraps node plus padding"), Size.Y, 188.0f);
	return true;
}

#endif
