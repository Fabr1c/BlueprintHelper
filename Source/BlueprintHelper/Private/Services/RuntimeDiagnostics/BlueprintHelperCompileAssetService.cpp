// BlueprintHelper Service Layer 。CompileBlueprintAsset 服务实现

#include "Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileService.h"
#include "Structure/BlueprintHelperServiceTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"

FBlueprintHelperCompileAssetService::FBlueprintHelperCompileAssetService(
	const FBlueprintHelperCompileService& InCompileService)
	: CompileService(InCompileService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperCompileAssetService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FString AssetPath;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("compile_blueprint_asset"), TraceId,
			{TEXT("invalid_request"), EBlueprintHelperToolStage::ParseInput, TEXT("缺少 asset_path。"), false});
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
		return R;
	}

	// Build compile_result
	FBlueprintHelperCompileAssetResultData Data;
	Data.CompileResult.bSuccess = Cr.bSuccess;
	Data.CompileResult.Status = Cr.bSuccess ? TEXT("succeeded") : TEXT("failed");
	Data.CompileResult.WarningCount = Cr.Diagnostics.WarningCount;

	if (!Cr.bSuccess)
	{
		Data.CompileResult.Format = TEXT("markdown");
		FString Markdown = TEXT("## Compile Errors\n\n");
		for (const auto& Item : Cr.Diagnostics.Items)
		{
			if (Item.Severity == EBlueprintHelperDiagnosticSeverity::Error)
			{
				Markdown += FString::Printf(TEXT("- `unmapped`: %s\n"), *Item.Message);
			}
		}
		Data.CompileResult.Markdown = Markdown;
	}

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
	Result.Operation = TEXT("compile_blueprint_asset");
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;
	Result.CustomTargetJson = Tgt;
	Result.Data = Data.ToJson();
	return Result;
}
