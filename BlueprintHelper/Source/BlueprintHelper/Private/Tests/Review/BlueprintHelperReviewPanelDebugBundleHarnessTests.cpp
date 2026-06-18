#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/SBlueprintHelperMainWindow.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"

struct FBlueprintHelperReviewPanelSurfaceHarnessCase
{
	FString Name;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	FString TargetKind;
	FString TargetKeyPrefix;
};

static FBlueprintHelperReviewVisibleChange BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
	const FBlueprintHelperReviewPanelSurfaceHarnessCase& TestCase,
	const FString& Suffix,
	EBlueprintHelperReviewChangeStatus Status)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = FString::Printf(TEXT("surface_diff_%s_%s"), *TestCase.Name, *Suffix);
	Change.AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperTests/ReviewPanelSurfaceHarness/%s"),
		*TestCase.Name);
	Change.DisplayLabel = FString::Printf(TEXT("%s_%s"), *TestCase.Name, *Suffix);
	Change.LocationKey = FString::Printf(TEXT("%s:%s_%s"), *TestCase.TargetKind, *TestCase.Name, *Suffix);
	Change.LatestEvidenceId = FString::Printf(TEXT("surface_diff_evidence_%s_%s"), *TestCase.Name, *Suffix);
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.Status = Status;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = TestCase.Surface;
	Target.TargetKind = TestCase.TargetKind;
	Target.TargetKey = FString::Printf(TEXT("%s:%s_%s"), *TestCase.TargetKeyPrefix, *TestCase.Name, *Suffix);
	Target.DisplayLabel = Change.DisplayLabel;
	Target.LatestEvidenceId = Change.LatestEvidenceId;
	Target.Status = Status;
	Change.AtomicTargets.Add(Target);
	return Change;
}

static FString BlueprintHelperReviewPanelMakeMaterialInstanceParameterSnapshot(
	const FString& AssetPath,
	const FString& ParameterName,
	const FString& ParameterType,
	const FString& EffectiveValue,
	const FString& Source,
	bool bHasOverride)
{
	return FString::Printf(
		TEXT("{\"target_kind\":\"material_instance_parameter\",\"asset_path\":\"%s\",\"parameter_name\":\"%s\",\"parameter_type\":\"%s\",\"has_override\":%s,\"source\":\"%s\",\"effective_value\":\"%s\",\"override_value\":\"%s\",\"override_state\":\"%s\"}"),
		*AssetPath,
		*ParameterName,
		*ParameterType,
		bHasOverride ? TEXT("true") : TEXT("false"),
		*Source,
		*EffectiveValue,
		bHasOverride ? *EffectiveValue : TEXT("<unset>"),
		bHasOverride ? TEXT("override") : TEXT("inherited"));
}

