#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskPreviewStore.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptService.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptStore.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptTypes.h"

class FBlueprintHelperExecutionReceiptTestUtils
{
public:
	static TSharedRef<FJsonObject> MakeReceipt()
	{
		TSharedRef<FJsonObject> Receipt = MakeShared<FJsonObject>();
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::SchemaField(), FBlueprintHelperExecutionReceiptFields::Schema());
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::ReceiptId(), TEXT("receipt_test"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::CliRunId(), TEXT("cli_test"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::PreviewId(), TEXT("preview_test"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskSpecHash(), TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskPlanHash(), TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::PolicyHash(), TEXT("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"));
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::Status(), TEXT("created"));
		return Receipt;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExecutionReceiptPreviewStoreTest,
	"BlueprintHelper.TaskRuntime.ExecutionReceipt.PreviewStorePersistsReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperExecutionReceiptPreviewStoreTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPreviewStore Store;
	FBlueprintHelperTaskPreviewStoreCreateRequest Request;
	Request.TaskSpecHash = TEXT("spec_hash");
	Request.TaskPlanHash = TEXT("plan_hash");
	Request.ExecutionPolicyHash = TEXT("policy_hash");
	Request.ExecutionReceiptJson = FBlueprintHelperExecutionReceiptTestUtils::MakeReceipt();
	Request.bPassed = true;

	const FString Token = Store.Store(Request);
	const FBlueprintHelperTaskPreviewStoreResolveResult Resolved = Store.Resolve(Token, TEXT("spec_hash"));

	TestTrue(TEXT("preview token resolves"), Resolved.bOk);
	TestTrue(TEXT("receipt json is stored"), Resolved.ExecutionReceiptJson.IsValid());
	FString ReceiptId;
	TestTrue(TEXT("receipt_id exists"), Resolved.ExecutionReceiptJson->TryGetStringField(
		FBlueprintHelperExecutionReceiptFields::ReceiptId(),
		ReceiptId));
	TestEqual(TEXT("receipt_id preserved"), ReceiptId, FString(TEXT("receipt_test")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExecutionReceiptServiceBuildsPreviewReceiptTest,
	"BlueprintHelper.TaskRuntime.ExecutionReceipt.ServiceBuildsPreviewReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperExecutionReceiptServiceBuildsPreviewReceiptTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> TokenRequest = MakeShared<FJsonObject>();
	TokenRequest->SetStringField(FBlueprintHelperExecutionReceiptFields::ReceiptId(), TEXT("receipt_preview"));
	TokenRequest->SetStringField(FBlueprintHelperExecutionReceiptFields::CliRunId(), TEXT("cli_preview"));
	TokenRequest->SetStringField(FBlueprintHelperExecutionReceiptFields::PreviewId(), TEXT("preview_preview"));
	TokenRequest->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskSpecHash(), TEXT("spec_hash"));
	TokenRequest->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskPlanHash(), TEXT("plan_hash"));
	TokenRequest->SetStringField(TEXT("execution_policy_hash"), TEXT("policy_hash"));
	TokenRequest->SetObjectField(FBlueprintHelperExecutionReceiptFields::Receipt(), FBlueprintHelperExecutionReceiptTestUtils::MakeReceipt());

	TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TargetAssets;
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/BP_Test")));
	TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

	const TSharedPtr<FJsonObject> Receipt = FBlueprintHelperExecutionReceiptService::BuildPreviewReceipt(
		TokenRequest,
		TaskPlan,
		true);

	TestTrue(TEXT("preview receipt exists"), Receipt.IsValid());
	TestEqual(TEXT("preview status"), Receipt->GetStringField(FBlueprintHelperExecutionReceiptFields::Status()), FString(TEXT("previewed")));
	TestEqual(TEXT("receipt id from request"), Receipt->GetStringField(FBlueprintHelperExecutionReceiptFields::ReceiptId()), FString(TEXT("receipt_test")));
	const TArray<TSharedPtr<FJsonValue>>* ReceiptTargets = nullptr;
	TestTrue(TEXT("target assets copied"), Receipt->TryGetArrayField(
		FBlueprintHelperExecutionReceiptFields::TargetAssets(),
		ReceiptTargets) && ReceiptTargets && ReceiptTargets->Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperExecutionReceiptStoreFindsInMemoryJournalTest,
	"BlueprintHelper.TaskRuntime.ExecutionReceipt.StoreFindsInMemoryJournal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperExecutionReceiptStoreFindsInMemoryJournalTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
	Journal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
	Journal->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskRunId(), TEXT("task_receipt_test"));
	FBlueprintHelperExecutionReceiptService::AttachReceiptToJournal(Journal, FBlueprintHelperExecutionReceiptTestUtils::MakeReceipt());

	TMap<FString, TSharedPtr<FJsonObject>> Journals;
	Journals.Add(TEXT("task_receipt_test"), Journal);

	FBlueprintHelperExecutionReceiptStore Store;
	TSharedPtr<FJsonObject> Receipt;
	FString TaskRunId;
	FString Error;
	TestTrue(TEXT("receipt found"), Store.FindReceiptById(TEXT("receipt_test"), Journals, Receipt, TaskRunId, Error));
	TestEqual(TEXT("task run id found"), TaskRunId, FString(TEXT("task_receipt_test")));
	TestTrue(TEXT("receipt output valid"), Receipt.IsValid());
	return true;
}

#endif
