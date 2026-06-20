// BlueprintHelper execution receipt protocol constants.

#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptTypes.h"

const TCHAR* FBlueprintHelperExecutionReceiptFields::Schema()
{
	return TEXT("BlueprintHelper.ExecutionReceipt.v1");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::SchemaField()
{
	return TEXT("schema");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::ReceiptId()
{
	return TEXT("receipt_id");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::RequestId()
{
	return TEXT("request_id");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::CliRunId()
{
	return TEXT("cli_run_id");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::PreviewId()
{
	return TEXT("preview_id");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::TaskRunId()
{
	return TEXT("task_run_id");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::TaskSpecHash()
{
	return TEXT("task_spec_hash");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::TaskPlanHash()
{
	return TEXT("task_plan_hash");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::PolicyHash()
{
	return TEXT("policy_hash");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::Status()
{
	return TEXT("status");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::Receipt()
{
	return TEXT("receipt");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::JournalRef()
{
	return TEXT("journal_ref");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::ReadbackRef()
{
	return TEXT("readback_ref");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::CreatedAt()
{
	return TEXT("created_at");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::UpdatedAt()
{
	return TEXT("updated_at");
}

const TCHAR* FBlueprintHelperExecutionReceiptFields::TargetAssets()
{
	return TEXT("target_assets");
}
