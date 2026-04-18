// BlueprintHelper Service Layer — 导出服务实现

#include "Services/BlueprintHelperExportService.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "BlueprintTextConverter.h"

FBlueprintHelperExportService::FBlueprintHelperExportService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperExportResult FBlueprintHelperExportService::Export(const FBlueprintHelperExportRequest& Request) const
{
	FBlueprintHelperExportResult Result;

	if (Request.Scope == EBlueprintHelperExportScope::FullBlueprint)
	{
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(Request.Target, Result.Diagnostics);
		if (!Blueprint)
		{
			return Result;
		}

		Result.JsonText = FBlueprintToTextConverter::ExportBlueprintToJson(Blueprint);
		Result.bSuccess = !Result.JsonText.IsEmpty();
		if (!Result.bSuccess)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("蓝图导出 JSON 失败。"));
		}
	}
	else
	{
		UEdGraph* Graph = Resolver.ResolveGraph(Request.Target, Result.Diagnostics);
		if (!Graph)
		{
			return Result;
		}

		Result.JsonText = FBlueprintToTextConverter::ConvertGraphToJson(Graph);
		Result.bSuccess = !Result.JsonText.IsEmpty();
		if (!Result.bSuccess)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("图表导出 JSON 失败。"));
		}
	}

	return Result;
}
