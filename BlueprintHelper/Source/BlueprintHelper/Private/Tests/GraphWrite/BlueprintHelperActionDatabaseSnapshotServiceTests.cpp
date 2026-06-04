#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperActionDatabaseSnapshotService.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionDatabaseSnapshotService_EntriesArePureData,
	"BlueprintHelper.GraphWrite.AutoSearch.ActionDatabaseSnapshot.EntriesArePureData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperActionDatabaseSnapshotService_EntriesArePureData::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionDatabaseSnapshotService Service;
	TestTrue(TEXT("new snapshot service starts dirty"), Service.IsDirty());

	const FBlueprintHelperActionDatabaseSnapshot Snapshot = Service.BuildSnapshotOnGameThread();
	TestTrue(TEXT("snapshot generation is present"), !Snapshot.Generation.IsEmpty());
	TestEqual(TEXT("entry count mirrors entries"), Snapshot.EntryCount, Snapshot.Entries.Num());
	TestFalse(TEXT("snapshot service clean after build"), Service.IsDirty());

	Service.MarkDirty();
	TestTrue(TEXT("mark dirty flips state"), Service.IsDirty());

	for (const FBlueprintHelperActionDatabaseSnapshotEntry& Entry : Snapshot.Entries)
	{
		TestFalse(TEXT("stable id does not contain raw pointer marker"), Entry.StableId.Contains(TEXT("0x")));
		TestTrue(TEXT("entry has display or menu name"), !Entry.DisplayName.IsEmpty() || !Entry.MenuName.IsEmpty());
		TestFalse(TEXT("spawner signature hash hides raw pointer marker"), Entry.SpawnerSignatureHash.Contains(TEXT("0x")));
	}
	return true;
}

#endif
