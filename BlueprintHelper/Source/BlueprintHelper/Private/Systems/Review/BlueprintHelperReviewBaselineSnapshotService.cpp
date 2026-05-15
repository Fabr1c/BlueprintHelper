// BlueprintHelper Review baseline semantic snapshot service.

#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
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
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace BlueprintHelperReviewBaselineSnapshotServiceLocal
{
	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	static FString GetObjectPathNameSafe(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	static FString GetObjectClassPathNameSafe(const UObject* Object)
	{
		return Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString();
	}

	static FString PinDirectionToString(const EEdGraphPinDirection Direction)
	{
		return Direction == EGPD_Output ? TEXT("output") : TEXT("input");
	}

	static void AppendGraphs(TArray<UEdGraph*>& OutGraphs, const TArray<UEdGraph*>& InGraphs)
	{
		for (UEdGraph* Graph : InGraphs)
		{
			if (Graph)
			{
				OutGraphs.Add(Graph);
			}
		}
	}
}

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
		FPaths::ProjectSavedDir(),
		TEXT("BlueprintHelper"),
		TEXT("Review"),
		TEXT("Snapshots"),
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
	Snapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot.v1"));
	Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
	Snapshot->SetStringField(TEXT("object_name"), GetNameSafe(Asset));
	Snapshot->SetStringField(TEXT("object_path"), BlueprintHelperReviewBaselineSnapshotServiceLocal::GetObjectPathNameSafe(Asset));
	Snapshot->SetStringField(TEXT("object_class"), BlueprintHelperReviewBaselineSnapshotServiceLocal::GetObjectClassPathNameSafe(Asset));
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
			VariableJson->SetStringField(TEXT("pin_sub_category_object"), BlueprintHelperReviewBaselineSnapshotServiceLocal::GetObjectPathNameSafe(Variable.VarType.PinSubCategoryObject.Get()));
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
		BlueprintHelperReviewBaselineSnapshotServiceLocal::AppendGraphs(Graphs, Blueprint->UbergraphPages);
		BlueprintHelperReviewBaselineSnapshotServiceLocal::AppendGraphs(Graphs, Blueprint->FunctionGraphs);
		BlueprintHelperReviewBaselineSnapshotServiceLocal::AppendGraphs(Graphs, Blueprint->MacroGraphs);
		BlueprintHelperReviewBaselineSnapshotServiceLocal::AppendGraphs(Graphs, Blueprint->DelegateSignatureGraphs);
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

TSharedRef<FJsonObject> FBlueprintHelperReviewBaselineSnapshotService::BuildDataTableSnapshot(const UDataTable* DataTable)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("row_struct"), DataTable && DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetPathName() : FString());

	TArray<TSharedPtr<FJsonValue>> Rows;
	if (DataTable && DataTable->GetRowStruct())
	{
		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		for (const TPair<FName, uint8*>& RowPair : DataTable->GetRowMap())
		{
			TSharedRef<FJsonObject> RowJson = MakeShared<FJsonObject>();
			RowJson->SetStringField(TEXT("name"), RowPair.Key.ToString());
			FString RowValue;
			if (RowPair.Value)
			{
				RowStruct->ExportText(RowValue, RowPair.Value, nullptr, const_cast<UDataTable*>(DataTable), PPF_None, nullptr);
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
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Nodes.Add(MakeShared<FJsonValueObject>(BuildNodeSnapshot(Node)));
			}
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
	Json->SetStringField(TEXT("class"), BlueprintHelperReviewBaselineSnapshotServiceLocal::GetObjectClassPathNameSafe(Node));
	Json->SetStringField(TEXT("title"), Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString());
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
			PinJson->SetStringField(TEXT("direction"), BlueprintHelperReviewBaselineSnapshotServiceLocal::PinDirectionToString(Pin->Direction));
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
			PinJson->SetArrayField(TEXT("linked_to"), BlueprintHelperReviewBaselineSnapshotServiceLocal::MakeStringArray(LinkedPins));
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
