#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreePositionPolicy.h"

#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"

class FBlueprintHelperWidgetTreePositionPolicyTestsLocalUtils
{
public:
	static UTextBlock* AddTextChild(UCanvasPanel* ParentPanel, const TCHAR* WidgetName)
	{
		if (!ParentPanel)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = NewObject<UTextBlock>(ParentPanel, WidgetName);
		ParentPanel->AddChild(TextBlock);
		return TextBlock;
	}

	static FBlueprintHelperWidgetTreeSummary MakeSummaryForExpectedPosition()
	{
		FBlueprintHelperWidgetTreeSummary Summary;

		FBlueprintHelperWidgetTreeItem Root;
		Root.WidgetName = TEXT("Canvas_Root");
		Root.WidgetClass = TEXT("CanvasPanel");
		Root.WidgetClassPath = TEXT("/Script/UMG.CanvasPanel");
		Root.VirtualIndex = 0;

		FBlueprintHelperWidgetTreeItem Child;
		Child.WidgetName = TEXT("Button_Start");
		Child.WidgetClass = TEXT("Button");
		Child.WidgetClassPath = TEXT("/Script/UMG.Button");
		Child.ParentName = Root.WidgetName;
		Child.VirtualIndex = 1;

		Root.Children.Add(Child);
		Summary.Root = Root;
		Summary.Index.Add(Root.WidgetName, Root);
		Summary.Index.Add(Child.WidgetName, Child);

		FBlueprintHelperNamedSlotEntry NamedSlot;
		NamedSlot.HostWidgetName = TEXT("DialogShell");
		NamedSlot.SlotName = TEXT("Body");
		NamedSlot.ContentWidgetName = TEXT("InventoryPanel");
		NamedSlot.VirtualIndex = 0;
		Summary.NamedSlots.Add(NamedSlot);

		return Summary;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreePositionPolicyPanelIndexTest,
	"BlueprintHelper.UMGWidget.PositionPolicy.PanelVirtualIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreePositionPolicyPanelIndexTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Panel = NewObject<UCanvasPanel>(GetTransientPackage(), TEXT("PolicyRootPanel"));
	UTextBlock* FirstChild = FBlueprintHelperWidgetTreePositionPolicyTestsLocalUtils::AddTextChild(Panel, TEXT("FirstChild"));
	UTextBlock* SecondChild = FBlueprintHelperWidgetTreePositionPolicyTestsLocalUtils::AddTextChild(Panel, TEXT("SecondChild"));

	TestEqual(TEXT("first child virtual index is 0"),
		FBlueprintHelperWidgetTreePositionPolicy::BuildPanelVirtualIndex(Panel, FirstChild),
		0);
	TestEqual(TEXT("second child virtual index is 1"),
		FBlueprintHelperWidgetTreePositionPolicy::BuildPanelVirtualIndex(Panel, SecondChild),
		1);

	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("panel insert accepts end virtual index"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateVirtualIndexForPanel(Panel, 2, true, ErrorCode, ErrorMessage));
	TestFalse(TEXT("panel rejects negative virtual index"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateVirtualIndexForPanel(Panel, -1, true, ErrorCode, ErrorMessage));
	TestEqual(TEXT("negative panel index reports invalid_virtual_index"),
		ErrorCode,
		FString(TEXT("invalid_virtual_index")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreePositionPolicyContentSlotIndexTest,
	"BlueprintHelper.UMGWidget.PositionPolicy.ContentSlotVirtualIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreePositionPolicyContentSlotIndexTest::RunTest(const FString& Parameters)
{
	FString ErrorCode;
	FString ErrorMessage;

	TestTrue(TEXT("single content slot accepts virtual index 0"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateSingleContentVirtualIndex(0, ErrorCode, ErrorMessage));
	TestFalse(TEXT("single content slot rejects virtual index 1"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateSingleContentVirtualIndex(1, ErrorCode, ErrorMessage));
	TestEqual(TEXT("content slot overflow reports invalid_virtual_index"),
		ErrorCode,
		FString(TEXT("invalid_virtual_index")));

	TestTrue(TEXT("named slot accepts virtual index 0"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateNamedSlotVirtualIndex(0, ErrorCode, ErrorMessage));
	TestFalse(TEXT("named slot rejects negative virtual index"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateNamedSlotVirtualIndex(-1, ErrorCode, ErrorMessage));
	TestEqual(TEXT("named slot negative reports invalid_virtual_index"),
		ErrorCode,
		FString(TEXT("invalid_virtual_index")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreePositionPolicyExpectedFactsTest,
	"BlueprintHelper.UMGWidget.PositionPolicy.ExpectedProjectedFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreePositionPolicyExpectedFactsTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeSummary Summary =
		FBlueprintHelperWidgetTreePositionPolicyTestsLocalUtils::MakeSummaryForExpectedPosition();

	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("expected parent and virtual index match panel child facts"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateExpectedPosition(
			Summary,
			TEXT("Button_Start"),
			TEXT("Canvas_Root"),
			TOptional<int32>(1),
			ErrorCode,
			ErrorMessage));

	TestFalse(TEXT("expected parent mismatch is rejected"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateExpectedPosition(
			Summary,
			TEXT("Button_Start"),
			TEXT("OtherPanel"),
			TOptional<int32>(1),
			ErrorCode,
			ErrorMessage));
	TestEqual(TEXT("expected parent mismatch reports stable code"),
		ErrorCode,
		FString(TEXT("expected_parent_mismatch")));

	TestTrue(TEXT("expected position supports named slot content facts"),
		FBlueprintHelperWidgetTreePositionPolicy::ValidateExpectedPosition(
			Summary,
			TEXT("InventoryPanel"),
			TEXT("DialogShell"),
			TOptional<int32>(0),
			ErrorCode,
			ErrorMessage));

	return true;
}

#endif
