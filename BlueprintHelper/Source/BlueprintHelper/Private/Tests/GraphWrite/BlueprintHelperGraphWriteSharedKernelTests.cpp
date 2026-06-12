#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperSharedGraphMutationKernel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSharedGraphMutationKernelRunExistingOperationTest,
	"BlueprintHelper.GraphWrite.SharedKernel.RunExistingOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSharedGraphMutationKernelRunExistingOperationTest::RunTest(const FString& Parameters)
{
	bool bCalled = false;
	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperSharedGraphMutationKernel::RunExistingOperation(
			EBlueprintHelperGraphWriteUnitOfWorkMode::Preview,
			TEXT("k2.owned_graph.patch.connect_pins"),
			TEXT("patch_owned_graph"),
			EBlueprintHelperGraphBodyKind::Unknown,
			[&bCalled]()
			{
				bCalled = true;
				FBlueprintHelperToolResultBase Inner;
				Inner.bOk = true;
				Inner.Status = EBlueprintHelperToolStatus::DryRun;
				Inner.Operation = TEXT("patch_blueprint_graph");
				Inner.TraceId = TEXT("shared_kernel_test");
				return Inner;
			});

	TestTrue(TEXT("operation called"), bCalled);
	TestTrue(TEXT("result ok"), Result.bOk);
	TestEqual(TEXT("preview status"), ToolStatusToString(Result.Status), FString(TEXT("dry_run")));
	return true;
}
