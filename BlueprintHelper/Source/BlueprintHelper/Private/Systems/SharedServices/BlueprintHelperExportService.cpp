// BlueprintHelper Service Layer — 导出服务实现

#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"

FBlueprintHelperExportService::FBlueprintHelperExportService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperExportResult FBlueprintHelperExportService::Export(const FBlueprintHelperExportRequest& Request) const
{
	FBlueprintHelperExportResult Result;
	switch (Request.Scope)
	{
	case EBlueprintHelperExportScope::FullBlueprint:
		Result.EffectiveScope = TEXT("blueprint");
		break;
	case EBlueprintHelperExportScope::Selection:
		Result.EffectiveScope = TEXT("selection");
		break;
	default:
		Result.EffectiveScope = TEXT("graph");
		break;
	}

	if (Request.Scope == EBlueprintHelperExportScope::FullBlueprint)
	{
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(Request.Target, Result.Diagnostics);
		if (!Blueprint)
		{
			return Result;
		}

		Result.JsonObject = FBlueprintToTextConverter::ExportBlueprintToJsonObject(Blueprint);
		Result.bSuccess = Result.JsonObject.IsValid() && Result.JsonObject->Values.Num() > 0;
		if (!Result.bSuccess)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("蓝图导出 JSON 失败。"));
		}
	}
	else
	{
		if (Request.Scope == EBlueprintHelperExportScope::Selection)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Warning,
				TEXT("selection scope 当前按目标图表导出。"), TEXT(""), TEXT("selection_scope_degraded"));
		}

		UEdGraph* Graph = Resolver.ResolveGraph(Request.Target, Result.Diagnostics);
		if (!Graph)
		{
			return Result;
		}

		Result.JsonObject = FBlueprintToTextConverter::ConvertGraphToJsonObject(Graph);
		Result.bSuccess = Result.JsonObject.IsValid() && Result.JsonObject->Values.Num() > 0;
		if (!Result.bSuccess)
		{
			Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("图表导出 JSON 失败。"));
		}
	}

	return Result;
}
