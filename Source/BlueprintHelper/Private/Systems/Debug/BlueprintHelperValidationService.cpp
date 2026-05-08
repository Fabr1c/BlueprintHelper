// BlueprintHelper Service Layer — JSON 结构校验服务实现

#include "Systems/Debug/BlueprintHelperValidationService.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FBlueprintHelperValidationResult FBlueprintHelperValidationService::Validate(const FString& JsonText) const
{
	FBlueprintHelperValidationResult Result;

	TSharedPtr<FJsonObject> Root;
	if (!ValidateJsonParseable(JsonText, Root, Result.Diagnostics))
	{
		return Result;
	}

	ValidateVersion(Root, Result.DetectedVersion, Result.Diagnostics);
	ValidateNodeIds(Root, Result.Diagnostics);
	ValidateLinkReferences(Root, Result.Diagnostics);

	Result.bValid = !Result.Diagnostics.HasErrors();
	return Result;
}

bool FBlueprintHelperValidationService::ValidateJsonParseable(const FString& JsonText, TSharedPtr<FJsonObject>& OutRoot, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("JSON 解析失败：输入不是合法 JSON。"));
		return false;
	}
	return true;
}

bool FBlueprintHelperValidationService::ValidateVersion(const TSharedPtr<FJsonObject>& Root, FString& OutVersion, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	if (!Root->HasField(TEXT("version")))
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Warning, TEXT("JSON 缺少 version 字段，按 1.0 处理。"));
		OutVersion = TEXT("1.0");
		return true;
	}

	if (!Root->TryGetStringField(TEXT("version"), OutVersion))
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
			TEXT("version 字段必须是字符串。"), TEXT(""), TEXT("invalid_field_type"), TEXT("$.version"));
		return false;
	}
	return true;
}

bool FBlueprintHelperValidationService::ValidateNodeIds(const TSharedPtr<FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	bool bHasAnyNodes = false;
	bool bHasDuplicates = false;

	auto CollectNodeIds = [&OutDiag, &bHasAnyNodes, &bHasDuplicates](
		const TArray<TSharedPtr<FJsonValue>>& NodesArray,
		const FString& GraphName,
		TSet<FString>& GraphNodeIds)
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (NodeVal->TryGetObject(NodeObj) && NodeObj && NodeObj->IsValid())
			{
				FString Id;
				if (!(*NodeObj)->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
				{
					continue;
				}

				bHasAnyNodes = true;
				if (GraphNodeIds.Contains(Id))
				{
					OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
						FString::Printf(TEXT("图表 '%s' 中节点 ID 重复：'%s'。"), *GraphName, *Id),
						TEXT(""), TEXT("duplicate_node_id"), GraphName);
					bHasDuplicates = true;
				}
				else
				{
					GraphNodeIds.Add(Id);
				}
			}
		}
	};

	auto CollectExistingRefIds = [&OutDiag, &bHasAnyNodes, &bHasDuplicates](
		const TArray<TSharedPtr<FJsonValue>>& ExistingRefsArray,
		const FString& GraphName,
		TSet<FString>& GraphNodeIds)
	{
		for (const TSharedPtr<FJsonValue>& RefVal : ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject>* RefObj = nullptr;
			if (!RefVal->TryGetObject(RefObj) || !RefObj || !RefObj->IsValid())
			{
				continue;
			}

			FString Id;
			if (!(*RefObj)->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
			{
				continue;
			}

			bHasAnyNodes = true;
			if (GraphNodeIds.Contains(Id))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("图表 '%s' 中节点 ID 重复：'%s'。"), *GraphName, *Id),
					TEXT(""), TEXT("duplicate_node_id"), GraphName);
				bHasDuplicates = true;
			}
			else
			{
				GraphNodeIds.Add(Id);
			}
		}
	};

	TSet<FString> RootGraphNodeIds;
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		CollectNodeIds(*NodesArray, TEXT("$"), RootGraphNodeIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray))
	{
		CollectExistingRefIds(*ExistingRefsArray, TEXT("$"), RootGraphNodeIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("graphs"), GraphsArray))
	{
		for (int32 GraphIndex = 0; GraphIndex < GraphsArray->Num(); ++GraphIndex)
		{
			const TSharedPtr<FJsonValue>& GraphVal = (*GraphsArray)[GraphIndex];
			const TSharedPtr<FJsonObject>* GraphObj = nullptr;
			if (GraphVal->TryGetObject(GraphObj) && GraphObj && GraphObj->IsValid())
			{
				FString GraphName;
				if (!(*GraphObj)->TryGetStringField(TEXT("name"), GraphName)
					&& !(*GraphObj)->TryGetStringField(TEXT("graph"), GraphName)
					&& !(*GraphObj)->TryGetStringField(TEXT("graph_name"), GraphName))
				{
					GraphName = FString::Printf(TEXT("Graph_%d"), GraphIndex);
				}

				TSet<FString> GraphNodeIds;
				const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("nodes"), GraphNodes))
				{
					CollectNodeIds(*GraphNodes, GraphName, GraphNodeIds);
				}

				const TArray<TSharedPtr<FJsonValue>>* GraphExistingRefs = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("existing_node_refs"), GraphExistingRefs))
				{
					CollectExistingRefIds(*GraphExistingRefs, GraphName, GraphNodeIds);
				}
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!bHasAnyNodes && !Root->TryGetArrayField(TEXT("blueprint_operations"), OpsArray))
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
			TEXT("JSON 中缺少 nodes 数组或 graphs 数组。"), TEXT(""), TEXT("missing_nodes"));
		return false;
	}

	return !bHasDuplicates;
}

