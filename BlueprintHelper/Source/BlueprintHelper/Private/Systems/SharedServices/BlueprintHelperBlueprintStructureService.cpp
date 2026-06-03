// BlueprintHelper Service Layer — 蓝图结构查询与操作服务实现

#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperVariableReplicationService.h"
#include "Systems/SharedServices/Utils/BlueprintHelperBlueprintStructureUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperStructure, Log, All);

FBlueprintHelperBlueprintStructureService::FBlueprintHelperBlueprintStructureService(
	const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

// ─── 辅助 ───

class FBlueprintHelperBlueprintStructureVariableMetadataUtils
{
public:
	static FString GetTooltip(UBlueprint* Blueprint, const FName& VariableName)
	{
		return GetMetaDataValue(Blueprint, VariableName, FBlueprintMetadata::MD_Tooltip);
	}

	static bool IsExposeOnSpawn(UBlueprint* Blueprint, const FName& VariableName)
	{
		const FString Value = GetMetaDataValue(Blueprint, VariableName, FBlueprintMetadata::MD_ExposeOnSpawn);
		return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

private:
	static FString GetMetaDataValue(UBlueprint* Blueprint, const FName& VariableName, const FName& Key)
	{
		if (!Blueprint)
		{
			return FString();
		}

		FString Value;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, VariableName, nullptr, Key, Value);
		return Value;
	}
};

UBlueprint* FBlueprintHelperBlueprintStructureService::ResolveBP(
	const FBlueprintHelperGraphTarget& Target, FString& OutError) const
{
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag);
	if (!BP && Diag.Items.Num() > 0)
	{
		OutError = Diag.Items[0].Message;
	}
	return BP;
}

// ─── ListGraphs ───

FBlueprintHelperListGraphsResult FBlueprintHelperBlueprintStructureService::ListGraphs(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListGraphsResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	auto AddGraphInfo = [&Result](UEdGraph* Graph, const FString& Type)
	{
		if (!Graph) return;
		FBlueprintHelperGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = Type;
		Info.NodeCount = Graph->Nodes.Num();

		// 检查是否 Pure 函数
		if (Type == TEXT("Function"))
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					Info.bIsPure = (Entry->GetExtraFlags() & FUNC_BlueprintPure) != 0;
					break;
				}
			}
		}

		Result.Graphs.Add(Info);
	};

	// UbergraphPages（EventGraph 等）
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		AddGraphInfo(Graph, TEXT("EventGraph"));
	}

	// FunctionGraphs
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		AddGraphInfo(Graph, TEXT("Function"));
	}

	// MacroGraphs
	for (UEdGraph* Graph : BP->MacroGraphs)
	{
		AddGraphInfo(Graph, TEXT("Macro"));
	}

	Result.bSuccess = true;
	return Result;
}

// ─── ListVariables ───

FBlueprintHelperListVariablesResult FBlueprintHelperBlueprintStructureService::ListVariables(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListVariablesResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		// 跳过事件分发器（MC Delegate 类型）
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			continue;
		}

		FBlueprintHelperVariableInfo Info;
		Info.Name = Var.VarName.ToString();
		Info.TypeCategory = Var.VarType.PinCategory.ToString();

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			Info.SubCategoryObject = Var.VarType.PinSubCategoryObject->GetPathName();
		}

		if (Var.VarType.IsArray())
		{
			Info.ContainerType = TEXT("Array");
		}
		else if (Var.VarType.IsSet())
		{
			Info.ContainerType = TEXT("Set");
		}
		else if (Var.VarType.IsMap())
		{
			Info.ContainerType = TEXT("Map");
		}
		else
		{
			Info.ContainerType = TEXT("None");
		}

		Info.DefaultValue = Var.DefaultValue;
		Info.Category = Var.Category.ToString();
		Info.Tooltip = FBlueprintHelperBlueprintStructureVariableMetadataUtils::GetTooltip(BP, Var.VarName);
		Info.bIsEditable = !(Var.PropertyFlags & CPF_DisableEditOnInstance);
		Info.bExposeOnSpawn = FBlueprintHelperBlueprintStructureVariableMetadataUtils::IsExposeOnSpawn(BP, Var.VarName);
		Info.Replication = FBlueprintHelperVariableReplicationService::ReadMemberVariableFacts(*BP, Var);

		Result.Variables.Add(Info);
	}

	Result.bSuccess = true;
	return Result;
}

// ─── ListEventDispatchers ───

