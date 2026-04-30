// BlueprintHelper Service Layer — 导入服务实现

#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperValidationService.h"
#include "Services/BlueprintHelperCompileService.h"
#include "TextToBlueprintGenerator.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FGraphNodeSnapshot = TMap<UEdGraph*, TSet<UEdGraphNode*>>;
using FPackageDirtySnapshot = TMap<UPackage*, bool>;

void AddGraphIfValid(TArray<UEdGraph*>& Graphs, UEdGraph* Graph)
{
	if (Graph)
	{
		Graphs.AddUnique(Graph);
	}
}

TArray<UEdGraph*> CollectBlueprintGraphs(UBlueprint* Blueprint)
{
	TArray<UEdGraph*> Graphs;
	if (!Blueprint)
	{
		return Graphs;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		AddGraphIfValid(Graphs, Graph);
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		AddGraphIfValid(Graphs, Graph);
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		AddGraphIfValid(Graphs, Graph);
	}
	return Graphs;
}

FGraphNodeSnapshot CaptureGraphNodeSnapshot(const TArray<UEdGraph*>& Graphs)
{
	FGraphNodeSnapshot Snapshot;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSet<UEdGraphNode*>& Nodes = Snapshot.FindOrAdd(Graph);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Nodes.Add(Node);
			}
		}
	}
	return Snapshot;
}

FPackageDirtySnapshot CapturePackageDirtySnapshot(const TArray<UEdGraph*>& Graphs)
{
	FPackageDirtySnapshot Snapshot;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetOutermost())
		{
			Snapshot.FindOrAdd(Graph->GetOutermost()) = Graph->GetOutermost()->IsDirty();
		}
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			if (Blueprint->GetOutermost())
			{
				Snapshot.FindOrAdd(Blueprint->GetOutermost()) = Blueprint->GetOutermost()->IsDirty();
			}
		}
	}
	return Snapshot;
}

void RestorePackageDirtySnapshot(const FPackageDirtySnapshot& Snapshot)
{
	for (const TPair<UPackage*, bool>& Pair : Snapshot)
	{
		if (Pair.Key)
		{
			Pair.Key->SetDirtyFlag(Pair.Value);
		}
	}
}

void RemoveNodesCreatedAfterSnapshot(const FGraphNodeSnapshot& Snapshot)
{
	for (const TPair<UEdGraph*, TSet<UEdGraphNode*>>& Pair : Snapshot)
	{
		UEdGraph* Graph = Pair.Key;
		if (!Graph)
		{
			continue;
		}

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		for (int32 NodeIndex = Graph->Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			UEdGraphNode* Node = Graph->Nodes[NodeIndex];
			if (!Node || Pair.Value.Contains(Node))
			{
				continue;
			}

			if (Blueprint)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
			else
			{
				Graph->RemoveNode(Node);
			}
		}

		Graph->NotifyGraphChanged();
	}
}

EBlueprintHelperDiagnosticSeverity ConvertGeneratorSeverity(const FString& Severity)
{
	if (Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDiagnosticSeverity::Error;
	}
	if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDiagnosticSeverity::Warning;
	}
	return EBlueprintHelperDiagnosticSeverity::Info;
}

