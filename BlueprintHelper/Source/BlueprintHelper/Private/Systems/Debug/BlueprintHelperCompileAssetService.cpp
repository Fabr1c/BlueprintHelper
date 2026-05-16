// BlueprintHelper Service Layer 。CompileBlueprintAsset 服务实现

#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Shared/BlueprintHelperServiceTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"

FBlueprintHelperCompileAssetService::FBlueprintHelperCompileAssetService(
	const FBlueprintHelperCompileService& InCompileService,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: CompileService(InCompileService)
	, DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperCompileAssetService::BuildResultFromCompileResult(
	const FString& TraceId,
	const FString& AssetPath,
	const FBlueprintHelperCompileResult& CompileResult,
	const FBlueprintHelperDebugEntryService* DebugEntryService)
{
	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("asset_path"), AssetPath);

	FBlueprintHelperCompileAssetResultData Data;
	Data.CompileResult.bSuccess = CompileResult.bSuccess;
	Data.CompileResult.Status = CompileResult.bSuccess ? TEXT("succeeded") : TEXT("failed");
	Data.CompileResult.ErrorCount = CompileResult.Diagnostics.ErrorCount;
	Data.CompileResult.WarningCount = CompileResult.Diagnostics.WarningCount;
	Data.CompileResult.CompilerResults = CompileResult.Diagnostics.Items;

	FString CompilerResultsMarkdown;
	if (!CompileResult.bSuccess)
	{
		Data.CompileResult.Format = TEXT("markdown");
		CompilerResultsMarkdown = TEXT("## Compiler Results\n\n");
		for (const FBlueprintHelperDiagnosticItem& Item : CompileResult.Diagnostics.Items)
		{
			const TCHAR* SeverityText = TEXT("info");
			if (Item.Severity == EBlueprintHelperDiagnosticSeverity::Error)
			{
				SeverityText = TEXT("error");
			}
			else if (Item.Severity == EBlueprintHelperDiagnosticSeverity::Warning)
			{
				SeverityText = TEXT("warning");
			}

			if (!Item.Message.IsEmpty())
			{
				CompilerResultsMarkdown += FString::Printf(TEXT("- `%s`: %s\n"), SeverityText, *Item.Message);
			}
		}
		if (CompilerResultsMarkdown == TEXT("## Compiler Results\n\n"))
		{
			CompilerResultsMarkdown += TEXT("- `unmapped`: Blueprint compile failed without mapped diagnostics.\n");
		}
		Data.CompileResult.Markdown = CompilerResultsMarkdown;
	}

	if (!CompileResult.bSuccess)
	{
		FBlueprintHelperToolError Err;
		Err.Code = TEXT("compile_failed");
		Err.Stage = EBlueprintHelperToolStage::Execute;
		Err.Message = FString::Printf(
			TEXT("Blueprint compile failed for %s with %d error(s)."),
			*AssetPath,
			CompileResult.Diagnostics.ErrorCount);
		Err.Expected = TEXT("Blueprint compile success");
		Err.Actual = CompilerResultsMarkdown;
		Err.bRetryable = false;

		FBlueprintHelperToolResultBase Failure =
			FBlueprintHelperToolResultBuilder::Failure(TEXT("compile_blueprint_asset"), TraceId, Err);
		Failure.CustomTargetJson = Tgt;
		Failure.Data = Data.ToJson();
		if (DebugEntryService)
		{
			FBlueprintHelperDebugEntryEventInput DebugInput;
			DebugInput.SourceLayer = TEXT("debug");
			DebugInput.Source = TEXT("compile_failure");
			DebugInput.Stage = TEXT("execute");
			DebugInput.AssetPaths.Add(AssetPath);
			DebugInput.RecommendedNext = TEXT("fix_blueprint_compile_errors");
			DebugEntryService->AttachDebugCaseToFailureBestEffort(Failure, DebugInput);
		}
		return Failure;
	}

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = FBlueprintHelperProtocol::ToolResultSchema;
	Result.Operation = TEXT("compile_blueprint_asset");
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;
	Result.CustomTargetJson = Tgt;
	Result.Data = Data.ToJson();
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperCompileAssetService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FString AssetPath;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(TEXT("compile_blueprint_asset"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 asset_path。"), false});
		if (DebugEntryService)
		{
			FBlueprintHelperDebugEntryEventInput DebugInput;
			DebugInput.SourceLayer = TEXT("debug");
			DebugInput.Source = TEXT("compile_failure");
			DebugInput.Stage = TEXT("parse_input");
			DebugInput.RecommendedNext = TEXT("provide_asset_path");
			DebugEntryService->AttachDebugCaseToFailureBestEffort(Result, DebugInput);
		}
		return Result;
	}

	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("asset_path"), AssetPath);

	// Use existing CompileService
	FBlueprintHelperGraphTarget GraphTarget;
	GraphTarget.BlueprintPath = AssetPath;

	const FBlueprintHelperCompileResult Cr = CompileService.Compile(GraphTarget);

	// Tool failure: asset not found
	if (!Cr.bSuccess && Cr.BlueprintStatus == 0 && Cr.Diagnostics.ErrorCount == 0)
	{
		FBlueprintHelperToolError Err;
		Err.Code = TEXT("asset_not_found");
		Err.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Err.Message = FString::Printf(TEXT("蓝图资产 %s 未找到。"), *AssetPath);
		Err.bRetryable = false;
		auto R = FBlueprintHelperToolResultBuilder::Failure(TEXT("compile_blueprint_asset"), TraceId, Err);
		R.CustomTargetJson = Tgt;
		if (DebugEntryService)
		{
			FBlueprintHelperDebugEntryEventInput DebugInput;
			DebugInput.SourceLayer = TEXT("debug");
			DebugInput.Source = TEXT("compile_failure");
			DebugInput.Stage = TEXT("resolve_target");
			DebugInput.AssetPaths.Add(AssetPath);
			DebugInput.RecommendedNext = TEXT("verify_asset_path");
			DebugEntryService->AttachDebugCaseToFailureBestEffort(R, DebugInput);
		}
		return R;
	}

	return BuildResultFromCompileResult(TraceId, AssetPath, Cr, DebugEntryService);
}
