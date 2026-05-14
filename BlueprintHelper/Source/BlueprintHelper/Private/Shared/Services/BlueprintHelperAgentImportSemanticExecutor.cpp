#include "Shared/Services/BlueprintHelperAgentImportSemanticExecutor.h"

#include "EdGraph/EdGraph.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

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

EBlueprintHelperAgentImportDiagnosticSeverity ConvertSeverity(EBlueprintHelperDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperDiagnosticSeverity::Info:
		return EBlueprintHelperAgentImportDiagnosticSeverity::Info;
	case EBlueprintHelperDiagnosticSeverity::Warning:
		return EBlueprintHelperAgentImportDiagnosticSeverity::Warning;
	case EBlueprintHelperDiagnosticSeverity::Error:
		return EBlueprintHelperAgentImportDiagnosticSeverity::Error;
	default:
		return EBlueprintHelperAgentImportDiagnosticSeverity::Error;
	}
}

EBlueprintHelperAgentImportDiagnosticSeverity ConvertGeneratorSeverity(const FString& Severity)
{
	if (Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperAgentImportDiagnosticSeverity::Error;
	}
	if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperAgentImportDiagnosticSeverity::Warning;
	}
	return EBlueprintHelperAgentImportDiagnosticSeverity::Info;
}

FString MakeServiceDiagnosticPath(const FBlueprintHelperDiagnosticItem& Diagnostic)
{
	if (!Diagnostic.NodeId.IsEmpty() && !Diagnostic.PinName.IsEmpty())
	{
		return FString::Printf(TEXT("$.target_graph.node[%s].pin[%s]"), *Diagnostic.NodeId, *Diagnostic.PinName);
	}
	if (!Diagnostic.NodeId.IsEmpty())
	{
		return FString::Printf(TEXT("$.target_graph.node[%s]"), *Diagnostic.NodeId);
	}
	if (!Diagnostic.Field.IsEmpty())
	{
		return FString::Printf(TEXT("$.%s"), *Diagnostic.Field);
	}
	return TEXT("$.target_graph");
}

FString MakeGeneratorDiagnosticPath(const FBlueprintGeneratorDiagnostic& Diagnostic)
{
	if (!Diagnostic.NodeId.IsEmpty() && !Diagnostic.PinName.IsEmpty())
	{
		return FString::Printf(TEXT("$.logic_spec.node[%s].pin[%s]"), *Diagnostic.NodeId, *Diagnostic.PinName);
	}
	if (!Diagnostic.NodeId.IsEmpty())
	{
		return FString::Printf(TEXT("$.logic_spec.node[%s]"), *Diagnostic.NodeId);
	}
	return TEXT("$.logic_spec");
}
}

FBlueprintHelperAgentImportSemanticExecutor::FBlueprintHelperAgentImportSemanticExecutor(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperAgentImportResult FBlueprintHelperAgentImportSemanticExecutor::Execute(
	const FString& OriginalJsonText,
	const FBlueprintHelperAgentImportParsedRequest& ParsedRequest) const
{
	FBlueprintHelperAgentImportResult Result;
	Result.bDryRun = ParsedRequest.Options.bDryRun;

	FBlueprintHelperDiagnosticSet ResolveDiagnostics;
	UEdGraph* TargetGraph = Resolver.ResolveGraph(ParsedRequest.Target, ResolveDiagnostics);
	for (const FBlueprintHelperDiagnosticItem& Diagnostic : ResolveDiagnostics.Items)
	{
		AddAgentImportDiagnostic(
			Result,
			ConvertSeverity(Diagnostic.Severity),
			Diagnostic.Code,
			MakeServiceDiagnosticPath(Diagnostic),
			Diagnostic.Message);
	}

	if (!TargetGraph)
	{
		AddAgentImportDiagnostic(
			Result,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("target_graph_not_found"),
			TEXT("$.target_graph"),
			TEXT("Unable to resolve target graph for SemanticIR AgentImport."));
		Result.bSuccess = false;
		Result.Status = TEXT("failed");
		Result.Message = Result.GetSummaryText();
		return Result;
	}

	if (Result.HasErrors())
	{
		Result.bSuccess = false;
		Result.Status = TEXT("failed");
		Result.Message = Result.GetSummaryText();
		return Result;
	}

	if (ParsedRequest.Options.bDryRun)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("dry_run");
		Result.Message = TEXT("SemanticIR AgentImport dry_run validation passed.");
		return Result;
	}

	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult = FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
		TargetGraph,
		OriginalJsonText,
		UnresolvedNodes);

	Result.CreatedNodeCount = GenerateResult.GeneratedNodeCount;
	Result.CreatedLinkCount = GenerateResult.CreatedConnectionCount;

	for (const FBlueprintGeneratorDiagnostic& Diagnostic : GenerateResult.ConnectionDiagnostics)
	{
		AddAgentImportDiagnostic(
			Result,
			ConvertGeneratorSeverity(Diagnostic.Severity),
			Diagnostic.Code,
			MakeGeneratorDiagnosticPath(Diagnostic),
			Diagnostic.Message);
	}

	for (const TSharedPtr<FUnresolvedNodeItem>& UnresolvedNode : UnresolvedNodes)
	{
		if (!UnresolvedNode.IsValid())
		{
			continue;
		}

		AddAgentImportDiagnostic(
			Result,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("unresolved_semantic_node"),
			TEXT("$.logic_spec"),
			FString::Printf(TEXT("%s: %s"), *UnresolvedNode->DisplayText, *UnresolvedNode->Reason));
	}

	Result.bSuccess = GenerateResult.bSucceed && !Result.HasErrors();
	if (!Result.bSuccess)
	{
		if (Result.ErrorCode.IsEmpty())
		{
			Result.ErrorCode = GenerateResult.Message.IsEmpty()
				? TEXT("semantic_graph_write_failed")
				: GenerateResult.Message;
		}
		Result.Status = TEXT("failed");
		Result.Message = Result.GetSummaryText();
		return Result;
	}

	Result.Status = Result.WarningCount > 0 ? TEXT("partial_success") : TEXT("full_success");
	Result.Message = Result.GetSummaryText();
	return Result;
}