// BlueprintHelper Review baseline semantic snapshot service.

#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"

#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "Systems/Review/Utils/BlueprintHelperReviewBaselineSnapshotServiceUtils.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewUtils.h"


TArray<FString> FBlueprintHelperReviewBaselineSnapshotService::CaptureSemanticBaselineSnapshots(
	const FString& ArchiveSessionId,
	const TArray<FString>& AssetPaths,
	TArray<FString>* OutWarnings) const
{
	TArray<FString> SnapshotRefs;
	if (ArchiveSessionId.IsEmpty())
	{
		return SnapshotRefs;
	}

	for (const FString& AssetPath : AssetPaths)
	{
		if (AssetPath.IsEmpty())
		{
			continue;
		}

		UObject* Asset = LoadAssetForSnapshot(AssetPath);
		if (!Asset)
		{
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(
					TEXT("Review baseline semantic snapshot skipped because asset could not be loaded: %s"),
					*AssetPath));
			}
			continue;
		}

		const FString SnapshotKey = MakeSnapshotKey(AssetPath);
		const TSharedRef<FJsonObject> Snapshot = BuildAssetSnapshot(AssetPath, Asset);
		FString WriteError;
		if (!WriteSnapshotJson(ArchiveSessionId, SnapshotKey, Snapshot, WriteError))
		{
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(
					TEXT("Review baseline semantic snapshot write failed for %s: %s"),
					*AssetPath,
					*WriteError));
			}
			continue;
		}

		SnapshotRefs.Add(MakeSnapshotRef(ArchiveSessionId, SnapshotKey));
	}

	return SnapshotRefs;
}

bool FBlueprintHelperReviewBaselineSnapshotService::CaptureTargetSnapshot(
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutSnapshotJson,
	FString& OutSnapshotHash,
	FString& OutError) const
{
	OutSnapshotJson.Reset();
	OutSnapshotHash.Reset();
	OutError.Reset();

	if (Target.AssetPath.IsEmpty())
	{
		OutError = TEXT("missing_asset_path");
		return false;
	}

	UObject* Asset = LoadAssetForSnapshot(Target.AssetPath);
	const TSharedRef<FJsonObject> Snapshot = BuildTargetSnapshot(Target, Asset, Asset != nullptr);
	OutSnapshotJson = FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObject(Snapshot);
	OutSnapshotHash = ComputeSemanticSnapshotHash(Snapshot);
	return true;
}

bool FBlueprintHelperReviewBaselineSnapshotService::TryLoadBaselineTargetSnapshot(
	const FString& ArchiveSessionId,
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutSnapshotJson,
	FString& OutSnapshotHash,
	FString& OutError) const
{
	OutSnapshotJson.Reset();
	OutSnapshotHash.Reset();
	OutError.Reset();

	if (ArchiveSessionId.IsEmpty())
	{
		OutError = TEXT("missing_archive_session_id");
		return false;
	}
	if (Target.AssetPath.IsEmpty())
	{
		OutError = TEXT("missing_asset_path");
		return false;
	}

	const FString SnapshotPath = FPaths::Combine(
		MakeSnapshotDirectory(ArchiveSessionId, MakeSnapshotKey(Target.AssetPath)),
		TEXT("baseline.semantic.json"));
	FString SnapshotText;
	if (!FFileHelper::LoadFileToString(SnapshotText, *SnapshotPath))
	{
		OutError = FString::Printf(TEXT("baseline_semantic_snapshot_not_found:%s"), *SnapshotPath);
		return false;
	}

	TSharedPtr<FJsonObject> AssetSnapshot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SnapshotText);
	if (!FJsonSerializer::Deserialize(Reader, AssetSnapshot) || !AssetSnapshot.IsValid())
	{
		OutError = FString::Printf(TEXT("baseline_semantic_snapshot_parse_failed:%s"), *SnapshotPath);
		return false;
	}

	const TSharedRef<FJsonObject> TargetSnapshot =
		BuildTargetSnapshotFromBaselineAssetSnapshot(Target, AssetSnapshot);
	OutSnapshotJson = FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObject(TargetSnapshot);
	OutSnapshotHash = ComputeSemanticSnapshotHash(TargetSnapshot);
	return true;
}

void FBlueprintHelperReviewBaselineSnapshotService::MakeMissingTargetSnapshot(
	const FBlueprintHelperReviewAtomicTarget& Target,
	bool bAssetExists,
	FString& OutSnapshotJson,
	FString& OutSnapshotHash)
{
	TSharedRef<FJsonObject> Snapshot = BuildTargetSnapshotHeader(Target, bAssetExists, FString());
	Snapshot->SetBoolField(TEXT("exists"), false);
	OutSnapshotJson = FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObject(Snapshot);
	OutSnapshotHash = ComputeSemanticSnapshotHash(Snapshot);
}

FString FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(
	const FString& SnapshotJson)
{
	TSharedPtr<FJsonObject> Snapshot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SnapshotJson);
	if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot.IsValid())
	{
		return FString::Printf(TEXT("crc32_%08x"), FCrc::StrCrc32(*SnapshotJson));
	}
	return ComputeSemanticSnapshotHash(Snapshot.ToSharedRef());
}

