#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Debug/BlueprintHelperEditorCommandService.h"
#include "Systems/SourceControl/BlueprintHelperSourceControlService.h"

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEditorCommandServiceDiscardDirtyPackagesTest,
	"BlueprintHelper.EditorCommand.CloseEditor.DiscardDirtyPackagesClearsDirtyFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEditorCommandServiceDiscardDirtyPackagesTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Game/BlueprintHelperAutomation/CloseEditorDiscardDirtyPackageTest"));
	TestNotNull(TEXT("test package is created"), Package);
	if (!Package)
	{
		return false;
	}

	Package->SetDirtyFlag(true);
	TestTrue(TEXT("test package starts dirty"), Package->IsDirty());

	TArray<UPackage*> Packages;
	Packages.Add(Package);
	const int32 DiscardedPackageCount = FBlueprintHelperEditorCommandService::DiscardDirtyPackages(Packages);

	TestEqual(TEXT("one dirty package is discarded"), DiscardedPackageCount, 1);
	TestFalse(TEXT("discarded package no longer triggers save prompts"), Package->IsDirty());

	Package->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSourceControlCheckedOutOtherHintTest,
	"BlueprintHelper.SourceControl.Classify.CheckedOutOtherReturnsAgentHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSourceControlCheckedOutOtherHintTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSourceControlFileState State;
	State.bValid = true;
	State.bSourceControlled = true;
	State.bCheckedOutOther = true;
	State.CheckedOutOther = TEXT("p4-user");

	FBlueprintHelperSourceControlService::ClassifyFileStateForAgent(State);

	TestEqual(TEXT("checked out by other status is surfaced"), State.Status, FString(TEXT("checked_out_by_other")));
	TestTrue(TEXT("agent hint includes owner"), State.AgentHint.Contains(TEXT("p4-user")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSourceControlCheckoutRequiredHintTest,
	"BlueprintHelper.SourceControl.Classify.CheckoutRequiredReturnsToolHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSourceControlCheckoutRequiredHintTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSourceControlFileState State;
	State.bValid = true;
	State.bSourceControlled = true;
	State.bCanCheckOut = true;

	FBlueprintHelperSourceControlService::ClassifyFileStateForAgent(State);

	TestEqual(TEXT("checkout required status is surfaced"), State.Status, FString(TEXT("checkout_required")));
	TestTrue(TEXT("agent hint names checkout tool"), State.AgentHint.Contains(TEXT("blueprinthelper_source_control_checkout")));
	return true;
}

#endif
