#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ExpandableArea.h"
#include "Components/NamedSlotInterface.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils
{
public:
	struct FWidgetTreeFixture
	{
		UPackage* Package = nullptr;
		UWidgetBlueprint* Blueprint = nullptr;
		UCanvasPanel* Root = nullptr;
		UTextBlock* FirstText = nullptr;
		UTextBlock* SecondText = nullptr;
		UExpandableArea* NamedSlotHost = nullptr;
		UTextBlock* SlotContent = nullptr;
	};

	static FString MakeUniqueName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FWidgetTreeFixture MakeFixture(const FString& Prefix)
	{
		FWidgetTreeFixture Fixture;
		Fixture.Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReviewWidgetTree/%s"),
			*MakeUniqueName(Prefix)));
		Fixture.Blueprint = NewObject<UWidgetBlueprint>(
			Fixture.Package,
			*MakeUniqueName(TEXT("WBP_WidgetTreeSnapshot")),
			RF_Public | RF_Standalone | RF_Transactional);
		Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Fixture.Blueprint,
			TEXT("WidgetTree"),
			RF_Transactional);

		Fixture.Root = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("Canvas_Root"));
		Fixture.Blueprint->WidgetTree->RootWidget = Fixture.Root;
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.Root);

		Fixture.FirstText = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("FirstText"));
		Fixture.Root->AddChild(Fixture.FirstText);
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.FirstText);

		Fixture.SecondText = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("SecondText"));
		Fixture.Root->AddChild(Fixture.SecondText);
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.SecondText);

		Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
			UExpandableArea::StaticClass(),
			TEXT("DialogShell"));
		Fixture.Root->AddChild(Fixture.NamedSlotHost);
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

		Fixture.SlotContent = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("OldBody"));
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.SlotContent);
		if (INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Fixture.NamedSlotHost))
		{
			NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), Fixture.SlotContent);
		}
		Fixture.Package->SetDirtyFlag(false);
		return Fixture;
	}

	static bool IsWidgetVariableRegistered(UWidgetBlueprint* Blueprint, UWidget* Widget)
	{
#if WITH_EDITORONLY_DATA
		if (!Blueprint || !Widget)
		{
			return false;
		}
#if BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS
		return Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName());
#else
		return Widget->bIsVariable;
#endif
#else
		return false;