FString FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(
	const TSharedRef<FJsonObject>& Snapshot)
{
	const TSharedPtr<FJsonObject> HashSnapshot = UBlueprintHelperReviewUtils::CloneReviewSnapshotObjectForHash(Snapshot);
	const FString CanonicalSnapshot =
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObjectCanonical(HashSnapshot.ToSharedRef());
	return FString::Printf(TEXT("crc32_%08x"), FCrc::StrCrc32(*CanonicalSnapshot));
}

FString FBlueprintHelperReviewBaselineSnapshotService::MakeSnapshotKey(const FString& AssetPath)
{
	FString Sanitized = AssetPath;
	Sanitized.ReplaceInline(TEXT("/"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("\\"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT("."), TEXT("_"));
	Sanitized.ReplaceInline(TEXT(":"), TEXT("_"));
	Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
	Sanitized.TrimStartAndEndInline();
	if (Sanitized.IsEmpty())
	{
		Sanitized = TEXT("asset");
	}

	const uint32 Hash = FCrc::StrCrc32(*AssetPath);
	return FString::Printf(TEXT("%s_%08x"), *Sanitized, Hash);
}

FString FBlueprintHelperReviewBaselineSnapshotService::MakeSnapshotDirectory(
	const FString& ArchiveSessionId,
	const FString& SnapshotKey)
{
	return FPaths::Combine(
		FBlueprintHelperReviewConfigResolver::Load().Artifact.SnapshotRoot,
		ArchiveSessionId,
		SnapshotKey);
}

FString FBlueprintHelperReviewBaselineSnapshotService::MakeSnapshotRef(
	const FString& ArchiveSessionId,
	const FString& SnapshotKey)
{
	return FString::Printf(
		TEXT("review://archive/%s/baseline/%s/baseline.semantic.json"),
		*ArchiveSessionId,
		*SnapshotKey);
}

UObject* FBlueprintHelperReviewBaselineSnapshotService::LoadAssetForSnapshot(const FString& AssetPath)
{
	UObject* LoadedAsset = FSoftObjectPath(AssetPath).TryLoad();
	if (LoadedAsset)
	{
		return LoadedAsset;
	}

	if (!AssetPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(AssetPath))
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			*AssetPath,
			*FPackageName::GetShortName(AssetPath));
		LoadedAsset = FSoftObjectPath(ObjectPath).TryLoad();
	}
	return LoadedAsset;
}

