// BlueprintHelper Service Layer - Material LogicMd projection.

#include "Systems/ToolClusters/Material/BlueprintHelperMaterialLogicMdProjector.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString FBlueprintHelperMaterialLogicMdProjector::BuildMarkdown(
	const TSharedPtr<FJsonObject>& LogicJson) const
{
	FString Markdown;
	Markdown += TEXT("# Material Logic\n\n");
	if (!LogicJson.IsValid())
	{
		Markdown += TEXT("No material logic payload.\n");
		return Markdown;
	}

	const TSharedPtr<FJsonObject>* Asset = nullptr;
	if (LogicJson->TryGetObjectField(TEXT("asset"), Asset) && Asset && Asset->IsValid())
	{
		FString AssetPath;
		if ((*Asset)->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			Markdown += FString::Printf(TEXT("- Asset: `%s`\n"), *AssetPath);
		}
	}

	const TSharedPtr<FJsonObject>* Stats = nullptr;
	if (LogicJson->TryGetObjectField(TEXT("stats"), Stats) && Stats && Stats->IsValid())
	{
		Markdown += FString::Printf(
			TEXT("- Nodes: %.0f\n- Links: %.0f\n\n"),
			(*Stats)->GetNumberField(TEXT("nodes")),
			(*Stats)->GetNumberField(TEXT("links")));
	}

	AppendParameters(LogicJson, Markdown);
	Markdown += TEXT("## Expressions\n\n");
	const TSharedPtr<FJsonObject>* Logic = nullptr;
	if (LogicJson->TryGetObjectField(TEXT("logic"), Logic) && Logic && Logic->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if ((*Logic)->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Nodes)
			{
				const TSharedPtr<FJsonObject>* Node = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Node) || !Node || !Node->IsValid())
				{
					continue;
				}
				Markdown += FString::Printf(
					TEXT("- `%s` class `%s` ref `%s`\n"),
					*(*Node)->GetStringField(TEXT("name")),
					*(*Node)->GetStringField(TEXT("class")),
					*(*Node)->GetStringField(TEXT("node_ref")));
			}
		}
	}
	Markdown += TEXT("\n");
	AppendOutputs(LogicJson, Markdown);
	Markdown += TEXT("## Connections\n\n");
	if (Logic && Logic->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
		if ((*Logic)->TryGetArrayField(TEXT("links"), Links) && Links)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Links)
			{
				const TSharedPtr<FJsonObject>* Link = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Link) || !Link || !Link->IsValid())
				{
					continue;
				}
				Markdown += FString::Printf(
					TEXT("- `%s.%s` -> `%s.%s`\n"),
					*(*Link)->GetStringField(TEXT("from_node_ref")),
					*(*Link)->GetStringField(TEXT("from_pin")),
					*(*Link)->GetStringField(TEXT("to_node_ref")),
					*(*Link)->GetStringField(TEXT("to_pin")));
			}
		}
	}
	Markdown += TEXT("\n## Owned Anchors\n\n");
	if (Logic && Logic->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if ((*Logic)->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Nodes)
			{
				const TSharedPtr<FJsonObject>* Node = nullptr;
				if (!Value.IsValid() || !Value->TryGetObject(Node) || !Node || !Node->IsValid())
				{
					continue;
				}

				FString BlockId;
				if ((*Node)->TryGetStringField(TEXT("block_id"), BlockId) && !BlockId.IsEmpty())
				{
					Markdown += FString::Printf(
						TEXT("- `%s` block `%s` node `%s`\n"),
						*(*Node)->GetStringField(TEXT("node_ref")),
						*BlockId,
						*(*Node)->GetStringField(TEXT("node_key")));
				}
			}
		}
	}
	Markdown += TEXT("\n");
	return Markdown;
}

void FBlueprintHelperMaterialLogicMdProjector::AppendParameters(
	const TSharedPtr<FJsonObject>& LogicJson,
	FString& OutMarkdown) const
{
	const TSharedPtr<FJsonObject>* Material = nullptr;
	if (!LogicJson.IsValid() || !LogicJson->TryGetObjectField(TEXT("material"), Material) || !Material || !Material->IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
	if (!(*Material)->TryGetArrayField(TEXT("parameters"), Parameters) || !Parameters)
	{
		return;
	}

	OutMarkdown += TEXT("## Parameters\n\n");
	for (const TSharedPtr<FJsonValue>& Value : *Parameters)
	{
		const TSharedPtr<FJsonObject>* Parameter = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Parameter) || !Parameter || !Parameter->IsValid())
		{
			continue;
		}

		OutMarkdown += FString::Printf(
			TEXT("- `%s` (%s) ref `%s`\n"),
			*(*Parameter)->GetStringField(TEXT("name")),
			*(*Parameter)->GetStringField(TEXT("kind")),
			*(*Parameter)->GetStringField(TEXT("node_ref")));
	}
	OutMarkdown += TEXT("\n");
}

void FBlueprintHelperMaterialLogicMdProjector::AppendOutputs(
	const TSharedPtr<FJsonObject>& LogicJson,
	FString& OutMarkdown) const
{
	const TSharedPtr<FJsonObject>* Material = nullptr;
	if (!LogicJson.IsValid() || !LogicJson->TryGetObjectField(TEXT("material"), Material) || !Material || !Material->IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
	if (!(*Material)->TryGetArrayField(TEXT("outputs"), Outputs) || !Outputs)
	{
		return;
	}

	OutMarkdown += TEXT("## Outputs\n\n");
	for (const TSharedPtr<FJsonValue>& Value : *Outputs)
	{
		const TSharedPtr<FJsonObject>* Output = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Output) || !Output || !Output->IsValid())
		{
			continue;
		}

		OutMarkdown += FString::Printf(
			TEXT("- `%s` <- `%s.%s`\n"),
			*(*Output)->GetStringField(TEXT("property")),
			*(*Output)->GetStringField(TEXT("source_node_ref")),
			*(*Output)->GetStringField(TEXT("source_pin")));
	}
	OutMarkdown += TEXT("\n");
}
