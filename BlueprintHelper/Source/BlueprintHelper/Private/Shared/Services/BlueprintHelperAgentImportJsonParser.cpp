#include "Shared/Services/BlueprintHelperAgentImportJsonParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
void AddAgentImportDiagnostic(
	FBlueprintHelperAgentImportResult& Result,
	EBlueprintHelperAgentImportDiagnosticSeverity Severity,
	const FString& Code,
	const FString& Path,
	const FString& Message,
	const FString& Suggestion = FString())
{
	FBlueprintHelperAgentImportDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.Path = Path;
	Diagnostic.Message = Message;
	Diagnostic.Suggestion = Suggestion;
	Result.Diagnostics.Add(Diagnostic);

	if (Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Error)
	{
		++Result.ErrorCount;
		if (Result.ErrorCode.IsEmpty())
		{
			Result.ErrorCode = Code;
		}
	}
	else if (Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Warning)
	{
		++Result.WarningCount;
	}
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Root,
	const TCHAR* FieldName,
	const FString& Path,
	FString& OutValue,
	FBlueprintHelperAgentImportResult& Result)
{
	if (!Root->TryGetStringField(FieldName, OutValue) || OutValue.TrimStartAndEnd().IsEmpty())
	{
		AddAgentImportDiagnostic(
			Result,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			FString::Printf(TEXT("missing_%s"), FieldName),
			Path,
			FString::Printf(TEXT("Required string field '%s' is missing or empty."), FieldName));
		return false;
	}

	OutValue = OutValue.TrimStartAndEnd();
	return true;
}


void ParseOptions(
	const TSharedPtr<FJsonObject>& Root,
	FBlueprintHelperAgentImportOptions& OutOptions)
{
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("options"), OptionsObject) || !OptionsObject || !OptionsObject->IsValid())
	{
		return;
	}

	(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), OutOptions.bDryRun);
	(*OptionsObject)->TryGetBoolField(TEXT("strict"), OutOptions.bStrict);
	(*OptionsObject)->TryGetBoolField(TEXT("compile"), OutOptions.bCompile);
	(*OptionsObject)->TryGetBoolField(TEXT("save"), OutOptions.bSave);
	(*OptionsObject)->TryGetBoolField(TEXT("create_missing_variables"), OutOptions.bCreateMissingVariables);
	(*OptionsObject)->TryGetBoolField(TEXT("reconstruct_existing_nodes"), OutOptions.bReconstructExistingNodes);
}

void RejectRetiredRootFields(
	const TSharedPtr<FJsonObject>& Root,
	FBlueprintHelperAgentImportResult& Result)
{
	const TCHAR* RetiredFields[] = {
		TEXT("nodes"),
		TEXT("links"),
		TEXT("declarations"),
		TEXT("layout")
	};

	for (const TCHAR* FieldName : RetiredFields)
	{
		const FString Field(FieldName);
		if (!Root->HasField(Field))
		{
			continue;
		}

		AddAgentImportDiagnostic(
			Result,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("retired_agent_import_field"),
			TEXT("$.") + Field,
			FString::Printf(TEXT("AgentImport root field '%s' is retired and is no longer accepted."), *Field),
			TEXT("Use logic_spec/SemanticIR input only."));
	}
}

}

bool FBlueprintHelperAgentImportJsonParser::Parse(
	const FString& JsonText,
	FBlueprintHelperAgentImportParsedRequest& OutRequest,
	FBlueprintHelperAgentImportResult& OutResult)
{
	OutRequest = FBlueprintHelperAgentImportParsedRequest();
	OutResult = FBlueprintHelperAgentImportResult();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AddAgentImportDiagnostic(
			OutResult,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("invalid_json"),
			TEXT("$"),
			TEXT("AgentImport request must be a valid JSON object."));
		OutResult.bSuccess = false;
		OutResult.Status = TEXT("failed");
		OutResult.Message = OutResult.GetSummaryText();
		return false;
	}

	FString Schema;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("BlueprintHelper.AgentImportGraph"))
	{
		AddAgentImportDiagnostic(
			OutResult,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("unsupported_schema"),
			TEXT("$.schema"),
			TEXT("AgentImport requires schema 'BlueprintHelper.AgentImportGraph'."));
	}

	FString Version;
	if (!Root->TryGetStringField(TEXT("version"), Version) || Version != TEXT("1.0"))
	{
		AddAgentImportDiagnostic(
			OutResult,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("unsupported_version"),
			TEXT("$.version"),
			TEXT("AgentImport requires version '1.0'."));
	}

	ReadRequiredString(Root, TEXT("target_blueprint"), TEXT("$.target_blueprint"), OutRequest.Target.BlueprintPath, OutResult);
	ReadRequiredString(Root, TEXT("target_graph"), TEXT("$.target_graph"), OutRequest.Target.GraphName, OutResult);

	FString Mode;
	if (Root->TryGetStringField(TEXT("mode"), Mode) && !Mode.Equals(TEXT("append"), ESearchCase::IgnoreCase))
	{
		AddAgentImportDiagnostic(
			OutResult,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("unsupported_mode"),
			TEXT("$.mode"),
			TEXT("SemanticIR AgentImport currently supports append mode only."));
	}
	OutRequest.Mode = EBlueprintHelperAgentImportMode::Append;

	RejectRetiredRootFields(Root, OutResult);
	ParseOptions(Root, OutRequest.Options);

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) || !LogicSpecObject || !LogicSpecObject->IsValid())
	{
		AddAgentImportDiagnostic(
			OutResult,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("logic_spec_required"),
			TEXT("$.logic_spec"),
			TEXT("AgentImport now only accepts BlueprintLogicSpec / SemanticIR input through 'logic_spec'."),
			TEXT("Provide a statement tree under logic_spec."));
	}


	OutResult.bDryRun = OutRequest.Options.bDryRun;
	OutResult.bSuccess = !OutResult.HasErrors();
	OutResult.Status = OutResult.bSuccess ? TEXT("parsed") : TEXT("failed");
	OutResult.Message = OutResult.GetSummaryText();
	return OutResult.bSuccess;
}