bool FBlueprintHelperReviewBaselineSnapshotService::WriteSnapshotJson(
	const FString& ArchiveSessionId,
	const FString& SnapshotKey,
	const TSharedRef<FJsonObject>& Snapshot,
	FString& OutError)
{
	const FString SnapshotDir = MakeSnapshotDirectory(ArchiveSessionId, SnapshotKey);
	if (!IFileManager::Get().MakeDirectory(*SnapshotDir, true))
	{
		OutError = FString::Printf(TEXT("Could not create snapshot directory: %s"), *SnapshotDir);
		return false;
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	if (!FJsonSerializer::Serialize(Snapshot, Writer))
	{
		OutError = TEXT("Could not serialize semantic baseline snapshot JSON.");
		return false;
	}

	const FString SnapshotPath = FPaths::Combine(SnapshotDir, TEXT("baseline.semantic.json"));
	if (!FFileHelper::SaveStringToFile(Serialized, *SnapshotPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not write semantic baseline snapshot: %s"), *SnapshotPath);
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildAssetSnapshot(
	const FString& AssetPath,
	UObject* Asset)
{
	TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetStringField(TEXT("schema"), FBlueprintHelperReviewConfigResolver::Load().MakeBaselineSemanticSnapshotSchema());
	Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
	Snapshot->SetStringField(TEXT("object_name"), GetNameSafe(Asset));
	Snapshot->SetStringField(TEXT("object_path"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(Asset));
	Snapshot->SetStringField(TEXT("object_class"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(Asset));
	Snapshot->SetStringField(TEXT("captured_at"), FDateTime::UtcNow().ToIso8601());

	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		Snapshot->SetObjectField(TEXT("blueprint"), BuildBlueprintSnapshot(Blueprint));
	}

	if (const UDataTable* DataTable = Cast<UDataTable>(Asset))
	{
		Snapshot->SetObjectField(TEXT("data_table"), BuildDataTableSnapshot(DataTable));
	}

	Snapshot->SetObjectField(TEXT("object_properties"), BuildGenericObjectSnapshot(Asset));
	return Snapshot;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildBlueprintSnapshot(const UBlueprint* Blueprint)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("parent_class"), Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
	Json->SetStringField(TEXT("generated_class"), Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());

	TArray<TSharedPtr<FJsonValue>> Variables;
	if (Blueprint)
	{
		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			TSharedRef<FJsonObject> VariableJson = MakeShared<FJsonObject>();
			VariableJson->SetStringField(TEXT("name"), Variable.VarName.ToString());
			VariableJson->SetStringField(TEXT("category"), Variable.Category.ToString());
			VariableJson->SetStringField(TEXT("pin_category"), Variable.VarType.PinCategory.ToString());
			VariableJson->SetStringField(TEXT("pin_sub_category"), Variable.VarType.PinSubCategory.ToString());
			VariableJson->SetStringField(TEXT("pin_sub_category_object"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
			VariableJson->SetStringField(TEXT("default_value"), Variable.DefaultValue);
			Variables.Add(MakeShared<FJsonValueObject>(VariableJson));
		}
	}
	Json->SetArrayField(TEXT("variables"), Variables);

	TArray<TSharedPtr<FJsonValue>> Components;
	if (Blueprint && Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node)
			{
				continue;
			}
			TSharedRef<FJsonObject> ComponentJson = MakeShared<FJsonObject>();
			ComponentJson->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			const UActorComponent* ComponentTemplate = Node->ComponentTemplate;
			ComponentJson->SetStringField(
				TEXT("component_class"),
				ComponentTemplate && ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetPathName() : FString());
			Components.Add(MakeShared<FJsonValueObject>(ComponentJson));
		}
	}
	Json->SetArrayField(TEXT("components"), Components);

	TArray<UEdGraph*> Graphs;
	if (Blueprint)
	{
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->UbergraphPages);
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->FunctionGraphs);
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->MacroGraphs);
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->DelegateSignatureGraphs);
	}

	TArray<TSharedPtr<FJsonValue>> GraphJsonValues;
	for (const UEdGraph* Graph : Graphs)
	{
		GraphJsonValues.Add(MakeShared<FJsonValueObject>(BuildGraphSnapshot(Graph, Graph ? Graph->GetName() : FString())));
	}
	Json->SetArrayField(TEXT("graphs"), GraphJsonValues);

	if (UWidgetBlueprint* WidgetBlueprint = const_cast<UWidgetBlueprint*>(Cast<UWidgetBlueprint>(Blueprint)))
	{
		if (WidgetBlueprint->WidgetTree)
		{
			Json->SetObjectField(TEXT("widget_tree"), BuildWidgetTreeSnapshot(WidgetBlueprint->WidgetTree));
		}
	}

	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildTargetSnapshotHeader(
	const FBlueprintHelperReviewAtomicTarget& Target,
	bool bAssetExists,
	const FString& AssetClass)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	const FString TargetName = FBlueprintHelperReviewBaselineSnapshotServiceUtils::ExtractTargetName(Target);
	Json->SetStringField(TEXT("schema"), FBlueprintHelperReviewConfigResolver::Load().DebugBundle.SchemaSnapshot);
	Json->SetStringField(TEXT("asset_path"), Target.AssetPath);
	Json->SetStringField(TEXT("target_kind"), Target.TargetKind);
	Json->SetStringField(TEXT("target_key"), Target.TargetKey);
	Json->SetStringField(TEXT("target_name"), TargetName);
	Json->SetBoolField(TEXT("asset_exists"), bAssetExists);
	Json->SetStringField(TEXT("asset_class"), AssetClass);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildTargetSnapshot(
	const FBlueprintHelperReviewAtomicTarget& Target,
	UObject* Asset,
	bool bAssetExists)
{
	TSharedRef<FJsonObject> Json = BuildTargetSnapshotHeader(
		Target,
		bAssetExists,
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(Asset));
	const FString TargetName = FBlueprintHelperReviewBaselineSnapshotServiceUtils::ExtractTargetName(Target);

	if (!bAssetExists)
	{
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
		FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::GraphNode)
		{
			Json->SetStringField(TEXT("surface"), TEXT("graph"));
			Json->SetStringField(TEXT("graph_name"), Target.GraphName);
			const UEdGraph* Graph = UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, Target.GraphName);
			if (!Graph)
			{
				Json->SetBoolField(TEXT("exists"), false);
				Json->SetStringField(TEXT("resolve_error_code"), TEXT("graph_not_found"));
				return Json;
			}

			const FString NodeName = UBlueprintHelperReviewUtils::ExtractReviewSnapshotAnchorName(Target.TargetKey, TEXT("node"));
			const UEdGraphNode* Node = UBlueprintHelperReviewUtils::FindReviewSnapshotNodeByGuid(Graph, Target.NodeGuid);
			if (!Node)
			{
				Node = UBlueprintHelperReviewUtils::FindReviewSnapshotNodeByName(Graph, NodeName);
			}
			if (!Node || UBlueprintHelperReviewUtils::IsReviewSnapshotIgnoredGraphNode(Node))
			{
				Json->SetBoolField(TEXT("exists"), false);
				Json->SetStringField(TEXT("resolve_error_code"), Node ? TEXT("node_ignored") : TEXT("node_not_found"));
				return Json;
			}

			Json->SetBoolField(TEXT("exists"), true);
			Json->SetObjectField(TEXT("node"), BuildNodeSnapshot(Node));
			TArray<const UEdGraphNode*> RestoreNodes;
			RestoreNodes.Add(Node);
			Json->SetStringField(TEXT("restore_text"), UBlueprintHelperReviewUtils::BuildReviewSnapshotRestoreText(RestoreNodes));
			return Json;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::GraphBlock)
		{
			Json->SetStringField(TEXT("surface"), TEXT("graph"));
			Json->SetStringField(TEXT("graph_name"), Target.GraphName);
			const FString BlockId = UBlueprintHelperReviewUtils::ExtractReviewSnapshotAnchorName(Target.TargetKey, TEXT("block"));
			Json->SetStringField(TEXT("block_id"), BlockId);
			const UEdGraph* Graph = UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, Target.GraphName);
			if (!Graph)
			{
				Json->SetBoolField(TEXT("exists"), false);
				Json->SetStringField(TEXT("resolve_error_code"), TEXT("graph_not_found"));
				return Json;
			}

			TArray<const UEdGraphNode*> BlockNodes;
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node || UBlueprintHelperReviewUtils::IsReviewSnapshotIgnoredGraphNode(Node))
				{
					continue;
				}
				if (UBlueprintHelperReviewUtils::GetReviewSnapshotNodeBlockId(Node) == BlockId)
				{
					BlockNodes.Add(Node);
				}
			}
			BlockNodes.Sort([](const UEdGraphNode& Left, const UEdGraphNode& Right)
			{
				return UBlueprintHelperReviewUtils::MakeReviewSnapshotNodeSortKey(&Left) < UBlueprintHelperReviewUtils::MakeReviewSnapshotNodeSortKey(&Right);
			});

			TArray<TSharedPtr<FJsonValue>> Nodes;
			for (const UEdGraphNode* Node : BlockNodes)
			{
				Nodes.Add(MakeShared<FJsonValueObject>(BuildNodeSnapshot(Node)));
			}
			Json->SetBoolField(TEXT("exists"), BlockNodes.Num() > 0);
			Json->SetArrayField(TEXT("nodes"), Nodes);
			if (BlockNodes.Num() > 0)
			{
				Json->SetStringField(TEXT("restore_text"), UBlueprintHelperReviewUtils::BuildReviewSnapshotRestoreText(BlockNodes));
			}
			if (BlockNodes.Num() == 0)
			{
				Json->SetStringField(TEXT("resolve_error_code"), TEXT("block_not_found"));
			}
			return Json;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable)
		{
			Json->SetStringField(TEXT("surface"), TEXT("my_blueprint"));
			for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
			{
				if (Variable.VarName.ToString() != TargetName)
				{
					continue;
				}

				Json->SetBoolField(TEXT("exists"), true);
				Json->SetStringField(TEXT("name"), Variable.VarName.ToString());
				Json->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
	Json->SetStringField(TEXT("category"), UBlueprintHelperReviewUtils::BlueprintHelperReviewMakeStableTextKeyForSnapshot(Variable.Category));
				Json->SetStringField(TEXT("pin_category"), Variable.VarType.PinCategory.ToString());
				Json->SetStringField(TEXT("pin_sub_category"), Variable.VarType.PinSubCategory.ToString());
				Json->SetStringField(TEXT("pin_sub_category_object"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
				Json->SetStringField(TEXT("default_value"), Variable.DefaultValue);
				return Json;
			}

			Json->SetBoolField(TEXT("exists"), false);
			return Json;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
		{
			Json->SetStringField(TEXT("surface"), TEXT("components"));
			if (Blueprint->SimpleConstructionScript)
			{
				for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
				{
					if (!Node || Node->GetVariableName().ToString() != TargetName)
					{
						continue;
					}

					Json->SetBoolField(TEXT("exists"), true);
					Json->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
					Json->SetStringField(TEXT("parent_component"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::FindScsParentComponentName(Blueprint, Node));
					const UActorComponent* ComponentTemplate = Node->ComponentTemplate;
					Json->SetStringField(
						TEXT("component_class"),
						ComponentTemplate && ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetPathName() : FString());
					if (ComponentTemplate)
					{
						Json->SetObjectField(TEXT("properties"), BuildGenericObjectSnapshot(const_cast<UActorComponent*>(ComponentTemplate)));
					}
					return Json;
				}
			}

			Json->SetBoolField(TEXT("exists"), false);
			return Json;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Signature)
		{
			Json->SetStringField(TEXT("surface"), TEXT("my_blueprint"));
			Json->SetBoolField(TEXT("exists"), false);
			TArray<UEdGraph*> Graphs;
			FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->UbergraphPages);
			FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->FunctionGraphs);
			FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->MacroGraphs);
			FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(Graphs, Blueprint->DelegateSignatureGraphs);
			for (const UEdGraph* Graph : Graphs)
			{
				if (Graph && Graph->GetName() == TargetName)
				{
					Json->SetBoolField(TEXT("exists"), true);
					Json->SetObjectField(TEXT("graph"), BuildGraphSnapshot(Graph, Graph->GetName()));
					return Json;
				}
			}
			return Json;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget
			|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
		{
			Json->SetStringField(TEXT("surface"), TEXT("umg_widget_tree"));
			UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
			if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
			{
				Json->SetBoolField(TEXT("exists"), false);
				return Json;
			}

			FString WidgetName;
			FString PropertyName;
			FBlueprintHelperReviewBaselineSnapshotServiceUtils::SplitWidgetPropertyTarget(TargetName, WidgetName, PropertyName);
			UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
			if (!Widget)
			{
				Json->SetBoolField(TEXT("exists"), false);
				Json->SetStringField(TEXT("widget_name"), WidgetName);
				if (!PropertyName.IsEmpty())
				{
					Json->SetStringField(TEXT("property_path"), PropertyName);
				}
				return Json;
			}

			Json->SetBoolField(TEXT("exists"), true);
			Json->SetStringField(TEXT("widget_name"), WidgetName);
			Json->SetStringField(TEXT("widget_class"), Widget->GetClass() ? Widget->GetClass()->GetPathName() : FString());
			int32 ChildIndex = INDEX_NONE;
			if (UPanelWidget* ParentWidget = UWidgetTree::FindWidgetParent(Widget, ChildIndex))
			{
				Json->SetStringField(TEXT("parent_widget"), ParentWidget->GetName());
				Json->SetNumberField(TEXT("child_index"), ChildIndex);
				Json->SetStringField(TEXT("slot_class"), Widget->Slot ? Widget->Slot->GetClass()->GetPathName() : FString());
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
			{
				Json->SetStringField(TEXT("property_path"), PropertyName);
				if (FProperty* Property = Widget->GetClass() ? Widget->GetClass()->FindPropertyByName(FName(*PropertyName)) : nullptr)
				{
					Json->SetStringField(TEXT("property_class"), Property->GetClass()->GetName());
					FString Value;
					Property->ExportText_InContainer(0, Value, Widget, Widget, Widget, PPF_None);
					Json->SetStringField(TEXT("value"), Value);
				}
			}
			else
			{
				Json->SetObjectField(TEXT("properties"), BuildGenericObjectSnapshot(Widget));
			}
			return Json;
		}
	}

	if (UDataTable* DataTable = Cast<UDataTable>(Asset))
	{
		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::DataTableRow)
		{
			Json->SetStringField(TEXT("surface"), TEXT("data_table"));
			Json->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetPathName() : FString());
			if (const UScriptStruct* RowStruct = DataTable->GetRowStruct())
			{
				if (uint8* const* RowData = DataTable->GetRowMap().Find(FName(*TargetName)))
				{
					Json->SetBoolField(TEXT("exists"), true);
					FString RowValue;
					if (*RowData)
					{
						RowStruct->ExportText(RowValue, *RowData, nullptr, DataTable, PPF_None, nullptr);
					}
					Json->SetStringField(TEXT("value"), RowValue);
					return Json;
				}
			}

			Json->SetBoolField(TEXT("exists"), false);
			return Json;
		}
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::ObjectProperty)
	{
		Json->SetStringField(TEXT("surface"), TEXT("details"));
		UObject* PropertyOwner = FBlueprintHelperReviewTargetKindRegistry::IsClassDefaultPropertyTargetKind(Target.TargetKind)
			? FBlueprintHelperReviewBaselineSnapshotServiceUtils::ResolveClassDefaultSnapshotObject(Asset)
			: Asset;
		Json->SetStringField(TEXT("property_owner_class"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(PropertyOwner));
		Json->SetStringField(TEXT("property_owner_path"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(PropertyOwner));
		if (PropertyOwner && PropertyOwner->GetClass())
		{
			FProperty* Property = nullptr;
			void* ValuePtr = nullptr;
			FString ExpectedType;
			FString ErrorCode;
			FString ErrorMessage;
			if (FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
				PropertyOwner,
				TargetName,
				Property,
				ValuePtr,
				ExpectedType,
				ErrorCode,
				ErrorMessage) &&
				Property &&
				ValuePtr)
			{
				Json->SetBoolField(TEXT("exists"), true);
				Json->SetStringField(TEXT("property_path"), TargetName);
				Json->SetStringField(TEXT("property_class"), Property->GetClass()->GetName());
				Json->SetStringField(TEXT("expected_type"), ExpectedType);
				FString Value;
				Property->ExportText_Direct(Value, ValuePtr, nullptr, PropertyOwner, PPF_None);
				Json->SetStringField(TEXT("value"), Value);
				return Json;
			}
			Json->SetStringField(TEXT("resolve_error_code"), ErrorCode);
			Json->SetStringField(TEXT("resolve_error_message"), ErrorMessage);
		}

		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::AssetFactory)
	{
		Json->SetStringField(TEXT("surface"), TEXT("asset"));
		Json->SetBoolField(TEXT("exists"), true);
		Json->SetObjectField(TEXT("asset"), BuildAssetSnapshot(Target.AssetPath, Asset));
		return Json;
	}

	Json->SetBoolField(TEXT("exists"), true);
	Json->SetObjectField(TEXT("asset"), BuildAssetSnapshot(Target.AssetPath, Asset));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildTargetSnapshotFromBaselineAssetSnapshot(
	const FBlueprintHelperReviewAtomicTarget& Target,
	const TSharedPtr<FJsonObject>& AssetSnapshot)
{
	FString AssetClass;
	if (AssetSnapshot.IsValid())
	{
		AssetSnapshot->TryGetStringField(TEXT("object_class"), AssetClass);
	}

	TSharedRef<FJsonObject> Json = BuildTargetSnapshotHeader(Target, AssetSnapshot.IsValid(), AssetClass);
	const FString TargetName = FBlueprintHelperReviewBaselineSnapshotServiceUtils::ExtractTargetName(Target);
	if (!AssetSnapshot.IsValid())
	{
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
		FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);

	const TSharedPtr<FJsonObject>* BlueprintSnapshotPtr = nullptr;
	const TSharedPtr<FJsonObject> BlueprintSnapshot =
		AssetSnapshot->TryGetObjectField(TEXT("blueprint"), BlueprintSnapshotPtr) && BlueprintSnapshotPtr
			? *BlueprintSnapshotPtr
			: nullptr;

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::GraphNode)
	{
		Json->SetStringField(TEXT("surface"), TEXT("graph"));
		Json->SetStringField(TEXT("graph_name"), Target.GraphName);
		const TSharedPtr<FJsonObject> GraphObject = UBlueprintHelperReviewUtils::FindBaselineGraphObject(BlueprintSnapshot, Target.GraphName);
		if (!GraphObject.IsValid())
		{
			Json->SetBoolField(TEXT("exists"), false);
			Json->SetStringField(TEXT("resolve_error_code"), TEXT("graph_not_found"));
			return Json;
		}

		const FString NodeName = UBlueprintHelperReviewUtils::ExtractReviewSnapshotAnchorName(Target.TargetKey, TEXT("node"));
		const TSharedPtr<FJsonObject> NodeObject = UBlueprintHelperReviewUtils::FindBaselineNodeObject(GraphObject, Target.NodeGuid, NodeName);
		if (!NodeObject.IsValid())
		{
			Json->SetBoolField(TEXT("exists"), false);
			Json->SetStringField(TEXT("resolve_error_code"), TEXT("node_not_found"));
			return Json;
		}

		Json->SetBoolField(TEXT("exists"), true);
		Json->SetObjectField(TEXT("node"), NodeObject);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::GraphBlock)
	{
		Json->SetStringField(TEXT("surface"), TEXT("graph"));
		Json->SetStringField(TEXT("graph_name"), Target.GraphName);
		const FString BlockId = UBlueprintHelperReviewUtils::ExtractReviewSnapshotAnchorName(Target.TargetKey, TEXT("block"));
		Json->SetStringField(TEXT("block_id"), BlockId);
		const TSharedPtr<FJsonObject> GraphObject = UBlueprintHelperReviewUtils::FindBaselineGraphObject(BlueprintSnapshot, Target.GraphName);
		if (!GraphObject.IsValid())
		{
			Json->SetBoolField(TEXT("exists"), false);
			Json->SetStringField(TEXT("resolve_error_code"), TEXT("graph_not_found"));
			return Json;
		}

		TArray<TSharedPtr<FJsonObject>> BlockNodes;
		const TArray<TSharedPtr<FJsonValue>>* NodeValues = nullptr;
		if (GraphObject->TryGetArrayField(TEXT("nodes"), NodeValues) && NodeValues)
		{
			for (const TSharedPtr<FJsonValue>& NodeValue : *NodeValues)
			{
				TSharedPtr<FJsonObject> NodeObject = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
				if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(NodeObject, TEXT("block_id"), BlockId))
				{
					BlockNodes.Add(NodeObject);
				}
			}
		}
		BlockNodes.Sort([](const TSharedPtr<FJsonObject>& Left, const TSharedPtr<FJsonObject>& Right)
		{
			FString LeftGuid;
			FString RightGuid;
			FString LeftName;
			FString RightName;
			if (Left.IsValid())
			{
				Left->TryGetStringField(TEXT("guid"), LeftGuid);
				Left->TryGetStringField(TEXT("name"), LeftName);
			}
			if (Right.IsValid())
			{
				Right->TryGetStringField(TEXT("guid"), RightGuid);
				Right->TryGetStringField(TEXT("name"), RightName);
			}
			return (LeftGuid + TEXT("|") + LeftName) < (RightGuid + TEXT("|") + RightName);
		});

		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const TSharedPtr<FJsonObject>& NodeObject : BlockNodes)
		{
			if (NodeObject.IsValid())
			{
				Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
			}
		}
		Json->SetBoolField(TEXT("exists"), Nodes.Num() > 0);
		Json->SetArrayField(TEXT("nodes"), Nodes);
		if (Nodes.Num() == 0)
		{
			Json->SetStringField(TEXT("resolve_error_code"), TEXT("block_not_found"));
		}
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable)
	{
		Json->SetStringField(TEXT("surface"), TEXT("my_blueprint"));
		for (const TSharedPtr<FJsonValue>& VariableValue : UBlueprintHelperReviewUtils::CopyBaselineJsonArray(BlueprintSnapshot, TEXT("variables")))
		{
			const TSharedPtr<FJsonObject> VariableObject = VariableValue.IsValid() ? VariableValue->AsObject() : nullptr;
			if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(VariableObject, TEXT("name"), TargetName))
			{
				Json->SetBoolField(TEXT("exists"), true);
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : VariableObject->Values)
				{
					Json->SetField(Field.Key, Field.Value);
				}
				return Json;
			}
		}
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
	{
		Json->SetStringField(TEXT("surface"), TEXT("components"));
		for (const TSharedPtr<FJsonValue>& ComponentValue : UBlueprintHelperReviewUtils::CopyBaselineJsonArray(BlueprintSnapshot, TEXT("components")))
		{
			const TSharedPtr<FJsonObject> ComponentObject = ComponentValue.IsValid() ? ComponentValue->AsObject() : nullptr;
			if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(ComponentObject, TEXT("name"), TargetName))
			{
				Json->SetBoolField(TEXT("exists"), true);
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : ComponentObject->Values)
				{
					Json->SetField(Field.Key, Field.Value);
				}
				return Json;
			}
		}
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Signature)
	{
		Json->SetStringField(TEXT("surface"), TEXT("my_blueprint"));
		const TSharedPtr<FJsonObject> GraphObject = UBlueprintHelperReviewUtils::FindBaselineGraphObject(BlueprintSnapshot, TargetName);
		Json->SetBoolField(TEXT("exists"), GraphObject.IsValid());
		if (GraphObject.IsValid())
		{
			Json->SetObjectField(TEXT("graph"), GraphObject);
		}
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget
		|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
	{
		Json->SetStringField(TEXT("surface"), TEXT("umg_widget_tree"));
		FString WidgetName;
		FString PropertyName;
		FBlueprintHelperReviewBaselineSnapshotServiceUtils::SplitWidgetPropertyTarget(TargetName, WidgetName, PropertyName);
		Json->SetStringField(TEXT("widget_name"), WidgetName);
		if (!PropertyName.IsEmpty())
		{
			Json->SetStringField(TEXT("property_path"), PropertyName);
		}

		const TSharedPtr<FJsonObject>* WidgetTreePtr = nullptr;
		const TSharedPtr<FJsonObject> WidgetTreeSnapshot =
			BlueprintSnapshot.IsValid() && BlueprintSnapshot->TryGetObjectField(TEXT("widget_tree"), WidgetTreePtr) && WidgetTreePtr
				? *WidgetTreePtr
				: nullptr;
		for (const TSharedPtr<FJsonValue>& WidgetValue : UBlueprintHelperReviewUtils::CopyBaselineJsonArray(WidgetTreeSnapshot, TEXT("widgets")))
		{
			const TSharedPtr<FJsonObject> WidgetObject = WidgetValue.IsValid() ? WidgetValue->AsObject() : nullptr;
			if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(WidgetObject, TEXT("name"), WidgetName))
			{
				Json->SetBoolField(TEXT("exists"), true);
				FString WidgetClass;
				WidgetObject->TryGetStringField(TEXT("class"), WidgetClass);
				Json->SetStringField(TEXT("widget_class"), WidgetClass);
				return Json;
			}
		}
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::DataTableRow)
	{
		Json->SetStringField(TEXT("surface"), TEXT("data_table"));
		const TSharedPtr<FJsonObject>* DataTablePtr = nullptr;
		const TSharedPtr<FJsonObject> DataTableSnapshot =
			AssetSnapshot->TryGetObjectField(TEXT("data_table"), DataTablePtr) && DataTablePtr
				? *DataTablePtr
				: nullptr;
		FString RowStruct;
		if (DataTableSnapshot.IsValid())
		{
			DataTableSnapshot->TryGetStringField(TEXT("row_struct"), RowStruct);
		}
		Json->SetStringField(TEXT("row_struct"), RowStruct);
		for (const TSharedPtr<FJsonValue>& RowValue : UBlueprintHelperReviewUtils::CopyBaselineJsonArray(DataTableSnapshot, TEXT("rows")))
		{
			const TSharedPtr<FJsonObject> RowObject = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
			if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(RowObject, TEXT("name"), TargetName))
			{
				Json->SetBoolField(TEXT("exists"), true);
				FString Value;
				RowObject->TryGetStringField(TEXT("value"), Value);
				Json->SetStringField(TEXT("value"), Value);
				return Json;
			}
		}
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::ObjectProperty)
	{
		Json->SetStringField(TEXT("surface"), TEXT("details"));
		const TSharedPtr<FJsonObject>* ObjectPropertiesPtr = nullptr;
		const TSharedPtr<FJsonObject> ObjectPropertiesSnapshot =
			AssetSnapshot->TryGetObjectField(TEXT("object_properties"), ObjectPropertiesPtr) && ObjectPropertiesPtr
				? *ObjectPropertiesPtr
				: nullptr;
		for (const TSharedPtr<FJsonValue>& PropertyValue : UBlueprintHelperReviewUtils::CopyBaselineJsonArray(ObjectPropertiesSnapshot, TEXT("properties")))
		{
			const TSharedPtr<FJsonObject> PropertyObject = PropertyValue.IsValid() ? PropertyValue->AsObject() : nullptr;
			if (UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(PropertyObject, TEXT("name"), TargetName))
			{
				Json->SetBoolField(TEXT("exists"), true);
				Json->SetStringField(TEXT("property_path"), TargetName);
				FString PropertyClass;
				FString Value;
				PropertyObject->TryGetStringField(TEXT("class"), PropertyClass);
				PropertyObject->TryGetStringField(TEXT("value"), Value);
				Json->SetStringField(TEXT("property_class"), PropertyClass);
				Json->SetStringField(TEXT("value"), Value);
				return Json;
			}
		}
		Json->SetBoolField(TEXT("exists"), false);
		return Json;
	}

	if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::AssetFactory)
	{
		Json->SetStringField(TEXT("surface"), TEXT("asset"));
		Json->SetBoolField(TEXT("exists"), true);
		Json->SetObjectField(TEXT("asset"), AssetSnapshot);
		return Json;
	}

	Json->SetBoolField(TEXT("exists"), true);
	Json->SetObjectField(TEXT("asset"), AssetSnapshot);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildDataTableSnapshot(const UDataTable* DataTable)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("row_struct"), DataTable && DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetPathName() : FString());

	TArray<TSharedPtr<FJsonValue>> Rows;
	if (DataTable && DataTable->GetRowStruct())
	{
		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		TArray<FName> RowNames;
		DataTable->GetRowMap().GetKeys(RowNames);
		RowNames.Sort([](const FName& Left, const FName& Right)
		{
			return Left.ToString() < Right.ToString();
		});
		for (const FName& RowName : RowNames)
		{
			uint8* const* RowData = DataTable->GetRowMap().Find(RowName);
			TSharedRef<FJsonObject> RowJson = MakeShared<FJsonObject>();
			RowJson->SetStringField(TEXT("name"), RowName.ToString());
			FString RowValue;
			if (RowData && *RowData)
			{
				RowStruct->ExportText(RowValue, *RowData, nullptr, const_cast<UDataTable*>(DataTable), PPF_None, nullptr);
			}
			RowJson->SetStringField(TEXT("value"), RowValue);
			Rows.Add(MakeShared<FJsonValueObject>(RowJson));
		}
	}
	Json->SetArrayField(TEXT("rows"), Rows);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildGenericObjectSnapshot(UObject* Asset)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Properties;
	if (!Asset || !Asset->GetClass())
	{
		Json->SetArrayField(TEXT("properties"), Properties);
		return Json;
	}

	for (TFieldIterator<FProperty> It(Asset->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property ||
			!Property->HasAnyPropertyFlags(CPF_Edit) ||
			Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			continue;
		}

		TSharedRef<FJsonObject> PropertyJson = MakeShared<FJsonObject>();
		PropertyJson->SetStringField(TEXT("name"), Property->GetName());
		PropertyJson->SetStringField(TEXT("class"), Property->GetClass()->GetName());
		FString Value;
		Property->ExportText_InContainer(0, Value, Asset, Asset, Asset, PPF_None);
		PropertyJson->SetStringField(TEXT("value"), Value);
		Properties.Add(MakeShared<FJsonValueObject>(PropertyJson));
	}

	Json->SetArrayField(TEXT("properties"), Properties);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildGraphSnapshot(
	const UEdGraph* Graph,
	const FString& Surface)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), Graph ? Graph->GetName() : FString());
	Json->SetStringField(TEXT("surface"), Surface);

	TArray<TSharedPtr<FJsonValue>> Nodes;
	if (Graph)
	{
		TArray<const UEdGraphNode*> SortedNodes;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				SortedNodes.Add(Node);
			}
		}
		SortedNodes.Sort([](const UEdGraphNode& Left, const UEdGraphNode& Right)
		{
			return UBlueprintHelperReviewUtils::MakeReviewSnapshotNodeSortKey(&Left) < UBlueprintHelperReviewUtils::MakeReviewSnapshotNodeSortKey(&Right);
		});
		for (const UEdGraphNode* Node : SortedNodes)
		{
			Nodes.Add(MakeShared<FJsonValueObject>(BuildNodeSnapshot(Node)));
		}
	}
	Json->SetArrayField(TEXT("nodes"), Nodes);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildNodeSnapshot(const UEdGraphNode* Node)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), Node ? Node->GetName() : FString());
	Json->SetStringField(TEXT("guid"), Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString());
	Json->SetStringField(TEXT("class"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(Node));
	Json->SetStringField(TEXT("title"), Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString());
	Json->SetStringField(TEXT("block_id"), UBlueprintHelperReviewUtils::GetReviewSnapshotNodeBlockId(Node));
	Json->SetNumberField(TEXT("x"), Node ? Node->NodePosX : 0);
	Json->SetNumberField(TEXT("y"), Node ? Node->NodePosY : 0);

	TArray<TSharedPtr<FJsonValue>> Pins;
	if (Node)
	{
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			TSharedRef<FJsonObject> PinJson = MakeShared<FJsonObject>();
			PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinJson->SetStringField(TEXT("direction"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::PinDirectionToString(Pin->Direction));
			PinJson->SetStringField(TEXT("pin_category"), Pin->PinType.PinCategory.ToString());
			PinJson->SetStringField(TEXT("pin_sub_category"), Pin->PinType.PinSubCategory.ToString());
			PinJson->SetStringField(TEXT("default_value"), Pin->DefaultValue);

			TArray<FString> LinkedPins;
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}
				LinkedPins.Add(FString::Printf(
					TEXT("%s:%s"),
					*LinkedPin->GetOwningNode()->NodeGuid.ToString(EGuidFormats::Digits),
					*LinkedPin->PinName.ToString()));
			}
			LinkedPins.Sort();
			PinJson->SetArrayField(TEXT("linked_to"), FBlueprintHelperReviewBaselineSnapshotServiceUtils::MakeStringArray(LinkedPins));
			Pins.Add(MakeShared<FJsonValueObject>(PinJson));
		}
	}
	Json->SetArrayField(TEXT("pins"), Pins);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildWidgetTreeSnapshot(UWidgetTree* WidgetTree)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("root_widget"), WidgetTree && WidgetTree->RootWidget ? WidgetTree->RootWidget->GetName() : FString());

	TArray<TSharedPtr<FJsonValue>> Widgets;
	if (WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		for (const UWidget* Widget : AllWidgets)
		{
			if (!Widget)
			{
				continue;
			}

			TSharedRef<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
			WidgetJson->SetStringField(TEXT("name"), Widget->GetName());
			WidgetJson->SetStringField(TEXT("class"), Widget->GetClass() ? Widget->GetClass()->GetPathName() : FString());
			Widgets.Add(MakeShared<FJsonValueObject>(WidgetJson));
		}
	}
	Json->SetArrayField(TEXT("widgets"), Widgets);
	return Json;
}

