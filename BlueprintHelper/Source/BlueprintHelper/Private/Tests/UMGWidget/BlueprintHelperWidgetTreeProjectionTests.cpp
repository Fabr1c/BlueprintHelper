#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreeProjectionService.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ExpandableArea.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetTreeProjectionTestsLocalUtils
{
public:
	struct FProjectionFixture
	{
		UPackage* Package = nullptr;
		UWidgetBlueprint* Blueprint = nullptr;
		UCanvasPanel* Root = nullptr;
		UTextBlock* FirstChild = nullptr;
		UTextBlock* SecondChild = nullptr;
		UExpandableArea* NamedSlotHost = nullptr;
		UTextBlock* NamedSlotContent = nullptr;
	};

	static FString MakeObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FProjectionFixture MakeFixture(const FString& Prefix)
	{
		FProjectionFixture Fixture;
		Fixture.Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperWidgetTreeProjection/%s"),
			*MakeObjectName(Prefix)));
		Fixture.Package->SetDirtyFlag(false);

		Fixture.Blueprint = NewObject<UWidgetBlueprint>(
			Fixture.Package,
			*MakeObjectName(TEXT("WBP_WidgetTreeProjection")),
			RF_Public | RF_Standalone | RF_Transactional);
		Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Fixture.Blueprint,
			TEXT("WidgetTree"),
			RF_Transactional);
		Fixture.Blueprint->ParentClass = UUserWidget::StaticClass();

		Fixture.Root = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("Canvas_Root"));
		Fixture.Blueprint->WidgetTree->RootWidget = Fixture.Root;

		Fixture.FirstChild = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("TitleText"));
		Fixture.Root->AddChild(Fixture.FirstChild);

		Fixture.SecondChild = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("SubtitleText"));
		Fixture.Root->AddChild(Fixture.SecondChild);

		Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
			UExpandableArea::StaticClass(),
			TEXT("DialogShell"));
		Fixture.Root->AddChild(Fixture.NamedSlotHost);

		Fixture.NamedSlotContent = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("InventoryPanel"));
		Fixture.NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), Fixture.NamedSlotContent);

		Fixture.Package->SetDirtyFlag(false);
		return Fixture;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreeProjectionRootAndIndexTest,
	"BlueprintHelper.UMGWidget.Projection.RootAndIndexFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreeProjectionRootAndIndexTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::FProjectionFixture Fixture =
		FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::MakeFixture(TEXT("RootAndIndex"));

	FBlueprintHelperWidgetTreeSummary Summary;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("projection builds widget tree summary"),
		FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			Fixture.Blueprint,
			Summary,
			ErrorCode,
			ErrorMessage));

	TestEqual(TEXT("root widget name is projected"),
		Summary.Root.WidgetName,
		FString(TEXT("Canvas_Root")));
	TestTrue(TEXT("asset class is the WidgetBlueprint asset class"),
		Summary.AssetClass.Contains(TEXT("WidgetBlueprint")));
	TestTrue(TEXT("parent class is projected"),
		!Summary.ParentClass.IsEmpty());
	TestEqual(TEXT("root virtual index is 0"),
		Summary.Root.VirtualIndex,
		0);
	TestTrue(TEXT("root class path is projected"),
		Summary.Root.WidgetClassPath.Contains(TEXT("CanvasPanel")));
	TestEqual(TEXT("root has three panel children"),
		Summary.Root.Children.Num(),
		3);

	if (Summary.Root.Children.Num() < 2)
	{
		return false;
	}

	const FBlueprintHelperWidgetTreeItem& FirstChild = Summary.Root.Children[0];
	const FBlueprintHelperWidgetTreeItem& SecondChild = Summary.Root.Children[1];
	TestEqual(TEXT("first child parent is root"),
		FirstChild.ParentName,
		FString(TEXT("Canvas_Root")));
	TestTrue(TEXT("first child slot class path is present"),
		!FirstChild.SlotClassPath.IsEmpty());
	TestEqual(TEXT("first child virtual index is 0"),
		FirstChild.VirtualIndex,
		0);
	TestEqual(TEXT("second child virtual index is 1"),
		SecondChild.VirtualIndex,
		1);

	const FBlueprintHelperWidgetTreeItem* IndexedChild = Summary.Index.Find(TEXT("SubtitleText"));
	TestNotNull(TEXT("flat index contains second child"), IndexedChild);
	if (IndexedChild)
	{
		TestEqual(TEXT("flat index preserves child virtual index"),
			IndexedChild->VirtualIndex,
			SecondChild.VirtualIndex);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreeProjectionVariableFlagUsesWidgetStateTest,
	"BlueprintHelper.UMGWidget.Projection.VariableFlagUsesWidgetState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreeProjectionVariableFlagUsesWidgetStateTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::FProjectionFixture Fixture =
		FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::MakeFixture(TEXT("VariableFlagUsesWidgetState"));
	TestNotNull(TEXT("fixture blueprint"), Fixture.Blueprint);
	TestNotNull(TEXT("first child"), Fixture.FirstChild);
	if (!Fixture.Blueprint || !Fixture.FirstChild)
	{
		return false;
	}

	FBlueprintHelperWidgetVersionCompat::SetWidgetVariableState(Fixture.Blueprint, Fixture.FirstChild, false);
	TestTrue(
		TEXT("source GUID is valid before projection"),
		FBlueprintHelperWidgetVersionCompat::HasWidgetSourceGuid(Fixture.Blueprint, Fixture.FirstChild));

	FBlueprintHelperWidgetTreeSummary Summary;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("projection builds widget tree summary"),
		FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			Fixture.Blueprint,
			Summary,
			ErrorCode,
			ErrorMessage));

	const FBlueprintHelperWidgetTreeItem* Child = Summary.Index.Find(TEXT("TitleText"));
	TestNotNull(TEXT("projection index contains target child"), Child);
	if (!Child)
	{
		return false;
	}

	TestFalse(TEXT("projection reports bIsVariable, not source GUID presence"), Child->bIsVariable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreeProjectionJsonShapeTest,
	"BlueprintHelper.UMGWidget.Projection.JsonOmitsEmptySlotFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreeProjectionJsonShapeTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::FProjectionFixture Fixture =
		FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::MakeFixture(TEXT("JsonShape"));

	FBlueprintHelperWidgetTreeSummary Summary;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("projection builds widget tree summary"),
		FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			Fixture.Blueprint,
			Summary,
			ErrorCode,
			ErrorMessage));

	const TSharedRef<FJsonObject> RootJson = Summary.Root.ToJson();
	TestTrue(TEXT("root JSON has widget_name"),
		RootJson->HasField(TEXT("widget_name")));
	TestTrue(TEXT("root JSON has widget_class_path"),
		RootJson->HasField(TEXT("widget_class_path")));
	TestTrue(TEXT("root JSON has virtual_index"),
		RootJson->HasField(TEXT("virtual_index")));
	TestFalse(TEXT("root JSON omits empty slot_class_path"),
		RootJson->HasField(TEXT("slot_class_path")));
	TestFalse(TEXT("root JSON omits empty slot_name"),
		RootJson->HasField(TEXT("slot_name")));
	TestTrue(TEXT("root JSON includes is_variable"),
		RootJson->HasField(TEXT("is_variable")));
	TestTrue(TEXT("root JSON includes is_inherited"),
		RootJson->HasField(TEXT("is_inherited")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreeProjectionSlotPropertiesTest,
	"BlueprintHelper.UMGWidget.Projection.SlotPropertiesExposeWritablePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreeProjectionSlotPropertiesTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::FProjectionFixture Fixture =
		FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::MakeFixture(TEXT("SlotProperties"));
	UCanvasPanelSlot* Slot = Fixture.FirstChild ? Cast<UCanvasPanelSlot>(Fixture.FirstChild->Slot) : nullptr;
	TestNotNull(TEXT("first child has CanvasPanelSlot"), Slot);
	if (!Slot)
	{
		return false;
	}

	FAnchorData Layout = Slot->GetLayout();
	Layout.Offsets.Left = 24.0f;
	Slot->SetLayout(Layout);

	FBlueprintHelperWidgetTreeSummary Summary;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("projection builds widget tree summary"),
		FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			Fixture.Blueprint,
			Summary,
			ErrorCode,
			ErrorMessage));

	const FBlueprintHelperWidgetTreeItem* Child = Summary.Index.Find(TEXT("TitleText"));
	TestNotNull(TEXT("projection index contains target child"), Child);
	if (!Child || !Child->SlotProperties.IsValid())
	{
		return false;
	}

	FString LeftValue;
	TestTrue(TEXT("slot properties expose canonical nested property path"),
		Child->SlotProperties->TryGetStringField(TEXT("LayoutData.Offsets.Left"), LeftValue));
	TestTrue(TEXT("slot property readback contains updated value"),
		LeftValue.Contains(TEXT("24")));

	const TSharedRef<FJsonObject> ChildJson = Child->ToJson();
	const TSharedPtr<FJsonObject>* SlotPropertiesJson = nullptr;
	TestTrue(TEXT("item JSON includes slot_properties"),
		ChildJson->TryGetObjectField(TEXT("slot_properties"), SlotPropertiesJson));
	if (!SlotPropertiesJson || !SlotPropertiesJson->IsValid())
	{
		return false;
	}

	FString JsonLeftValue;
	TestTrue(TEXT("item JSON preserves canonical nested property path"),
		(*SlotPropertiesJson)->TryGetStringField(TEXT("LayoutData.Offsets.Left"), JsonLeftValue));
	TestTrue(TEXT("item JSON preserves updated value"),
		JsonLeftValue.Contains(TEXT("24")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTreeProjectionNamedSlotFactsTest,
	"BlueprintHelper.UMGWidget.Projection.NamedSlotFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTreeProjectionNamedSlotFactsTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::FProjectionFixture Fixture =
		FBlueprintHelperWidgetTreeProjectionTestsLocalUtils::MakeFixture(TEXT("NamedSlotFacts"));

	FBlueprintHelperWidgetTreeSummary Summary;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(TEXT("projection builds widget tree summary"),
		FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			Fixture.Blueprint,
			Summary,
			ErrorCode,
			ErrorMessage));

	const FBlueprintHelperNamedSlotEntry* BodySlot = nullptr;
	for (const FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
	{
		if (Entry.HostWidgetName == TEXT("DialogShell")
			&& Entry.SlotName == TEXT("Body"))
		{
			BodySlot = &Entry;
			break;
		}
	}

	TestNotNull(TEXT("projection includes named slot body fact"), BodySlot);
	if (BodySlot)
	{
		TestEqual(TEXT("named slot content widget is projected"),
			BodySlot->ContentWidgetName,
			FString(TEXT("InventoryPanel")));
		TestEqual(TEXT("named slot virtual index is always 0"),
			BodySlot->VirtualIndex,
			0);
	}

	const FBlueprintHelperWidgetTreeItem* IndexedNamedSlotContent = Summary.Index.Find(TEXT("InventoryPanel"));
	TestNotNull(TEXT("flat index includes named slot content facts"), IndexedNamedSlotContent);
	if (IndexedNamedSlotContent)
	{
		TestEqual(TEXT("named slot content parent is host widget"),
			IndexedNamedSlotContent->ParentName,
			FString(TEXT("DialogShell")));
		TestEqual(TEXT("named slot content virtual index is 0"),
			IndexedNamedSlotContent->VirtualIndex,
			0);
	}

	return true;
}

#endif
