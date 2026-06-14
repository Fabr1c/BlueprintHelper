// BlueprintHelper MaterialGraph compile service boundary.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphCompileService.h"

#include "Dom/JsonObject.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "RHIFeatureLevel.h"

FString FBlueprintHelperMaterialGraphCompileService::BuildCompilePendingDiagnostic()
{
	return TEXT("MaterialGraph shader diagnostics are collected after material recompile; dry-run cannot prove shader compile output.");
}

TArray<FBlueprintHelperDiagnosticItem> FBlueprintHelperMaterialGraphCompileService::CollectExpressionDiagnostics(
	UMaterial* Material,
	const TMap<UMaterialExpression*, FString>& NodeKeyByExpression)
{
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	if (!Material)
	{
		return Diagnostics;
	}

	TSet<FString> SeenDiagnostics;
	if (FMaterialResource* MaterialResource = Material->GetMaterialResource(GMaxRHIFeatureLevel))
	{
		const TArray<FString>& CompileErrors = MaterialResource->GetCompileErrors();
		const TArray<UMaterialExpression*>& ErrorExpressions = MaterialResource->GetErrorExpressions();
		for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ++ErrorIndex)
		{
			UMaterialExpression* Expression = ErrorExpressions.IsValidIndex(ErrorIndex) ? ErrorExpressions[ErrorIndex] : nullptr;
			const FString ExpressionGuid = Expression
				? Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower)
				: FString();
			const FString Message = CompileErrors[ErrorIndex];
			const FString DedupeKey = ExpressionGuid + TEXT("|") + Message;
			if (Message.TrimStartAndEnd().IsEmpty() || SeenDiagnostics.Contains(DedupeKey))
			{
				continue;
			}

FBlueprintHelperDiagnosticItem Diagnostic;
			const bool bWarning = Message.Contains(TEXT("warning"), ESearchCase::IgnoreCase) &&
				!Message.Contains(TEXT("error"), ESearchCase::IgnoreCase);
			Diagnostic.Severity = bWarning
				? EBlueprintHelperDiagnosticSeverity::Warning
				: EBlueprintHelperDiagnosticSeverity::Error;
			Diagnostic.Code = bWarning ? TEXT("material_compile_warning") : TEXT("material_compile_error");
			Diagnostic.Message = Message;
			Diagnostic.GraphName = TEXT("MaterialGraph");
			Diagnostic.NodeGuid = ExpressionGuid;
			if (Expression)
			{
				Diagnostic.NodeClass = Expression->GetClass() ? Expression->GetClass()->GetName() : FString();
				if (const FString* NodeKey = NodeKeyByExpression.Find(Expression))
				{
					Diagnostic.NodeId = *NodeKey;
					Diagnostic.NodeName = *NodeKey;
					Diagnostic.TargetKey = FString::Printf(TEXT("material_expression:%s"), **NodeKey);
				}
			}
			Diagnostic.CompileDiagnosticCorrelationKey = ExpressionGuid.IsEmpty() ? Message : ExpressionGuid;
			SeenDiagnostics.Add(DedupeKey);
			Diagnostics.Add(MoveTemp(Diagnostic));
		}
	}

	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression || Expression->LastErrorText.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		const FString ExpressionGuid = Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		const FString DedupeKey = ExpressionGuid + TEXT("|") + Expression->LastErrorText;
		if (SeenDiagnostics.Contains(DedupeKey))
		{
			continue;
		}

		FBlueprintHelperDiagnosticItem Diagnostic;
		const bool bWarning = Expression->LastErrorText.Contains(TEXT("warning"), ESearchCase::IgnoreCase) &&
			!Expression->LastErrorText.Contains(TEXT("error"), ESearchCase::IgnoreCase);
		Diagnostic.Severity = bWarning
			? EBlueprintHelperDiagnosticSeverity::Warning
			: EBlueprintHelperDiagnosticSeverity::Error;
		Diagnostic.Code = bWarning ? TEXT("material_compile_warning") : TEXT("material_compile_error");
		Diagnostic.Message = Expression->LastErrorText;
		Diagnostic.GraphName = TEXT("MaterialGraph");
		Diagnostic.NodeClass = Expression->GetClass() ? Expression->GetClass()->GetName() : FString();
		Diagnostic.NodeGuid = ExpressionGuid;
		if (const FString* NodeKey = NodeKeyByExpression.Find(Expression))
		{
			Diagnostic.NodeId = *NodeKey;
			Diagnostic.NodeName = *NodeKey;
			Diagnostic.TargetKey = FString::Printf(TEXT("material_expression:%s"), **NodeKey);
		}
		Diagnostic.CompileDiagnosticCorrelationKey = Diagnostic.NodeGuid;
		SeenDiagnostics.Add(DedupeKey);
		Diagnostics.Add(MoveTemp(Diagnostic));
	}
	return Diagnostics;
}

bool FBlueprintHelperMaterialGraphCompileService::HasBlockingCompileErrors(
	const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics)
{
	for (const FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity == EBlueprintHelperDiagnosticSeverity::Error)
		{
			return true;
		}
	}
	return false;
}

FBlueprintHelperToolError FBlueprintHelperMaterialGraphCompileService::BuildBlockingCompileError(
	const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics)
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("material_compile_error");
	Error.Stage = EBlueprintHelperToolStage::Validate;
	Error.Message = TEXT("MaterialGraph edit produced shader compile errors.");
	Error.bRetryable = false;
	Error.RollbackResult = EBlueprintHelperRollbackResult::Unavailable;
	Error.Field = TEXT("compile_result");

	for (const FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity != EBlueprintHelperDiagnosticSeverity::Error)
		{
			continue;
		}

		if (!Diagnostic.Message.IsEmpty())
		{
			Error.Message = FString::Printf(
				TEXT("MaterialGraph edit produced shader compile errors: %s"),
				*Diagnostic.Message);
		}
		if (!Diagnostic.TargetKey.IsEmpty())
		{
			Error.Actual = Diagnostic.TargetKey;
		}
		else if (!Diagnostic.NodeId.IsEmpty())
		{
			Error.Actual = Diagnostic.NodeId;
		}
		break;
	}
	return Error;
}

TSharedRef<FJsonObject> FBlueprintHelperMaterialGraphCompileService::BuildCompileResultJson(
	const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics)
{
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	for (const FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity == EBlueprintHelperDiagnosticSeverity::Error)
		{
			++ErrorCount;
		}
		else if (Diagnostic.Severity == EBlueprintHelperDiagnosticSeverity::Warning)
		{
			++WarningCount;
		}
	}

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialCompileResult.v1"));
	Json->SetBoolField(TEXT("success"), ErrorCount == 0);
	Json->SetNumberField(TEXT("errors"), ErrorCount);
	Json->SetNumberField(TEXT("warnings"), WarningCount);
	Json->SetArrayField(TEXT("diagnostics"), BlueprintHelperDiagnosticItemsToJsonArray(Diagnostics));
	return Json;
}