#endif
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewWidgetTreeSnapshotVirtualIndexTest,
	"BlueprintHelper.Review.WidgetTree.SnapshotSerializesVirtualIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewWidgetTreeSnapshotVirtualIndexTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::FWidgetTreeFixture Fixture =
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::MakeFixture(TEXT("VirtualIndex"));
	TestNotNull(TEXT("fixture blueprint"), Fixture.Blueprint);
	if (!Fixture.Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FBlueprintHelperReviewAtomicTarget RootTarget;
	RootTarget.AssetPath = Fixture.Blueprint->GetPathName();
	RootTarget.TargetKind = TEXT("umg_widget");
	RootTarget.TargetKey = TEXT("umg_widget:Canvas_Root");
	RootTarget.VisualGroupKey = RootTarget.TargetKey;
	FString RootSnapshotJson;
	FString RootHash;
	FString RootError;
	TestTrue(
		TEXT("root target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(RootTarget, RootSnapshotJson, RootHash, RootError));

	FBlueprintHelperReviewAtomicTarget SecondTarget;
	SecondTarget.AssetPath = Fixture.Blueprint->GetPathName();
	SecondTarget.TargetKind = TEXT("umg_widget");
	SecondTarget.TargetKey = TEXT("umg_widget:SecondText");
	SecondTarget.VisualGroupKey = SecondTarget.TargetKey;
	FString SecondSnapshotJson;
	FString SecondHash;
	FString SecondError;
	TestTrue(
		TEXT("child target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(SecondTarget, SecondSnapshotJson, SecondHash, SecondError));

	TSharedPtr<FJsonObject> RootSnapshot;
	TSharedPtr<FJsonObject> SecondSnapshot;
	TestTrue(
		TEXT("root snapshot parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RootSnapshotJson), RootSnapshot) && RootSnapshot.IsValid());
	TestTrue(
		TEXT("child snapshot parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SecondSnapshotJson), SecondSnapshot) && SecondSnapshot.IsValid());
	if (!RootSnapshot.IsValid() || !SecondSnapshot.IsValid())
	{
		return false;
	}

	double VirtualIndex = -1.0;
	const bool bSecondHasVirtualIndex =
		SecondSnapshot->TryGetNumberField(TEXT("virtual_index"), VirtualIndex) &&
		FMath::RoundToInt(VirtualIndex) == 1;
	TestFalse(TEXT("root omits empty slot_class"), RootSnapshot->HasField(TEXT("slot_class")));
	TestFalse(TEXT("root omits empty slot_class_path"), RootSnapshot->HasField(TEXT("slot_class_path")));
	TestTrue(TEXT("second child has virtual_index 1"), bSecondHasVirtualIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewWidgetTreeRestoreNamedSlotTest,
	"BlueprintHelper.Review.WidgetTree.RestoreNamedSlotContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewWidgetTreeRestoreNamedSlotTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::FWidgetTreeFixture Fixture =
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::MakeFixture(TEXT("NamedSlotRestore"));
	TestNotNull(TEXT("fixture blueprint"), Fixture.Blueprint);
	if (!Fixture.Blueprint || !Fixture.NamedSlotHost)
	{
		return false;
	}

	INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Fixture.NamedSlotHost);
	TestNotNull(TEXT("named slot host"), NamedSlotHost);
	if (!NamedSlotHost)
	{
		return false;
	}

	UTextBlock* NewBody = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("NewBody"));
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, NewBody);
	NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), NewBody);

	TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetStringField(TEXT("surface"), TEXT("umg_widget_tree"));
	Snapshot->SetStringField(TEXT("target_subkind"), TEXT("named_slot_content"));
	Snapshot->SetBoolField(TEXT("exists"), true);
	Snapshot->SetBoolField(TEXT("content_exists"), true);
	Snapshot->SetStringField(TEXT("host_widget_name"), TEXT("DialogShell"));
	Snapshot->SetStringField(TEXT("slot_name"), TEXT("Body"));
	Snapshot->SetStringField(TEXT("content_widget_name"), TEXT("OldBody"));
	Snapshot->SetStringField(TEXT("content_widget_class"), UTextBlock::StaticClass()->GetPathName());
	Snapshot->SetNumberField(TEXT("virtual_index"), 0);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Fixture.Blueprint->GetPathName();
	Target.TargetKind = TEXT("umg_widget_tree");
	Target.TargetSubKind = TEXT("named_slot_content");
	Target.TargetKey = TEXT("umg_widget_tree:DialogShell:slot:Body");
	Target.VisualGroupKey = Target.TargetKey;

	FString Error;
	const bool bRestored = FBlueprintHelperReviewSnapshotRestoreService::RestoreWidgetFromSnapshot(
		Target,
		Snapshot,
		Error);
	TestTrue(FString::Printf(TEXT("restore succeeds: %s"), *Error), bRestored);
	UWidget* RestoredContent = NamedSlotHost->GetContentForSlot(FName(TEXT("Body")));
	TestNotNull(TEXT("slot has restored content"), RestoredContent);
	if (RestoredContent)
	{
		TestEqual(TEXT("old content restored"), RestoredContent->GetName(), FString(TEXT("OldBody")));
	}
	TestFalse(
		TEXT("replaced content variable is unregistered"),
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, NewBody));
	TestTrue(
		TEXT("restored content variable is registered"),
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, RestoredContent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewWidgetTreeRestoreSlotPropertyTest,
	"BlueprintHelper.Review.WidgetTree.RestoreSlotProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewWidgetTreeRestoreSlotPropertyTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::FWidgetTreeFixture Fixture =
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::MakeFixture(TEXT("SlotPropertyRestore"));
	TestNotNull(TEXT("fixture blueprint"), Fixture.Blueprint);
	TestNotNull(TEXT("first text"), Fixture.FirstText);
	if (!Fixture.Blueprint || !Fixture.FirstText)
	{
		return false;
	}

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Fixture.FirstText->Slot);
	TestNotNull(TEXT("first text has canvas panel slot"), Slot);
	if (!Slot)
	{
		return false;
	}

	FAnchorData InitialLayout = Slot->GetLayout();
	InitialLayout.Offsets.Left = 12.0f;
	Slot->SetLayout(InitialLayout);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Fixture.Blueprint->GetPathName();
	Target.TargetKind = TEXT("slot_property");
	Target.TargetSubKind = TEXT("slot_property");
	Target.TargetKey = TEXT("slot_property:FirstText.LayoutData.Offsets.Left");
	Target.VisualGroupKey = Target.TargetKey;
	Target.PropertyPath = TEXT("LayoutData.Offsets.Left");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(
		TEXT("slot property snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));

	TSharedPtr<FJsonObject> Snapshot;
	TestTrue(
		TEXT("slot property snapshot parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SnapshotJson), Snapshot) && Snapshot.IsValid());
	if (!Snapshot.IsValid())
	{
		return false;
	}

	FString ValueText;
	TestTrue(TEXT("snapshot carries slot property value"), Snapshot->TryGetStringField(TEXT("value"), ValueText));
	TestEqual(TEXT("snapshot carries before value"), ValueText, FString(TEXT("12.000000")));

	FAnchorData ChangedLayout = Slot->GetLayout();
	ChangedLayout.Offsets.Left = 48.0f;
	Slot->SetLayout(ChangedLayout);
	TestEqual(TEXT("slot changed before restore"), Slot->GetLayout().Offsets.Left, 48.0f);

	FString RestoreError;
	const bool bRestored = FBlueprintHelperReviewSnapshotRestoreService::RestoreWidgetFromSnapshot(
		Target,
		Snapshot,
		RestoreError);
	TestTrue(FString::Printf(TEXT("slot property restore succeeds: %s"), *RestoreError), bRestored);
	TestEqual(TEXT("slot property restored to before value"), Slot->GetLayout().Offsets.Left, 12.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewWidgetTreeRestoreWidgetVariableTest,
	"BlueprintHelper.Review.WidgetTree.RestoreWidgetVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewWidgetTreeRestoreWidgetVariableTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::FWidgetTreeFixture Fixture =
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::MakeFixture(TEXT("WidgetVariableRestore"));
	TestNotNull(TEXT("fixture blueprint"), Fixture.Blueprint);
	TestNotNull(TEXT("first text"), Fixture.FirstText);
	if (!Fixture.Blueprint || !Fixture.FirstText)
	{
		return false;
	}

	FBlueprintHelperWidgetVersionCompat::SetWidgetVariableState(Fixture.Blueprint, Fixture.FirstText, false);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Fixture.Blueprint->GetPathName();
	Target.TargetKind = TEXT("widget_variable");
	Target.TargetSubKind = TEXT("widget_variable");
	Target.TargetKey = TEXT("widget_variable:FirstText");
	Target.VisualGroupKey = Target.TargetKey;
	Target.PropertyPath = TEXT("is_variable");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(
		TEXT("widget variable snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));

	TSharedPtr<FJsonObject> Snapshot;
	TestTrue(
		TEXT("widget variable snapshot parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SnapshotJson), Snapshot) && Snapshot.IsValid());
	if (!Snapshot.IsValid())
	{
		return false;
	}

	bool bSnapshotIsVariable = true;
	TestTrue(TEXT("snapshot carries is_variable"), Snapshot->TryGetBoolField(TEXT("is_variable"), bSnapshotIsVariable));
	TestFalse(TEXT("snapshot records before variable state"), bSnapshotIsVariable);

	Fixture.FirstText->bIsVariable = true;
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.FirstText);
	TestTrue(
		TEXT("widget variable registered before restore"),
		FBlueprintHelperReviewWidgetTreeSnapshotTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, Fixture.FirstText));

	FString RestoreError;
	const bool bRestored = FBlueprintHelperReviewSnapshotRestoreService::RestoreWidgetFromSnapshot(
		Target,
		Snapshot,
		RestoreError);
	TestTrue(FString::Printf(TEXT("widget variable restore succeeds: %s"), *RestoreError), bRestored);
	TestFalse(TEXT("widget bIsVariable restored false"), Fixture.FirstText->bIsVariable);
	TestTrue(
		TEXT("widget source GUID remains valid after variable restore"),
		FBlueprintHelperWidgetVersionCompat::HasWidgetSourceGuid(Fixture.Blueprint, Fixture.FirstText));
	return true;
}

#endif
