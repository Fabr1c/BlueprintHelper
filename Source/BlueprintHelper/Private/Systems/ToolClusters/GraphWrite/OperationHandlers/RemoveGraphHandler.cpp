#include "Systems/ToolClusters/GraphWrite/OperationHandlers/RemoveGraphHandler.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "Dom/JsonObject.h"

bool FRemoveGraphHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("remove_graph"), ESearchCase::IgnoreCase);
}

bool FRemoveGraphHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_graph 失败：蓝图无效。");
		return false;
	}

	FString GraphName;
	if (!OpPayload->TryGetStringField(TEXT("name"), GraphName) || GraphName.IsEmpty())
	{
		OutError = TEXT("remove_graph 失败：缺。name 字段。");
		return false;
	}

	// 禁止删除 EventGraph
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("remove_graph 失败：不允许删除 EventGraph。");
		return false;
	}

	// 在函数图中查找
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	// 在宏图中查找
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	// 图表不存在，视为幂等成功
	return true;
}
