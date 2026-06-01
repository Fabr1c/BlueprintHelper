#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Debug/BlueprintHelperEditorCommandService.h"

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

#endif
