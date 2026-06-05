#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCompileDiagnosticsNodeAttributionJsonTest,
	"BlueprintHelper.RuntimeDiagnostics.Compile.NodeAttributedDiagnosticsJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCompileDiagnosticsNodeAttributionJsonTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperCompileResult CompileResult;
	CompileResult.bSuccess = false;
	CompileResult.BlueprintStatus = 1;

	FBlueprintHelperDiagnosticItem Diagnostic;
	Diagnostic.Severity = EBlueprintHelperDiagnosticSeverity::Error;
	Diagnostic.Code = TEXT("blueprint_compile_node_error");
	Diagnostic.Message = TEXT("Synthetic node compiler failure.");
	Diagnostic.GraphName = TEXT("EventGraph");
	Diagnostic.NodeId = TEXT("LegacyNodeId");
	Diagnostic.NodeName = TEXT("K2Node_CallFunction_0");
	Diagnostic.NodeGuid = TEXT("11111111-2222-3333-4444-555555555555");
	Diagnostic.NodeTitle = TEXT("Print String");
	Diagnostic.NodeClass = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Diagnostic.ErrorType = TEXT("compiler");
	CompileResult.Diagnostics.AddItem(MoveTemp(Diagnostic));

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperCompileAssetService::BuildResultFromCompileResult(
			TEXT("trace_compile_node_diagnostic"),
			TEXT("/Game/BP_CompileDiagnostics"),
			CompileResult,
			nullptr);

	TestTrue(TEXT("compile result data exists"), Result.Data.IsValid());
	if (!Result.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* CompileResultJson = nullptr;
	TestTrue(TEXT("compile_result exists"),
		Result.Data->TryGetObjectField(TEXT("compile_result"), CompileResultJson));
	if (!CompileResultJson || !CompileResultJson->IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CompilerResults = nullptr;
	TestTrue(TEXT("compiler_results exists"),
		(*CompileResultJson)->TryGetArrayField(TEXT("compiler_results"), CompilerResults));
	TestTrue(TEXT("one compiler result exists"), CompilerResults && CompilerResults->Num() == 1);
	if (!CompilerResults || CompilerResults->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> DiagnosticJson = (*CompilerResults)[0]->AsObject();
	TestTrue(TEXT("compiler result object is valid"), DiagnosticJson.IsValid());
	if (!DiagnosticJson.IsValid())
	{
		return false;
	}

	FString Value;
	TestTrue(TEXT("diagnostic code is serialized"), DiagnosticJson->TryGetStringField(TEXT("code"), Value));
	TestEqual(TEXT("diagnostic code"), Value, FString(TEXT("blueprint_compile_node_error")));
	TestTrue(TEXT("graph_name is serialized"), DiagnosticJson->TryGetStringField(TEXT("graph_name"), Value));
	TestFalse(TEXT("graph_name is non-empty"), Value.IsEmpty());
	TestTrue(TEXT("node_guid is serialized"), DiagnosticJson->TryGetStringField(TEXT("node_guid"), Value));
	TestFalse(TEXT("node_guid is non-empty"), Value.IsEmpty());
	TestTrue(TEXT("node_title is serialized"), DiagnosticJson->TryGetStringField(TEXT("node_title"), Value));
	TestFalse(TEXT("node_title is non-empty"), Value.IsEmpty());
	TestTrue(TEXT("node_class is serialized"), DiagnosticJson->TryGetStringField(TEXT("node_class"), Value));
	TestFalse(TEXT("node_class is non-empty"), Value.IsEmpty());
	TestTrue(TEXT("error_type is serialized"), DiagnosticJson->TryGetStringField(TEXT("error_type"), Value));
	TestEqual(TEXT("error_type is compiler"), Value, FString(TEXT("compiler")));
	TestTrue(TEXT("legacy node_id is still serialized"), DiagnosticJson->TryGetStringField(TEXT("node_id"), Value));
	TestTrue(TEXT("legacy node_name is still serialized"), DiagnosticJson->TryGetStringField(TEXT("node_name"), Value));
	return true;
}

#endif