bool FBlueprintHelperValidationService::ValidateLinkReferences(const TSharedPtr<FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	auto CollectIds = [](const TArray<TSharedPtr<FJsonValue>>& NodesArray, TSet<FString>& OutIds)
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (NodeVal->TryGetObject(NodeObj) && NodeObj && NodeObj->IsValid())
			{
				FString Id;
				if ((*NodeObj)->TryGetStringField(TEXT("id"), Id) && !Id.IsEmpty())
				{
					OutIds.Add(Id);
				}
			}
		}
	};

	auto CollectExistingRefIds = [](const TArray<TSharedPtr<FJsonValue>>& ExistingRefsArray, TSet<FString>& OutIds)
	{
		for (const TSharedPtr<FJsonValue>& RefVal : ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject>* RefObj = nullptr;
			if (RefVal->TryGetObject(RefObj) && RefObj && RefObj->IsValid())
			{
				FString Id;
				(*RefObj)->TryGetStringField(TEXT("id"), Id);
				if (!Id.IsEmpty())
				{
					OutIds.Add(Id);
				}
			}
		}
	};

	auto CheckLinks = [&OutDiag](const TArray<TSharedPtr<FJsonValue>>& LinksArray, const FString& GraphName, const TSet<FString>& GraphNodeIds)
	{
		bool bValid = true;
		for (const TSharedPtr<FJsonValue>& LinkVal : LinksArray)
		{
			const TSharedPtr<FJsonObject>* LinkObj = nullptr;
			if (!LinkVal->TryGetObject(LinkObj) || !LinkObj || !LinkObj->IsValid())
			{
				continue;
			}

			FString FromGraph;
			FString ToGraph;
			(*LinkObj)->TryGetStringField(TEXT("from_graph"), FromGraph);
			(*LinkObj)->TryGetStringField(TEXT("to_graph"), ToGraph);
			if ((!FromGraph.IsEmpty() && FromGraph != GraphName) || (!ToGraph.IsEmpty() && ToGraph != GraphName))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("跨 graph link 不支持：当前图表 '%s'，from_graph='%s'，to_graph='%s'。"),
						*GraphName, *FromGraph, *ToGraph),
					TEXT(""), TEXT("cross_graph_link_not_supported"), GraphName);
				bValid = false;
				continue;
			}

			FString FromId;
			FString ToId;
			(*LinkObj)->TryGetStringField(TEXT("from_id"), FromId);
			(*LinkObj)->TryGetStringField(TEXT("to_id"), ToId);

			if (!FromId.IsEmpty() && !GraphNodeIds.Contains(FromId))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("图表 '%s' 的连线引用了不存在的来源节点 ID：'%s'。"), *GraphName, *FromId),
					TEXT(""), TEXT("invalid_link_endpoint"), GraphName);
				bValid = false;
			}
			if (!ToId.IsEmpty() && !GraphNodeIds.Contains(ToId))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("图表 '%s' 的连线引用了不存在的目标节点 ID：'%s'。"), *GraphName, *ToId),
					TEXT(""), TEXT("invalid_link_endpoint"), GraphName);
				bValid = false;
			}
		}
		return bValid;
	};

	bool bValid = true;
	TSet<FString> RootGraphNodeIds;
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		CollectIds(*NodesArray, RootGraphNodeIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray))
	{
		CollectExistingRefIds(*ExistingRefsArray, RootGraphNodeIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (Root->TryGetArrayField(TEXT("links"), LinksArray))
	{
		bValid &= CheckLinks(*LinksArray, TEXT("$"), RootGraphNodeIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("graphs"), GraphsArray))
	{
		for (int32 GraphIndex = 0; GraphIndex < GraphsArray->Num(); ++GraphIndex)
		{
			const TSharedPtr<FJsonValue>& GraphVal = (*GraphsArray)[GraphIndex];
			const TSharedPtr<FJsonObject>* GraphObj = nullptr;
			if (GraphVal->TryGetObject(GraphObj) && GraphObj && GraphObj->IsValid())
			{
				FString GraphName;
				if (!(*GraphObj)->TryGetStringField(TEXT("name"), GraphName)
					&& !(*GraphObj)->TryGetStringField(TEXT("graph"), GraphName)
					&& !(*GraphObj)->TryGetStringField(TEXT("graph_name"), GraphName))
				{
					GraphName = FString::Printf(TEXT("Graph_%d"), GraphIndex);
				}

				TSet<FString> GraphNodeIds;
				const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("nodes"), GraphNodes))
				{
					CollectIds(*GraphNodes, GraphNodeIds);
				}

				const TArray<TSharedPtr<FJsonValue>>* GraphExistingRefs = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("existing_node_refs"), GraphExistingRefs))
				{
					CollectExistingRefIds(*GraphExistingRefs, GraphNodeIds);
				}

				const TArray<TSharedPtr<FJsonValue>>* GraphLinks = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("links"), GraphLinks))
				{
					bValid &= CheckLinks(*GraphLinks, GraphName, GraphNodeIds);
				}
			}
		}
	}

	return bValid;
}
