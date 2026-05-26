#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsRuntimeClusterStabilityTest,
	"BlueprintHelper.GraphWrite.GenericOps.Contract.RuntimeClusterStability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsRuntimeClusterStabilityTest::RunTest(const FString& Parameters)
{
	const TArray<FString> RuntimeClusters = {
		FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind::FunctionAction),
		FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind::FieldVariableAction),
		FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind::EventDelegateAction),
		FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction)
	};

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("FunctionAction runtime cluster remains canonical"), RuntimeClusters.Contains(TEXT("FunctionActionCluster")));
	bPassed &= TestTrue(TEXT("Field capability remains a first-class existing runtime cluster"), RuntimeClusters.Contains(TEXT("FieldVariableActionCluster")));
	bPassed &= TestTrue(TEXT("GenericAssetStructControl runtime owner remains canonical"), RuntimeClusters.Contains(TEXT("GenericAssetStructControlActionCluster")));
	for (const FString& Forbidden : {
		FString(TEXT("control")),
		FString(TEXT("generic_transform")),
		FString(TEXT("generic_create")),
		FString(TEXT("struct_select")),
		FString(TEXT("generic_op"))
	})
	{
		bPassed &= TestFalse(
			FString::Printf(TEXT("GenericOps logical group is not a runtime cluster: %s"), *Forbidden),
			RuntimeClusters.Contains(Forbidden));
	}
	return bPassed;
}

#endif
