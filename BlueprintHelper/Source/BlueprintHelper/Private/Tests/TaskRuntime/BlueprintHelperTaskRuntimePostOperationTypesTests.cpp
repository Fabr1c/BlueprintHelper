#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationTypes.h"

#include "Dom/JsonObject.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostOperationTypes_ToJson,
	"BlueprintHelper.TaskRuntime.PostOperation.TypesToJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimePostOperationTypes_ToJson::RunTest(const FString&)
{
	FBlueprintHelperTaskRuntimePostOperationRecordEx Record;
	Record.Operation = TEXT("save_asset");
	Record.AssetPath = TEXT("/Game/Test/BP_Test");
	Record.Status = EBlueprintHelperTaskRuntimePostOperationStatus::Skipped;
	Record.Reason = TEXT("package_clean");
	Record.DurationMs = 0.25;

	const TSharedRef<FJsonObject> Json = FBlueprintHelperTaskRuntimePostOperationJson::RecordToJson(Record);
	TestEqual(TEXT("operation"), Json->GetStringField(TEXT("operation")), FString(TEXT("save_asset")));
	TestEqual(TEXT("asset_path"), Json->GetStringField(TEXT("asset_path")), FString(TEXT("/Game/Test/BP_Test")));
	TestEqual(TEXT("status"), Json->GetStringField(TEXT("status")), FString(TEXT("skipped")));
	TestEqual(TEXT("reason"), Json->GetStringField(TEXT("reason")), FString(TEXT("package_clean")));
	TestEqual(TEXT("duration"), Json->GetNumberField(TEXT("duration_ms")), 0.25);
	return true;
}

#endif
