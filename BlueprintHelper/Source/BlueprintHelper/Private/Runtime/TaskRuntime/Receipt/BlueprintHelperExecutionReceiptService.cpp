// BlueprintHelper execution receipt JSON service.

#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperExecutionReceiptServiceLocalUtils
{
public:
	static void CopyStringField(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceField,
		const TSharedPtr<FJsonObject>& Target,
		const TCHAR* TargetField)
	{
		if (!Source.IsValid() || !Target.IsValid())
		{
			return;
		}

		FString Value;
		if (Source->TryGetStringField(SourceField, Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TargetField, Value);
		}
	}

	static void CopyStringFieldIfMissing(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* SourceField,
		const TSharedPtr<FJsonObject>& Target,
		const TCHAR* TargetField)
	{
		if (!Source.IsValid() || !Target.IsValid() || Target->HasTypedField<EJson::String>(TargetField))
		{
			return;
		}
		CopyStringField(Source, SourceField, Target, TargetField);
	}

	static void CopyTargetAssetsIfMissing(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& Receipt)
	{
		if (!TaskPlan.IsValid() || !Receipt.IsValid() || Receipt->HasField(FBlueprintHelperExecutionReceiptFields::TargetAssets()))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
		{
			Receipt->SetArrayField(FBlueprintHelperExecutionReceiptFields::TargetAssets(), *TargetAssets);
		}
	}

	static void EnsureTimestampFields(const TSharedPtr<FJsonObject>& Receipt)
	{
		if (!Receipt.IsValid())
		{
			return;
		}

		const FString Now = FDateTime::UtcNow().ToIso8601();
		if (!Receipt->HasTypedField<EJson::String>(FBlueprintHelperExecutionReceiptFields::CreatedAt()))
		{
			Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::CreatedAt(), Now);
		}
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::UpdatedAt(), Now);
	}

	static TSharedPtr<FJsonObject> ReadNestedReceipt(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName)
	{
		const TSharedPtr<FJsonObject>* Receipt = nullptr;
		if (Json.IsValid() &&
			Json->TryGetObjectField(FieldName, Receipt) &&
			Receipt && Receipt->IsValid())
		{
			return *Receipt;
		}
		return nullptr;
	}
};

TSharedPtr<FJsonObject> FBlueprintHelperExecutionReceiptService::CloneJsonObject(
	const TSharedPtr<FJsonObject>& Source)
{
	if (!Source.IsValid())
	{
		return nullptr;
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	if (!FJsonSerializer::Serialize(Source.ToSharedRef(), Writer))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Cloned;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
	if (!FJsonSerializer::Deserialize(Reader, Cloned))
	{
		return nullptr;
	}
	return Cloned;
}

TSharedPtr<FJsonObject> FBlueprintHelperExecutionReceiptService::ReadReceiptFromPayload(
	const TSharedPtr<FJsonObject>& Payload)
{
	TSharedPtr<FJsonObject> Receipt = FBlueprintHelperExecutionReceiptServiceLocalUtils::ReadNestedReceipt(
		Payload,
		FBlueprintHelperExecutionReceiptFields::Receipt());
	if (Receipt.IsValid())
	{
		return CloneJsonObject(Receipt);
	}

	const TSharedPtr<FJsonObject>* TokenRequest = nullptr;
	if (Payload.IsValid() &&
		Payload->TryGetObjectField(TEXT("preview_token_request"), TokenRequest) &&
		TokenRequest && TokenRequest->IsValid())
	{
		Receipt = FBlueprintHelperExecutionReceiptServiceLocalUtils::ReadNestedReceipt(
			*TokenRequest,
			FBlueprintHelperExecutionReceiptFields::Receipt());
		if (Receipt.IsValid())
		{
			return CloneJsonObject(Receipt);
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FBlueprintHelperExecutionReceiptService::ReadReceiptFromJournal(
	const TSharedPtr<FJsonObject>& Journal)
{
	return CloneJsonObject(FBlueprintHelperExecutionReceiptServiceLocalUtils::ReadNestedReceipt(
		Journal,
		FBlueprintHelperExecutionReceiptFields::Receipt()));
}

TSharedPtr<FJsonObject> FBlueprintHelperExecutionReceiptService::BuildPreviewReceipt(
	const TSharedPtr<FJsonObject>& PreviewTokenRequest,
	const TSharedPtr<FJsonObject>& TaskPlan,
	bool bPassed)
{
	TSharedPtr<FJsonObject> Receipt = FBlueprintHelperExecutionReceiptServiceLocalUtils::ReadNestedReceipt(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::Receipt());
	Receipt = Receipt.IsValid() ? CloneJsonObject(Receipt) : MakeShared<FJsonObject>();
	if (!Receipt.IsValid())
	{
		return nullptr;
	}

	Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::SchemaField(), FBlueprintHelperExecutionReceiptFields::Schema());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::ReceiptId(),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::ReceiptId());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::CliRunId(),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::CliRunId());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::PreviewId(),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::PreviewId());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::TaskSpecHash(),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::TaskSpecHash());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		FBlueprintHelperExecutionReceiptFields::TaskPlanHash(),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::TaskPlanHash());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringFieldIfMissing(
		PreviewTokenRequest,
		TEXT("execution_policy_hash"),
		Receipt,
		FBlueprintHelperExecutionReceiptFields::PolicyHash());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyTargetAssetsIfMissing(TaskPlan, Receipt);
	Receipt->SetStringField(
		FBlueprintHelperExecutionReceiptFields::Status(),
		bPassed ? TEXT("previewed") : TEXT("preview_blocked"));
	FBlueprintHelperExecutionReceiptServiceLocalUtils::EnsureTimestampFields(Receipt);
	return Receipt;
}

