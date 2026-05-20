#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

struct FBlueprintHelperTaskRuntimePostOperationKindName
{
	EBlueprintHelperTaskRuntimePostOperationKind Kind;
	const TCHAR* Name;
};

struct FBlueprintHelperTaskRuntimePostOperationStatusName
{
	EBlueprintHelperTaskRuntimePostOperationStatus Status;
	const TCHAR* Name;
};

class FBlueprintHelperTaskRuntimePostOperationTypeLocalUtils
{
public:
	static const TCHAR* FindKindName(EBlueprintHelperTaskRuntimePostOperationKind Kind)
	{
		static const FBlueprintHelperTaskRuntimePostOperationKindName Names[] = {
			{EBlueprintHelperTaskRuntimePostOperationKind::Compile, TEXT("compile")},
			{EBlueprintHelperTaskRuntimePostOperationKind::Save, TEXT("save")}
		};

		for (const FBlueprintHelperTaskRuntimePostOperationKindName& Entry : Names)
		{
			if (Entry.Kind == Kind)
			{
				return Entry.Name;
			}
		}

		return TEXT("unknown");
	}

	static const TCHAR* FindStatusName(EBlueprintHelperTaskRuntimePostOperationStatus Status)
	{
		static const FBlueprintHelperTaskRuntimePostOperationStatusName Names[] = {
			{EBlueprintHelperTaskRuntimePostOperationStatus::Planned, TEXT("planned")},
			{EBlueprintHelperTaskRuntimePostOperationStatus::Executed, TEXT("executed")},
			{EBlueprintHelperTaskRuntimePostOperationStatus::Skipped, TEXT("skipped")},
			{EBlueprintHelperTaskRuntimePostOperationStatus::Failed, TEXT("failed")}
		};

		for (const FBlueprintHelperTaskRuntimePostOperationStatusName& Entry : Names)
		{
			if (Entry.Status == Status)
			{
				return Entry.Name;
			}
		}

		return TEXT("unknown");
	}

	static TSharedRef<FJsonObject> PlanItemToJson(const FBlueprintHelperTaskRuntimePostOperationPlanItem& Item)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("kind"), FindKindName(Item.Kind));
		Json->SetStringField(TEXT("operation"), Item.Operation);
		Json->SetStringField(TEXT("asset_path"), Item.AssetPath);
		if (!Item.Reason.IsEmpty())
		{
			Json->SetStringField(TEXT("reason"), Item.Reason);
		}
		return Json;
	}
};

const TCHAR* FBlueprintHelperTaskRuntimePostOperationJson::KindToString(
	EBlueprintHelperTaskRuntimePostOperationKind Kind)
{
	return FBlueprintHelperTaskRuntimePostOperationTypeLocalUtils::FindKindName(Kind);
}

const TCHAR* FBlueprintHelperTaskRuntimePostOperationJson::StatusToString(
	EBlueprintHelperTaskRuntimePostOperationStatus Status)
{
	return FBlueprintHelperTaskRuntimePostOperationTypeLocalUtils::FindStatusName(Status);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePostOperationJson::RecordToJson(
	const FBlueprintHelperTaskRuntimePostOperationRecordEx& Record)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("kind"), KindToString(Record.Kind));
	Json->SetStringField(TEXT("operation"), Record.Operation);
	Json->SetStringField(TEXT("asset_path"), Record.AssetPath);
	Json->SetStringField(TEXT("status"), StatusToString(Record.Status));
	if (!Record.Reason.IsEmpty())
	{
		Json->SetStringField(TEXT("reason"), Record.Reason);
	}
	Json->SetNumberField(TEXT("duration_ms"), Record.DurationMs);
	Json->SetObjectField(TEXT("result"), Record.Result.ToJson());
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePostOperationJson::PlanToJson(
	const FBlueprintHelperTaskRuntimePostOperationPlan& Plan)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("requested_compile"), Plan.bRequestedCompile);
	Json->SetBoolField(TEXT("requested_save"), Plan.bRequestedSave);
	Json->SetBoolField(TEXT("has_target_assets"), Plan.bHasTargetAssets);
	if (!Plan.MissingTargetAssetsReason.IsEmpty())
	{
		Json->SetStringField(TEXT("missing_target_assets_reason"), Plan.MissingTargetAssetsReason);
	}

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FBlueprintHelperTaskRuntimePostOperationPlanItem& Item : Plan.Items)
	{
		Items.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperTaskRuntimePostOperationTypeLocalUtils::PlanItemToJson(Item)));
	}
	Json->SetArrayField(TEXT("items"), Items);
	return Json;
}