void AddGeneratorDiagnosticsToResult(
	const TArray<FBlueprintGeneratorDiagnostic>& GeneratorDiagnostics,
	FBlueprintHelperImportResult& Result)
{
	for (const FBlueprintGeneratorDiagnostic& Diagnostic : GeneratorDiagnostics)
	{
		Result.Diagnostics.Add(
			ConvertGeneratorSeverity(Diagnostic.Severity),
			Diagnostic.Message,
			Diagnostic.NodeId,
			Diagnostic.Code,
			TEXT(""),
			Diagnostic.PinName,
			Diagnostic.NodeId);
	}
}
}

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
	Result.Status = TEXT("failed");

	// 1. 校验
	FBlueprintHelperValidationResult ValResult = Validator.Validate(Request.JsonText);
	if (!ValResult.bValid)
	{
		Result.Diagnostics = MoveTemp(ValResult.Diagnostics);
		return Result;
	}

	// 2. 根据 JSON 结构选择单图/多图路径
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	FBlueprintGenerateResult GenResult;
	const bool bNeedsMultiGraph = NeedsMultiGraphPath(Request.JsonText);
	FGraphNodeSnapshot NodeSnapshot;
	FPackageDirtySnapshot DirtySnapshot;

	// 3. 事务包裹（整体 Undo 支持）
	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Import JSON")));

	if (bNeedsMultiGraph)
	{
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(Request.Target, Result.Diagnostics);
		if (!Blueprint)
		{
			Transaction.Cancel();
			return Result;
		}
		TArray<UEdGraph*> SnapshotGraphs = CollectBlueprintGraphs(Blueprint);
		NodeSnapshot = CaptureGraphNodeSnapshot(SnapshotGraphs);
		DirtySnapshot = CapturePackageDirtySnapshot(SnapshotGraphs);
		GenResult = TextToBlueprintGenerator::GenerateMultiGraphFromJson(Blueprint, Request.JsonText, Unresolved);
	}
	else
	{
		UEdGraph* Graph = Resolver.ResolveGraph(Request.Target, Result.Diagnostics);
		if (!Graph)
		{
			Transaction.Cancel();
			return Result;
		}
		TArray<UEdGraph*> SnapshotGraphs;
		SnapshotGraphs.Add(Graph);
		NodeSnapshot = CaptureGraphNodeSnapshot(SnapshotGraphs);
		DirtySnapshot = CapturePackageDirtySnapshot(SnapshotGraphs);
		GenResult = TextToBlueprintGenerator::GenerateBlueprintFromJson(Graph, Request.JsonText, Unresolved);
	}

	// 4. 转换结果
	Result.bSuccess = GenResult.bSucceed;
	Result.GeneratedNodeCount = GenResult.GeneratedNodeCount;
	Result.UnresolvedNodeCount = GenResult.UnresolvedNodeCount;
	Result.LinksConnected = GenResult.CreatedConnectionCount;
	AddGeneratorDiagnosticsToResult(GenResult.DefaultValueDiagnostics, Result);
	AddGeneratorDiagnosticsToResult(GenResult.PinTypeDiagnostics, Result);
	AddGeneratorDiagnosticsToResult(GenResult.ConnectionDiagnostics, Result);

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
			FString::Printf(TEXT("%d 个节点未匹配。"), Result.UnresolvedNodeCount),
			TEXT(""), TEXT("unresolved_nodes"));
	}

	const bool bHasLinkFailures = GenResult.CreatedConnectionCount < GenResult.RequestedConnectionCount;
	if (bHasLinkFailures)
	{
		Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error,
			FString::Printf(TEXT("请求建立 %d 条连线，实际建立 %d 条。"),
				GenResult.RequestedConnectionCount, GenResult.CreatedConnectionCount),
			TEXT(""), TEXT("link_connection_failed"));
	}

	if (!GenResult.bSucceed)
	{
		Result.Status = TEXT("no_op");
	}
	else if (Result.Diagnostics.HasErrors() || Result.UnresolvedNodeCount > 0)
	{
		Result.Status = TEXT("partial_success");
	}
	else
	{
		Result.Status = TEXT("full_success");
	}

	if ((Result.Status == TEXT("partial_success") || Result.Status == TEXT("no_op")) && Request.bStrict && !Request.bAllowPartial)
	{
		RemoveNodesCreatedAfterSnapshot(NodeSnapshot);
		RestorePackageDirtySnapshot(DirtySnapshot);
		Transaction.Cancel();
		Result.bSuccess = false;
		Result.bRolledBack = true;
		Result.Status = TEXT("failed");
		Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error,
			TEXT("strict 导入检测到 partial/no-op，已回滚本次事务。"),
			TEXT(""), TEXT("strict_import_rolled_back"));
		return Result;
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
