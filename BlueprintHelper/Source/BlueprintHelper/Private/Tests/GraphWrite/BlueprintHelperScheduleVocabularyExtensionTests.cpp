#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsFunctionScheduleVocabularyOwnedByFunctionActionTest,
	"BlueprintHelper.GraphWrite.GenericOps.Schedule.FunctionVocabularyOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsFunctionScheduleVocabularyOwnedByFunctionActionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionSemanticConstraints Semantic;
	Semantic.Kind = EBlueprintHelperActionSemanticKind::Schedule;
	Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Schedule;
	Semantic.FunctionOperation = TEXT("schedule_function");
	TestTrue(
		TEXT("FunctionAction owns function-backed schedule operations"),
		FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(Semantic));

	Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	TestFalse(
		TEXT("schedule_function does not leak into create semantics"),
		FBlueprintHelperFunctionSemanticActionResolver::IsSupportedSemanticKind(Semantic));
	return true;
}

#endif
