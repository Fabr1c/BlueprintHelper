// BlueprintHelper Service Layer — 导入服务实现

#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperValidationService.h"
#include "Services/BlueprintHelperCompileService.h"
#include "TextToBlueprintGenerator.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FBlueprintHelperImportService::FBlueprintHelperImportService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperValidationService& InValidator)
	: Resolver(InResolver)
	, Validator(InValidator)
{
}

void FBlueprintHelperImportService::SetCompileService(const FBlueprintHelperCompileService* InCompileService)
{
	CompileService = InCompileService;
}

FBlueprintHelperImportResult FBlueprintHelperImportService::Import(const FBlueprintHelperImportRequest& Request) const
{
	FBlueprintHelperImportResult Result;

	// 1. 校验
	FBlueprintHelperValidationResult ValResult = Validator.Validate(Request.JsonText);
	if (!ValResult.bValid)
	{
		Result.Diagnostics = MoveTemp(ValResult.Diagnostics);
		return Result;
	}

	// 2. 事务包裹（整体 Undo 支持）
	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Import JSON")));

	// 3. 根据 JSON 结构选择单图/多图路径
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	FBlueprintGenerateResult GenResult;

	if (NeedsMultiGraphPath(Request.JsonText))
	{
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(Request.Target, Result.Diagnostics);
		if (!Blueprint)
		{
			return Result;
		}
		GenResult = TextToBlueprintGenerator::GenerateMultiGraphFromJson(Blueprint, Request.JsonText, Unresolved);
	}
	else
	{
		UEdGraph* Graph = Resolver.ResolveGraph(Request.Target, Result.Diagnostics);
		if (!Graph)
		{
			return Result;
		}
		GenResult = TextToBlueprintGenerator::GenerateBlueprintFromJson(Graph, Request.JsonText, Unresolved);
	}

	// 4. 转换结果
	Result.bSuccess = GenResult.bSucceed;
	Result.GeneratedNodeCount = GenResult.GeneratedNodeCount;
	Result.UnresolvedNodeCount = GenResult.UnresolvedNodeCount;

	for (const TSharedPtr<FUnresolvedNodeItem>& Item : Unresolved)
	{
		if (Item.IsValid())
		{
			Result.UnresolvedNodeSummaries.Add(
				FString::Printf(TEXT("%s: %s"), *Item->NodeData.FunctionName, *Item->Reason));
		}
	}

	if (!Result.bSuccess && Result.UnresolvedNodeCount > 0)
	{
		Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Warning,
			FString::Printf(TEXT("%d 个节点未匹配。"), Result.UnresolvedNodeCount));
	}

	// 5. 可选自动编译
	if (Request.bAutoCompile && Result.bSuccess && CompileService)
	{
		FBlueprintHelperCompileResult CompileResult = CompileService->Compile(Request.Target);
		if (!CompileResult.bSuccess)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Warning,
				FString::Printf(TEXT("自动编译完成但有错误：%d 个错误。"), CompileResult.Diagnostics.ErrorCount));
		}
	}

	return Result;
}

bool FBlueprintHelperImportService::NeedsMultiGraphPath(const FString& JsonText) const
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	return Root->HasField(TEXT("graphs")) || Root->HasField(TEXT("blueprint_operations"));
}
