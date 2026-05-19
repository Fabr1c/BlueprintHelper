// BlueprintHelper TaskRuntime post IO batch DTO.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimePostIoFlushResult::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("ok"), bOk);

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperTaskRuntimePostIoDiagnostic& Diagnostic : Diagnostics)
	{
		TSharedRef<FJsonObject> DiagnosticJson = MakeShared<FJsonObject>();
		DiagnosticJson->SetStringField(TEXT("code"), Diagnostic.Code);
		DiagnosticJson->SetStringField(TEXT("message"), Diagnostic.Message);
		if (!Diagnostic.Field.IsEmpty())
		{
			DiagnosticJson->SetStringField(TEXT("field"), Diagnostic.Field);
		}
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(DiagnosticJson));
	}
	Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
	return Json;
}