FBlueprintHelperListDispatchersResult FBlueprintHelperBlueprintStructureService::ListEventDispatchers(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListDispatchersResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarType.PinCategory != UEdGraphSchema_K2::PC_MCDelegate)
		{
			continue;
		}

		FBlueprintHelperEventDispatcherInfo Info;
		Info.Name = Var.VarName.ToString();

		// 查找委托签名图获取参数
		UEdGraph* SigGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(BP, Var.VarName);
		if (SigGraph)
		{
			for (UEdGraphNode* Node : SigGraph->Nodes)
			{
				UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (!Entry) continue;
				for (const TSharedPtr<FUserPinInfo>& Pin : Entry->UserDefinedPins)
				{
					if (Pin.IsValid())
					{
						Info.Params.Add(FString::Printf(TEXT("%s:%s"),
							*Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString()));
					}
				}
				break;
			}
		}

		Result.Dispatchers.Add(Info);
	}

	Result.bSuccess = true;
	return Result;
}

// ─── AddVariable ───

bool FBlueprintHelperBlueprintStructureService::AddVariable(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Variable")));
	const bool bOk = UBlueprintHelperBlueprintStructureUtils::AddMemberVariableDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── RemoveVariable ───

bool FBlueprintHelperBlueprintStructureService::RemoveVariable(
	const FBlueprintHelperGraphTarget& Target, const FString& VarName, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Variable")));
	const bool bOk = UBlueprintHelperBlueprintStructureUtils::RemoveMemberVariableDirect(BP, VarName, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── AddGraph ───

bool FBlueprintHelperBlueprintStructureService::AddGraph(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FString GraphType;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("graph_type"), GraphType);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Graph")));
	const bool bOk = GraphType.Equals(TEXT("Macro"), ESearchCase::IgnoreCase)
		? UBlueprintHelperBlueprintStructureUtils::AddMacroGraphDirect(BP, Params, OutError)
		: UBlueprintHelperBlueprintStructureUtils::AddFunctionGraphDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── RemoveGraph ───

bool FBlueprintHelperBlueprintStructureService::RemoveGraph(
	const FBlueprintHelperGraphTarget& Target, const FString& GraphName, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Graph")));
	const bool bOk = UBlueprintHelperBlueprintStructureUtils::RemoveGraphDirect(BP, GraphName, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── AddEventDispatcher ───

bool FBlueprintHelperBlueprintStructureService::AddEventDispatcher(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Event Dispatcher")));
	const bool bOk = UBlueprintHelperBlueprintStructureUtils::AddEventDispatcherDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── DeleteNodes ───

bool FBlueprintHelperBlueprintStructureService::DeleteNodes(
	const FBlueprintHelperGraphTarget& Target, const TArray<FString>& NodeIds,
	int32& OutDeletedCount, FString& OutError) const
{
	OutDeletedCount = 0;

	FBlueprintHelperDiagnosticSet Diag;
	UEdGraph* Graph = Resolver.ResolveGraph(Target, Diag);
	if (!Graph)
	{
		OutError = Diag.Items.Num() > 0 ? Diag.Items[0].Message : TEXT("未找到目标图表。");
		return false;
	}

	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!BP)
	{
		OutError = TEXT("未找到图表所属蓝图。");
		return false;
	}

	TMap<FGuid, UEdGraphNode*> GuidToNode;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		GuidToNode.Add(Node->NodeGuid, Node);
	}

	TArray<UEdGraphNode*> NodesToRemove;
	TSet<FGuid> SeenGuids;
	for (const FString& NodeId : NodeIds)
	{
		if (NodeId.StartsWith(TEXT("Node_")))
		{
			OutError = FString::Printf(TEXT("delete_nodes 不再接受不稳定 ID '%s'。请使用 node_guid。"), *NodeId);
			return false;
		}

		FGuid NodeGuid;
		if (!UBlueprintHelperBlueprintStructureUtils::ParseStableNodeGuid(NodeId, NodeGuid))
		{
			OutError = FString::Printf(TEXT("delete_nodes 只接受稳定 node_guid，收到: %s"), *NodeId);
			return false;
		}

		if (SeenGuids.Contains(NodeGuid))
		{
			OutError = FString::Printf(TEXT("delete_nodes 包含重复 node_guid: %s"), *NodeId);
			return false;
		}
		SeenGuids.Add(NodeGuid);

		UEdGraphNode** Found = GuidToNode.Find(NodeGuid);
		if (!Found || !*Found)
		{
			OutError = FString::Printf(TEXT("未找到 node_guid: %s"), *NodeId);
			return false;
		}

		if (Cast<UK2Node_FunctionEntry>(*Found) || Cast<UK2Node_FunctionResult>(*Found))
		{
			OutError = FString::Printf(TEXT("受保护节点不能删除: %s"), *NodeId);
			return false;
		}

		NodesToRemove.Add(*Found);
	}

	if (NodesToRemove.Num() != NodeIds.Num())
	{
		OutError = TEXT("delete_nodes 目标数量与解析数量不一致。");
		return false;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Delete Nodes")), BP);
	Mutation.Modify(Graph);

	for (UEdGraphNode* Node : NodesToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(BP, Node, true);
		++OutDeletedCount;
	}

	if (OutDeletedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}

	Mutation.Commit();
	return true;
}
