#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace BlueprintHelperGraphWrite80PercentPlanContractTests
{
static FString ProjectRoot()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
}

static bool LoadProjectFile(const FString& RelativePath, FString& OutText)
{
	return FFileHelper::LoadFileToString(OutText, *(ProjectRoot() / RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWrite80PercentPlanDocsContractTest,
	"BlueprintHelper.GraphWrite.Capability80.P0.PlanDocsContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWrite80PercentPlanDocsContractTest::RunTest(const FString& Parameters)
{
	FString Roadmap;
	TestTrue(TEXT("roadmap loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_Roadmap_20260522_CN.md"),
			Roadmap));
	TestTrue(TEXT("roadmap defines legacy strict mode"), Roadmap.Contains(TEXT("Legacy 严格模式")));
	TestTrue(TEXT("roadmap defines direct spawn boundary"), Roadmap.Contains(TEXT("Direct spawn 边界")));
	TestTrue(TEXT("roadmap defines NeedsMoreSemanticContext distinction"), Roadmap.Contains(TEXT("必要上下文缺失不等于候选超过阈值")));

	FString TestRecord;
	TestTrue(TEXT("test record loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md"),
			TestRecord));
	TestTrue(TEXT("test record includes physical door"), TestRecord.Contains(TEXT("PhysicalDoor_InteractableOnly")));
	TestTrue(TEXT("test record includes synthetic function field control"), TestRecord.Contains(TEXT("TimedAccessGate_StateMachine")));
	TestTrue(TEXT("test record includes synthetic struct event delegate"), TestRecord.Contains(TEXT("EventDrivenConfigApplier")));
	TestTrue(TEXT("test record initializes as not run"), TestRecord.Contains(TEXT("not_run")));

	FString GapDoc;
	TestTrue(TEXT("gap doc loads"),
		BlueprintHelperGraphWrite80PercentPlanContractTests::LoadProjectFile(
			TEXT("Plugins/BlueprintHelper/BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md"),
			GapDoc));
	TestTrue(TEXT("gap doc records legacy deletion gate"), GapDoc.Contains(TEXT("Legacy 删除门禁")));
	TestTrue(TEXT("gap doc requires delete or gap reason"), GapDoc.Contains(TEXT("默认直接删除")));

	return true;
}

#endif