static FBlueprintHelperReviewVisibleChange BlueprintHelperReviewPanelMakeMaterialInstanceHarnessChange(
	const FString& ChangeId,
	const FString& ParameterName,
	const FString& ParameterType,
	const FString& BeforeValue,
	const FString& AfterValue,
	EBlueprintHelperReviewChangeStatus Status)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelperTests/ReviewPanelMaterialInstance/MI_PanelHarness");
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = ChangeId;
	Change.AssetPath = AssetPath;
	Change.DisplayLabel = FString::Printf(TEXT("%s (%s)"), *ParameterName, *ParameterType);
	Change.LocationKey = FString::Printf(TEXT("material_instance_parameter:%s"), *ParameterName);
	Change.LatestEvidenceId = FString::Printf(TEXT("material_instance_panel_evidence_%s"), *ParameterName);
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.Status = Status;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Material;
	Target.TargetKind = TEXT("material_instance_parameter");
	Target.TargetSubKind = ParameterType;
	Target.TargetKey = FString::Printf(TEXT("material_instance_parameter:%s:%s"), *ParameterType, *ParameterName);
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = Change.DisplayLabel;
	Target.PropertyPath = ParameterName;
	Target.LatestEvidenceId = Change.LatestEvidenceId;
	Target.Status = Status;
	Target.BeforeSnapshotJson = BlueprintHelperReviewPanelMakeMaterialInstanceParameterSnapshot(
		AssetPath,
		ParameterName,
		ParameterType,
		BeforeValue,
		TEXT("inherited"),
		false);
	Target.AfterSnapshotJson = BlueprintHelperReviewPanelMakeMaterialInstanceParameterSnapshot(
		AssetPath,
		ParameterName,
		ParameterType,
		AfterValue,
		TEXT("override"),
		true);
	Change.AtomicTargets.Add(Target);
	return Change;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelDebugBundleHarnessTest,
	"BlueprintHelper.Review.Panel.DebugBundleHarness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelDebugBundleHarnessTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("review_panel_debug_harness_change");
	Change.AssetPath = TEXT("/Game/BlueprintHelperTests/BP_ReviewPanelDebugHarness");
	Change.DisplayLabel = TEXT("variable DebugHarnessValue");
	Change.LocationKey = TEXT("blueprint_variable:DebugHarnessValue");
	Change.LatestEvidenceId = TEXT("review_panel_debug_harness_evidence");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:DebugHarnessValue");
	Target.DisplayLabel = TEXT("DebugHarnessValue");
	Target.LatestEvidenceId = Change.LatestEvidenceId;
	Target.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SBlueprintHelperReviewPanel> Panel =
		SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestEqual(TEXT("ReviewPanel receives initial visible change"), Panel->GetVisibleChangeCountForTesting(), 1);
	TestTrue(TEXT("ReviewPanel can select the change"), Panel->SelectChangeForTesting(Change.ChangeId));

	FString BundlePath;
	FString CaptureDebugText;
	TestTrue(
		TEXT("CaptureFocus debug bundle returns a bundle path"),
		Panel->CaptureFocusDebugBundleForTesting(BundlePath, CaptureDebugText));
	TestFalse(TEXT("CaptureFocus debug bundle path is not empty"), BundlePath.IsEmpty());
	TestTrue(
		TEXT("CaptureFocus debug text records traversal"),
		CaptureDebugText.Contains(TEXT("Debug focus traversal")));
	TestTrue(
		TEXT("CaptureFocus debug bundle file exists"),
		IFileManager::Get().FileExists(*BundlePath));

	FString LoadedDebugText;
	TestTrue(
		TEXT("Embedded Debug loader reads the captured bundle"),
		Panel->LoadDebugBundleForTesting(BundlePath, LoadedDebugText));
	TestTrue(
		TEXT("Loaded debug text records bundle path"),
		LoadedDebugText.Contains(TEXT("DebugBundle loaded from")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelSurfaceDiffRefreshHarnessTest,
	"BlueprintHelper.Review.Panel.SurfaceDiffRefreshHarness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelSurfaceDiffRefreshHarnessTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperReviewPanelSurfaceHarnessCase> TestCases =
	{
		{ TEXT("Graph"), EBlueprintHelperReviewSurface::Graph, TEXT("graph_node"), TEXT("graph_node") },
		{ TEXT("Components"), EBlueprintHelperReviewSurface::Components, TEXT("component"), TEXT("component") },
		{ TEXT("MyBlueprint"), EBlueprintHelperReviewSurface::MyBlueprint, TEXT("blueprint_variable"), TEXT("blueprint_variable") },
		{ TEXT("Details"), EBlueprintHelperReviewSurface::Details, TEXT("component"), TEXT("component") },
		{ TEXT("WidgetTree"), EBlueprintHelperReviewSurface::UMGWidgetTree, TEXT("umg_widget"), TEXT("umg_widget") },
		{ TEXT("DataTable"), EBlueprintHelperReviewSurface::DataTable, TEXT("datatable_row"), TEXT("datatable_row") },
		{ TEXT("DataAsset"), EBlueprintHelperReviewSurface::DataAsset, TEXT("object_property"), TEXT("object_property") },
		{ TEXT("StructField"), EBlueprintHelperReviewSurface::DataAsset, TEXT("struct_field"), TEXT("struct_field") },
		{ TEXT("Material"), EBlueprintHelperReviewSurface::Material, TEXT("material_expression"), TEXT("material_expression") }
	};

	for (const FBlueprintHelperReviewPanelSurfaceHarnessCase& TestCase : TestCases)
	{
		FBlueprintHelperReviewVisibleChange FirstChange =
			BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
				TestCase,
				TEXT("First"),
				EBlueprintHelperReviewChangeStatus::Pending);
		FBlueprintHelperReviewVisibleChange SecondChange =
			BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
				TestCase,
				TEXT("Second"),
				EBlueprintHelperReviewChangeStatus::Pending);

		TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
		InitialChanges.Add(FirstChange);
		InitialChanges.Add(SecondChange);

		TSharedRef<SBlueprintHelperReviewPanel> Panel =
			SNew(SBlueprintHelperReviewPanel)
			.InitialChanges(InitialChanges);

		TestEqual(
			FString::Printf(TEXT("%s surface starts with two diff models"), *TestCase.Name),
			Panel->GetSurfaceDiffModelCountForTesting(TestCase.Surface),
			2);
		TArray<FString> InitialIds = Panel->GetSurfaceDiffModelIdsForTesting(TestCase.Surface);
		TestTrue(
			FString::Printf(TEXT("%s surface includes first pending row"), *TestCase.Name),
			InitialIds.Contains(FirstChange.ChangeId));
		TestTrue(
			FString::Printf(TEXT("%s surface includes second pending row"), *TestCase.Name),
			InitialIds.Contains(SecondChange.ChangeId));

		FirstChange.Status = EBlueprintHelperReviewChangeStatus::Accepted;
		FirstChange.AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Accepted;
		TArray<FBlueprintHelperReviewVisibleChange> AfterSingleAccept;
		AfterSingleAccept.Add(FirstChange);
		AfterSingleAccept.Add(SecondChange);
		Panel->RefreshVisibleChangesForTesting(AfterSingleAccept);

		TArray<FString> AfterSingleAcceptIds = Panel->GetSurfaceDiffModelIdsForTesting(TestCase.Surface);
		TestEqual(
			FString::Printf(TEXT("%s surface keeps sibling row after first row accepted"), *TestCase.Name),
			AfterSingleAcceptIds.Num(),
			1);
		TestFalse(
			FString::Printf(TEXT("%s surface removes accepted first row"), *TestCase.Name),
			AfterSingleAcceptIds.Contains(FirstChange.ChangeId));
		TestTrue(
			FString::Printf(TEXT("%s surface keeps pending second row"), *TestCase.Name),
			AfterSingleAcceptIds.Contains(SecondChange.ChangeId));

		SecondChange.Status = EBlueprintHelperReviewChangeStatus::Rejected;
		SecondChange.AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Rejected;
		TArray<FBlueprintHelperReviewVisibleChange> AfterAllClosed;
		AfterAllClosed.Add(FirstChange);
		AfterAllClosed.Add(SecondChange);
		Panel->RefreshVisibleChangesForTesting(AfterAllClosed);
		TestEqual(
			FString::Printf(TEXT("%s surface removes all closed rows"), *TestCase.Name),
			Panel->GetSurfaceDiffModelCountForTesting(TestCase.Surface),
			0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelMaterialInstanceE2EHarnessTest,
	"BlueprintHelper.Review.Panel.MaterialInstanceE2EHarness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelMaterialInstanceE2EHarnessTest::RunTest(const FString& Parameters)
{
	TSharedRef<SBlueprintHelperMainWindow> MainWindow = SNew(SBlueprintHelperMainWindow);
	MainWindow->ShowReviewPageForTesting();

	TSharedPtr<SBlueprintHelperReviewPanel> ReviewPanel = MainWindow->GetReviewPanelForTesting();
	TestTrue(TEXT("MainWindow constructs embedded ReviewPanel on Review page"), ReviewPanel.IsValid());
	if (!ReviewPanel.IsValid())
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange AcceptedOverride =
		BlueprintHelperReviewPanelMakeMaterialInstanceHarnessChange(
			TEXT("material_instance_panel_accept_scalar"),
			TEXT("PanelScalar"),
			TEXT("scalar"),
			TEXT("0.0"),
			TEXT("0.47"),
			EBlueprintHelperReviewChangeStatus::Pending);
	FBlueprintHelperReviewVisibleChange RejectedOverride =
		BlueprintHelperReviewPanelMakeMaterialInstanceHarnessChange(
			TEXT("material_instance_panel_reject_vector"),
			TEXT("PanelVector"),
			TEXT("vector"),
			TEXT("(R=1.000000,G=1.000000,B=1.000000,A=1.000000)"),
			TEXT("(R=0.200000,G=0.450000,B=0.800000,A=1.000000)"),
			EBlueprintHelperReviewChangeStatus::Pending);

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	Changes.Add(AcceptedOverride);
	Changes.Add(RejectedOverride);

	const TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> ProjectionRegistry =
		FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault();
	TestEqual(
		TEXT("MaterialInstance scalar projects through surface projection registry"),
		ProjectionRegistry->ProjectVisibleChange(AcceptedOverride, TEXT("unknown"), TEXT("material")).Num(),
		1);
	TestEqual(
		TEXT("MaterialInstance vector projects through surface projection registry"),
		ProjectionRegistry->ProjectVisibleChange(RejectedOverride, TEXT("unknown"), TEXT("material")).Num(),
		1);

	ReviewPanel->RefreshVisibleChangesForTesting(Changes);

	TestEqual(
		TEXT("MaterialInstance ReviewPanel surface starts with two diff models"),
		ReviewPanel->GetSurfaceDiffModelCountForTesting(EBlueprintHelperReviewSurface::Material),
		2);
	const TArray<FString> InitialSurfaceIds =
		ReviewPanel->GetSurfaceDiffModelIdsForTesting(EBlueprintHelperReviewSurface::Material);
	TestTrue(
		TEXT("MaterialInstance surface includes scalar override row"),
		InitialSurfaceIds.Contains(AcceptedOverride.ChangeId));
	TestTrue(
		TEXT("MaterialInstance surface includes vector override row"),
		InitialSurfaceIds.Contains(RejectedOverride.ChangeId));
	TestTrue(
		TEXT("Embedded ReviewPanel can select MaterialInstance scalar change"),
		ReviewPanel->SelectChangeForTesting(AcceptedOverride.ChangeId));

	FString BundlePath;
	FString CaptureDebugText;
	TestTrue(
		TEXT("Embedded ReviewPanel captures MaterialInstance DebugBundle"),
		ReviewPanel->CaptureFocusDebugBundleForTesting(BundlePath, CaptureDebugText));
	TestTrue(
		TEXT("MaterialInstance DebugBundle file exists"),
		IFileManager::Get().FileExists(*BundlePath));

	Changes[0].Status = EBlueprintHelperReviewChangeStatus::Accepted;
	Changes[0].AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Accepted;
	ReviewPanel->RefreshVisibleChangesForTesting(Changes);
	TArray<FString> AfterAcceptIds =
		ReviewPanel->GetSurfaceDiffModelIdsForTesting(EBlueprintHelperReviewSurface::Material);
	TestFalse(
		TEXT("Accepted MaterialInstance override row disappears"),
		AfterAcceptIds.Contains(AcceptedOverride.ChangeId));
	TestTrue(
		TEXT("Rejected candidate MaterialInstance override row remains pending"),
		AfterAcceptIds.Contains(RejectedOverride.ChangeId));

	Changes[1].Status = EBlueprintHelperReviewChangeStatus::Rejected;
	Changes[1].AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Rejected;
	ReviewPanel->RefreshVisibleChangesForTesting(Changes);
	TestEqual(
		TEXT("MaterialInstance surface removes accepted and rejected rows"),
		ReviewPanel->GetSurfaceDiffModelCountForTesting(EBlueprintHelperReviewSurface::Material),
		0);

	FString LoadedDebugText;
	TestTrue(
		TEXT("Embedded ReviewPanel loads MaterialInstance DebugBundle"),
		ReviewPanel->LoadDebugBundleForTesting(BundlePath, LoadedDebugText));
	TestTrue(
		TEXT("MaterialInstance loaded debug text records bundle path"),
		LoadedDebugText.Contains(TEXT("DebugBundle loaded from")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelMainWindowE2EHarnessTest,
	"BlueprintHelper.Review.Panel.MainWindowE2EHarness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelMainWindowE2EHarnessTest::RunTest(const FString& Parameters)
{
	TSharedRef<SBlueprintHelperMainWindow> MainWindow = SNew(SBlueprintHelperMainWindow);
	MainWindow->ShowReviewPageForTesting();

	TSharedPtr<SBlueprintHelperReviewPanel> ReviewPanel = MainWindow->GetReviewPanelForTesting();
	TestTrue(TEXT("MainWindow constructs embedded ReviewPanel on Review page"), ReviewPanel.IsValid());
	if (!ReviewPanel.IsValid())
	{
		return false;
	}

	const TArray<FBlueprintHelperReviewPanelSurfaceHarnessCase> TestCases =
	{
		{ TEXT("MainGraph"), EBlueprintHelperReviewSurface::Graph, TEXT("graph_node"), TEXT("graph_node") },
		{ TEXT("MainComponents"), EBlueprintHelperReviewSurface::Components, TEXT("component"), TEXT("component") },
		{ TEXT("MainMyBlueprint"), EBlueprintHelperReviewSurface::MyBlueprint, TEXT("blueprint_variable"), TEXT("blueprint_variable") },
		{ TEXT("MainDetails"), EBlueprintHelperReviewSurface::Details, TEXT("component"), TEXT("component") },
		{ TEXT("MainWidgetTree"), EBlueprintHelperReviewSurface::UMGWidgetTree, TEXT("umg_widget"), TEXT("umg_widget") },
		{ TEXT("MainDataTable"), EBlueprintHelperReviewSurface::DataTable, TEXT("datatable_row"), TEXT("datatable_row") },
		{ TEXT("MainDataAsset"), EBlueprintHelperReviewSurface::DataAsset, TEXT("object_property"), TEXT("object_property") },
		{ TEXT("MainStructField"), EBlueprintHelperReviewSurface::DataAsset, TEXT("struct_field"), TEXT("struct_field") },
		{ TEXT("MainMaterial"), EBlueprintHelperReviewSurface::Material, TEXT("material_expression"), TEXT("material_expression") }
	};

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	for (const FBlueprintHelperReviewPanelSurfaceHarnessCase& TestCase : TestCases)
	{
		Changes.Add(BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
			TestCase,
			TEXT("First"),
			EBlueprintHelperReviewChangeStatus::Pending));
		Changes.Add(BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
			TestCase,
			TEXT("Second"),
			EBlueprintHelperReviewChangeStatus::Pending));
	}
	ReviewPanel->RefreshVisibleChangesForTesting(Changes);

	for (const FBlueprintHelperReviewPanelSurfaceHarnessCase& TestCase : TestCases)
	{
		const FString FirstId = FString::Printf(TEXT("surface_diff_%s_First"), *TestCase.Name);
		const FString SecondId = FString::Printf(TEXT("surface_diff_%s_Second"), *TestCase.Name);
		const TArray<FString> InitialSurfaceIds = ReviewPanel->GetSurfaceDiffModelIdsForTesting(TestCase.Surface);
		TestTrue(
			FString::Printf(TEXT("%s embedded surface includes first pending row"), *TestCase.Name),
			InitialSurfaceIds.Contains(FirstId));
		TestTrue(
			FString::Printf(TEXT("%s embedded surface includes second pending row"), *TestCase.Name),
			InitialSurfaceIds.Contains(SecondId));
	}

	for (int32 Index = 0; Index + 1 < Changes.Num(); Index += 2)
	{
		FBlueprintHelperReviewVisibleChange& FirstChange = Changes[Index];
		FBlueprintHelperReviewVisibleChange& SecondChange = Changes[Index + 1];
		FirstChange.Status = EBlueprintHelperReviewChangeStatus::Accepted;
		FirstChange.AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Accepted;
		ReviewPanel->RefreshVisibleChangesForTesting(Changes);

		TArray<FString> RemainingIds = ReviewPanel->GetSurfaceDiffModelIdsForTesting(
			FirstChange.AtomicTargets[0].Surface);
		TestFalse(
			FString::Printf(TEXT("%s accepted row disappears in embedded ReviewPanel"), *FirstChange.DisplayLabel),
			RemainingIds.Contains(FirstChange.ChangeId));
		TestTrue(
			FString::Printf(TEXT("%s sibling row remains in embedded ReviewPanel"), *SecondChange.DisplayLabel),
			RemainingIds.Contains(SecondChange.ChangeId));

		SecondChange.Status = EBlueprintHelperReviewChangeStatus::Rejected;
		SecondChange.AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Rejected;
		ReviewPanel->RefreshVisibleChangesForTesting(Changes);
		TestFalse(
			FString::Printf(TEXT("%s rejected row disappears in embedded ReviewPanel"), *SecondChange.DisplayLabel),
			ReviewPanel->GetSurfaceDiffModelIdsForTesting(SecondChange.AtomicTargets[0].Surface).Contains(SecondChange.ChangeId));
	}

	ReviewPanel->RefreshVisibleChangesForTesting(Changes);
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		const TArray<FString> SurfaceIds = ReviewPanel->GetSurfaceDiffModelIdsForTesting(Change.AtomicTargets[0].Surface);
		TestFalse(
			FString::Printf(TEXT("%s closed row no longer appears in embedded ReviewPanel surface diff"), *Change.DisplayLabel),
			SurfaceIds.Contains(Change.ChangeId));
	}

	FBlueprintHelperReviewVisibleChange DebugChange =
		BlueprintHelperReviewPanelMakeSurfaceHarnessChange(
			{ TEXT("MainDebug"), EBlueprintHelperReviewSurface::MyBlueprint, TEXT("blueprint_variable"), TEXT("blueprint_variable") },
			TEXT("Focus"),
			EBlueprintHelperReviewChangeStatus::Pending);
	TArray<FBlueprintHelperReviewVisibleChange> DebugChanges;
	DebugChanges.Add(DebugChange);
	ReviewPanel->RefreshVisibleChangesForTesting(DebugChanges);
	TestTrue(TEXT("Embedded ReviewPanel can select debug focus change"), ReviewPanel->SelectChangeForTesting(DebugChange.ChangeId));

	FString BundlePath;
	FString CaptureDebugText;
	TestTrue(
		TEXT("Embedded ReviewPanel can capture focus DebugBundle"),
		ReviewPanel->CaptureFocusDebugBundleForTesting(BundlePath, CaptureDebugText));
	TestTrue(
		TEXT("Embedded ReviewPanel DebugBundle file exists"),
		IFileManager::Get().FileExists(*BundlePath));

	FString LoadedDebugText;
	TestTrue(
		TEXT("Embedded ReviewPanel can load captured DebugBundle"),
		ReviewPanel->LoadDebugBundleForTesting(BundlePath, LoadedDebugText));
	TestTrue(
		TEXT("Embedded ReviewPanel loaded debug text records bundle path"),
		LoadedDebugText.Contains(TEXT("DebugBundle loaded from")));

	return true;
}

#endif
