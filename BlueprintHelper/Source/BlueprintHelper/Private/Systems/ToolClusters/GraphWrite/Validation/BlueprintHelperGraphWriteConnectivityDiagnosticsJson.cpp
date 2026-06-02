#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityDiagnosticsJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FBlueprintHelperGraphWriteConnectivityDiagnosticsJson::Attach(
	const TSharedPtr<FJsonObject>& Data,
	const TArray<FBlueprintGeneratorDiagnostic>& Diagnostics)
{
	if (!Data.IsValid() || Diagnostics.Num() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Violations;
	for (const FBlueprintGeneratorDiagnostic& Diagnostic : Diagnostics)
	{
		TSharedRef<FJsonObject> Violation = MakeShared<FJsonObject>();
		Violation->SetStringField(TEXT("code"), Diagnostic.Code);
		Violation->SetStringField(TEXT("node_id"), Diagnostic.NodeId);
		Violation->SetStringField(TEXT("message"), Diagnostic.Message);
		Violations.Add(MakeShared<FJsonValueObject>(Violation));
	}

	TSharedRef<FJsonObject> Connectivity = MakeShared<FJsonObject>();
	Connectivity->SetArrayField(TEXT("violations"), Violations);
	Data->SetObjectField(TEXT("connectivity"), Connectivity);
}
