#include "Shared/Services/BlueprintHelperAgentImportService.h"

#include "Shared/Services/BlueprintHelperAgentImportJsonParser.h"
#include "Shared/Services/BlueprintHelperAgentImportSemanticExecutor.h"

FBlueprintHelperAgentImportService::FBlueprintHelperAgentImportService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperCompileService& InCompileService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService)
	: Resolver(InResolver)
	, CompileService(InCompileService)
	, AssetBrowseService(InAssetBrowseService)
{
}

FBlueprintHelperAgentImportResult FBlueprintHelperAgentImportService::Import(const FBlueprintHelperAgentImportRequest& Request) const
{
	FBlueprintHelperAgentImportParsedRequest ParsedRequest;
	FBlueprintHelperAgentImportResult ParseResult;
	if (!FBlueprintHelperAgentImportJsonParser::Parse(Request.JsonText, ParsedRequest, ParseResult))
	{
		return ParseResult;
	}

	FBlueprintHelperAgentImportSemanticExecutor Executor(Resolver);
	return Executor.Execute(Request.JsonText, ParsedRequest);
}

bool FBlueprintHelperAgentImportResult::HasErrors() const
{
	return ErrorCount > 0 || !ErrorCode.IsEmpty();
}

FString FBlueprintHelperAgentImportResult::GetSummaryText() const
{
	return FString::Printf(
		TEXT("AgentImport %s: nodes=%d links=%d warnings=%d errors=%d"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		CreatedNodeCount,
		CreatedLinkCount,
		WarningCount,
		ErrorCount);
}