TSharedPtr<FJsonObject> FBlueprintHelperExecutionReceiptService::BuildExecuteReceipt(
	const TSharedPtr<FJsonObject>& Payload,
	const FString& TaskRunId,
	bool bOk,
	const FString& JournalRef)
{
	TSharedPtr<FJsonObject> Receipt = ReadReceiptFromPayload(Payload);
	if (!Receipt.IsValid())
	{
		return nullptr;
	}

	Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::SchemaField(), FBlueprintHelperExecutionReceiptFields::Schema());
	if (!TaskRunId.IsEmpty())
	{
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::TaskRunId(), TaskRunId);
	}
	if (!JournalRef.IsEmpty())
	{
		Receipt->SetStringField(FBlueprintHelperExecutionReceiptFields::JournalRef(), JournalRef);
	}
	Receipt->SetStringField(
		FBlueprintHelperExecutionReceiptFields::Status(),
		bOk ? TEXT("applied") : TEXT("failed"));
	FBlueprintHelperExecutionReceiptServiceLocalUtils::EnsureTimestampFields(Receipt);
	return Receipt;
}

void FBlueprintHelperExecutionReceiptService::AttachReceiptToResultData(
	TSharedPtr<FJsonObject>& Data,
	const TSharedPtr<FJsonObject>& Receipt)
{
	if (!Receipt.IsValid())
	{
		return;
	}
	if (!Data.IsValid())
	{
		Data = MakeShared<FJsonObject>();
	}
	Data->SetObjectField(FBlueprintHelperExecutionReceiptFields::Receipt(), CloneJsonObject(Receipt).ToSharedRef());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringField(
		Receipt,
		FBlueprintHelperExecutionReceiptFields::ReceiptId(),
		Data,
		FBlueprintHelperExecutionReceiptFields::ReceiptId());
	FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringField(
		Receipt,
		FBlueprintHelperExecutionReceiptFields::TaskRunId(),
		Data,
		FBlueprintHelperExecutionReceiptFields::TaskRunId());
}

void FBlueprintHelperExecutionReceiptService::AttachReceiptToJournal(
	const TSharedPtr<FJsonObject>& Journal,
	const TSharedPtr<FJsonObject>& Receipt)
{
	if (!Journal.IsValid() || !Receipt.IsValid())
	{
		return;
	}
	Journal->SetObjectField(FBlueprintHelperExecutionReceiptFields::Receipt(), CloneJsonObject(Receipt).ToSharedRef());
	for (const TCHAR* Field : {
		FBlueprintHelperExecutionReceiptFields::ReceiptId(),
		FBlueprintHelperExecutionReceiptFields::RequestId(),
		FBlueprintHelperExecutionReceiptFields::CliRunId(),
		FBlueprintHelperExecutionReceiptFields::PreviewId(),
		FBlueprintHelperExecutionReceiptFields::TaskRunId(),
		FBlueprintHelperExecutionReceiptFields::TaskSpecHash(),
		FBlueprintHelperExecutionReceiptFields::TaskPlanHash(),
		FBlueprintHelperExecutionReceiptFields::PolicyHash(),
		FBlueprintHelperExecutionReceiptFields::JournalRef()
	})
	{
		FBlueprintHelperExecutionReceiptServiceLocalUtils::CopyStringField(Receipt, Field, Journal, Field);
	}
}
