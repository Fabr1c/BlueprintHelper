// BlueprintHelper Service Layer — JSON 结构校验服务实现

#include "Services/BlueprintHelperValidationService.h"
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

	OutVersion = Root->GetStringField(TEXT("version"));
	return true;
}

bool FBlueprintHelperValidationService::ValidateNodeIds(const TSharedPtr<FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	TSet<FString> AllNodeIds;
	bool bHasDuplicates = false;
	bool bHasExistingRefs = false;

	auto CollectNodeIds = [&](const TArray<TSharedPtr<FJsonValue>>& NodesArray)
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (NodeVal->TryGetObject(NodeObj) && (*NodeObj)->HasField(TEXT("id")))
			{
				const FString Id = (*NodeObj)->GetStringField(TEXT("id"));
				if (AllNodeIds.Contains(Id))
				{
					OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
						FString::Printf(TEXT("节点 ID 重复：'%s'。"), *Id));
					bHasDuplicates = true;
				}
				else
				{
					AllNodeIds.Add(Id);
				}
			}
		}
	};

	auto CollectExistingRefIds = [&](const TArray<TSharedPtr<FJsonValue>>& ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefVal : ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject>* RefObj = nullptr;
			if (!RefVal->TryGetObject(RefObj) || !(*RefObj)->HasField(TEXT("id")))
			{
				continue;
			}

			const FString Id = (*RefObj)->GetStringField(TEXT("id"));
			if (Id.IsEmpty())
			{
				continue;
			}

			bHasExistingRefs = true;
			if (AllNodeIds.Contains(Id))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("节点 ID 重复：'%s'。"), *Id));
				bHasDuplicates = true;
			}
			else
			{
				AllNodeIds.Add(Id);
			}
		}
	};

	// 顶层 nodes
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		CollectNodeIds(*NodesArray);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray))
	{
		CollectExistingRefIds(*ExistingRefsArray);
	}

	// graphs 中的 nodes
	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("graphs"), GraphsArray))
	{
		for (const TSharedPtr<FJsonValue>& GraphVal : *GraphsArray)
		{
			const TSharedPtr<FJsonObject>* GraphObj = nullptr;
			if (GraphVal->TryGetObject(GraphObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("nodes"), GraphNodes))
				{
					CollectNodeIds(*GraphNodes);
				}

				const TArray<TSharedPtr<FJsonValue>>* GraphExistingRefs = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("existing_node_refs"), GraphExistingRefs))
				{
					CollectExistingRefIds(*GraphExistingRefs);
				}
			}
		}
	}

	// 如果既无节点也无 blueprint_operations，报错
	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (AllNodeIds.Num() == 0 && !bHasExistingRefs && !Root->TryGetArrayField(TEXT("blueprint_operations"), OpsArray))
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("JSON 中缺少 nodes 数组或 graphs 数组。"));
		return false;
	}

	return !bHasDuplicates;
}

bool FBlueprintHelperValidationService::ValidateLinkReferences(const TSharedPtr<FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	// 收集所有节点 ID
	TSet<FString> AllNodeIds;

	auto CollectIds = [&](const TArray<TSharedPtr<FJsonValue>>& NodesArray)
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : NodesArray)
		{
			const TSharedPtr<FJsonObject>* NodeObj = nullptr;
			if (NodeVal->TryGetObject(NodeObj) && (*NodeObj)->HasField(TEXT("id")))
			{
				AllNodeIds.Add((*NodeObj)->GetStringField(TEXT("id")));
			}
		}
	};

	auto CollectExistingRefIds = [&](const TArray<TSharedPtr<FJsonValue>>& ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefVal : ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject>* RefObj = nullptr;
			if (RefVal->TryGetObject(RefObj) && (*RefObj)->HasField(TEXT("id")))
			{
				const FString Id = (*RefObj)->GetStringField(TEXT("id"));
				if (!Id.IsEmpty())
				{
					AllNodeIds.Add(Id);
				}
			}
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		CollectIds(*NodesArray);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray))
	{
		CollectExistingRefIds(*ExistingRefsArray);
	}

	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("graphs"), GraphsArray))
	{
		for (const TSharedPtr<FJsonValue>& GraphVal : *GraphsArray)
		{
			const TSharedPtr<FJsonObject>* GraphObj = nullptr;
			if (GraphVal->TryGetObject(GraphObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("nodes"), GraphNodes))
				{
					CollectIds(*GraphNodes);
				}

				const TArray<TSharedPtr<FJsonValue>>* GraphExistingRefs = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("existing_node_refs"), GraphExistingRefs))
				{
					CollectExistingRefIds(*GraphExistingRefs);
				}
			}
		}
	}

	if (AllNodeIds.Num() == 0)
	{
		return true;
	}

	// 检查连线引用
	bool bValid = true;
	auto CheckLinks = [&](const TArray<TSharedPtr<FJsonValue>>& LinksArray)
	{
		for (const TSharedPtr<FJsonValue>& LinkVal : LinksArray)
		{
			const TSharedPtr<FJsonObject>* LinkObj = nullptr;
			if (!LinkVal->TryGetObject(LinkObj))
			{
				continue;
			}

			const FString FromId = (*LinkObj)->GetStringField(TEXT("from_id"));
			const FString ToId = (*LinkObj)->GetStringField(TEXT("to_id"));

			if (!AllNodeIds.Contains(FromId))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("连线引用了不存在的来源节点 ID：'%s'。"), *FromId));
				bValid = false;
			}
			if (!AllNodeIds.Contains(ToId))
			{
				OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
					FString::Printf(TEXT("连线引用了不存在的目标节点 ID：'%s'。"), *ToId));
				bValid = false;
			}
		}
	};

	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (Root->TryGetArrayField(TEXT("links"), LinksArray))
	{
		CheckLinks(*LinksArray);
	}

	if (GraphsArray)
	{
		for (const TSharedPtr<FJsonValue>& GraphVal : *GraphsArray)
		{
			const TSharedPtr<FJsonObject>* GraphObj = nullptr;
			if (GraphVal->TryGetObject(GraphObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* GraphLinks = nullptr;
				if ((*GraphObj)->TryGetArrayField(TEXT("links"), GraphLinks))
				{
					CheckLinks(*GraphLinks);
				}
			}
		}
	}

	return bValid;
}
