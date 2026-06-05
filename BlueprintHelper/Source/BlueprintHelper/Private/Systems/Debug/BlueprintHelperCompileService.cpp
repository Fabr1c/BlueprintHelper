// BlueprintHelper Service Layer 。编译服务实现

#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"

class FBlueprintHelperCompileServiceLocalUtils
{
public:
	static void AppendBlueprintGraphs(UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)
	{
		if (!Blueprint)
		{
			return;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				OutGraphs.AddUnique(Graph);
			}
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph)
			{
				OutGraphs.AddUnique(Graph);
			}
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (Graph)
			{
				OutGraphs.AddUnique(Graph);
			}
		}
	}

	static bool HasNodeCompileDiagnostic(
		const FBlueprintHelperDiagnosticSet& Diagnostics,
		const FString& GraphName,
		const FString& NodeGuid,
		const FString& Message)
	{
		return Diagnostics.Items.ContainsByPredicate(
			[&GraphName, &NodeGuid, &Message](const FBlueprintHelperDiagnosticItem& Item)
			{
				return Item.Code == TEXT("blueprint_compile_node_error")
					&& Item.GraphName == GraphName
					&& Item.NodeGuid == NodeGuid
					&& Item.Message == Message;
			});
	}

	static void AppendNodeCompilerDiagnostics(UBlueprint* Blueprint, FBlueprintHelperDiagnosticSet& Diagnostics)
	{
		TArray<UEdGraph*> Graphs;
		AppendBlueprintGraphs(Blueprint, Graphs);

		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			const FString GraphName = Graph->GetName();
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node || !Node->bHasCompilerMessage)
				{
					continue;
				}

				FString ErrorMessage = Node->ErrorMsg;
				ErrorMessage.TrimStartAndEndInline();
				if (ErrorMessage.IsEmpty())
				{
					continue;
				}

				const FString NodeGuid = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
				if (HasNodeCompileDiagnostic(Diagnostics, GraphName, NodeGuid, ErrorMessage))
				{
					continue;
				}

				FBlueprintHelperDiagnosticItem Item;
				Item.Severity = EBlueprintHelperDiagnosticSeverity::Error;
				Item.Code = TEXT("blueprint_compile_node_error");
				Item.Message = ErrorMessage;
				Item.GraphName = GraphName;
				Item.NodeId = NodeGuid;
				Item.NodeName = Node->GetName();
				Item.NodeGuid = NodeGuid;
				Item.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
				Item.NodeClass = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
				Item.ErrorType = TEXT("compiler");
				Item.CompileDiagnosticCorrelationKey = BlueprintHelperDiagnosticCorrelationKey(Item);
				Diagnostics.AddItem(MoveTemp(Item));
			}
		}
	}
};

FBlueprintHelperCompileService::FBlueprintHelperCompileService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperCompileResult FBlueprintHelperCompileService::Compile(const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperCompileResult Result;

	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Result.Diagnostics);
	if (!Blueprint)
	{
		return Result;
	}

	// 触发编译
	FCompilerResultsLog LogResults;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &LogResults);

	// 收集状态
	Result.BlueprintStatus = static_cast<int32>(Blueprint->Status);
	Result.bSuccess = (Blueprint->Status != BS_Error);

	// 从编译日志提取消息
	for (const TSharedRef<FTokenizedMessage>& Msg : LogResults.Messages)
	{
		EBlueprintHelperDiagnosticSeverity Sev;
		switch (Msg->GetSeverity())
		{
		case EMessageSeverity::Error:
			Sev = EBlueprintHelperDiagnosticSeverity::Error;
			break;
		case EMessageSeverity::Warning:
		case EMessageSeverity::PerformanceWarning:
			Sev = EBlueprintHelperDiagnosticSeverity::Warning;
			break;
		default:
			Sev = EBlueprintHelperDiagnosticSeverity::Info;
			break;
		}
		Result.Diagnostics.Add(Sev, Msg->ToText().ToString());
	}

	FBlueprintHelperCompileServiceLocalUtils::AppendNodeCompilerDiagnostics(Blueprint, Result.Diagnostics);

	return Result;
}

FBlueprintHelperCompileResult FBlueprintHelperCompileService::GetStatus(const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperCompileResult Result;

	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Result.Diagnostics);
	if (!Blueprint)
	{
		return Result;
	}

	Result.BlueprintStatus = static_cast<int32>(Blueprint->Status);
	Result.bSuccess = (Blueprint->Status != BS_Error);

	if (Blueprint->Status == BS_Error)
	{
		Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error,
			FString::Printf(TEXT("蓝图 %s 当前编译状态为 Error。"), *Blueprint->GetName()));
	}

	return Result;
}
