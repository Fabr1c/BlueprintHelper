#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

FBlueprintHelperAssetFactoryToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadAssetFactoryPolicy()
{
	FBlueprintHelperAssetFactoryToolClusterPolicy Policy;
	Policy.DefaultParentClass = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.asset_factory.default_parent_class"), Policy.DefaultParentClass);
	Policy.DefaultValueType = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.asset_factory.default_value_type"), Policy.DefaultValueType);
	Policy.DefaultCollisionPolicy = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.asset_factory.default_collision_policy"), Policy.DefaultCollisionPolicy);
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.asset_factory.dry_run"), Policy.bDryRun);
	return Policy;
}

FBlueprintHelperComponentToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadComponentPolicy()
{
	FBlueprintHelperComponentToolClusterPolicy Policy;
	Policy.DefaultAttachRule = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.component.default_attach_rule"), Policy.DefaultAttachRule);
	Policy.DefaultNameCollisionPolicy = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.component.default_name_collision_policy"), Policy.DefaultNameCollisionPolicy);
	Policy.DefaultPropertyMode = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.component.default_property_mode"), Policy.DefaultPropertyMode);
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.component.dry_run"), Policy.bDryRun);
	return Policy;
}

FBlueprintHelperClassSettingsToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadClassSettingsPolicy()
{
	FBlueprintHelperClassSettingsToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.class_settings.dry_run"), Policy.bDryRun);
	Policy.bValidationShouldCompile = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.class_settings.validation_should_compile"), Policy.bValidationShouldCompile);
	Policy.bValidationShouldSave = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.class_settings.validation_should_save"), Policy.bValidationShouldSave);
	return Policy;
}

FBlueprintHelperBlueprintVariablesToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadBlueprintVariablesPolicy()
{
	FBlueprintHelperBlueprintVariablesToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.blueprint_variables.dry_run"), Policy.bDryRun);
	Policy.ReadMemberDefaultsScope = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.blueprint_variables.read_member_defaults_scope"), Policy.ReadMemberDefaultsScope);
	Policy.AssetPathFallback = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.blueprint_variables.asset_path_fallback"), Policy.AssetPathFallback);
	return Policy;
}

FBlueprintHelperObjectPropertyToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadObjectPropertyPolicy()
{
	FBlueprintHelperObjectPropertyToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.object_property.dry_run"), Policy.bDryRun);
	return Policy;
}

FBlueprintHelperDataTableToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadDataTablePolicy()
{
	FBlueprintHelperDataTableToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.data_table.dry_run"), Policy.bDryRun);
	Policy.bWriteRequiresRowStruct = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.data_table.write_requires_row_struct"), Policy.bWriteRequiresRowStruct);
	return Policy;
}

FBlueprintHelperUmgWidgetToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadUmgWidgetPolicy()
{
	FBlueprintHelperUmgWidgetToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.umg_widget.dry_run"), Policy.bDryRun);
	Policy.bAssetPathRequired = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.umg_widget.asset_path_required"), Policy.bAssetPathRequired);
	return Policy;
}

FBlueprintHelperSignatureToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadSignaturePolicy()
{
	FBlueprintHelperSignatureToolClusterPolicy Policy;
	Policy.ReferenceContextSearchScope = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.signature.reference_context_search_scope"), Policy.ReferenceContextSearchScope);
	Policy.ReferenceContextResolutionPolicy = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.signature.reference_context_resolution_policy"), Policy.ReferenceContextResolutionPolicy);
	Policy.ReferenceContextDetail = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.signature.reference_context_detail"), Policy.ReferenceContextDetail);
	Policy.ReferenceContextMaxResults = FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("tool_clusters.signature.reference_context_max_results"), Policy.ReferenceContextMaxResults);
	return Policy;
}

FBlueprintHelperGraphWriteToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy()
{
	FBlueprintHelperGraphWriteToolClusterPolicy Policy;
	Policy.bDryRun = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.dry_run"), Policy.bDryRun);
	Policy.bStrict = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.strict"), Policy.bStrict);
	Policy.bCreateMissingVariables = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.create_missing_variables"), Policy.bCreateMissingVariables);
	Policy.bReconstructExistingNodes = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.reconstruct_existing_nodes"), Policy.bReconstructExistingNodes);
	Policy.bCompile = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.compile"), Policy.bCompile);
	Policy.bSave = FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("tool_clusters.graph_write.save"), Policy.bSave);
	Policy.Layout = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.graph_write.layout"), Policy.Layout);
	return Policy;
}

FBlueprintHelperReadContextToolClusterPolicy FBlueprintHelperToolClusterConfigResolver::LoadReadContextPolicy()
{
	FBlueprintHelperReadContextToolClusterPolicy Policy;
	Policy.DefaultScope = FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("tool_clusters.read_context.default_scope"), Policy.DefaultScope);
	Policy.MaxOutputRows = FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("tool_clusters.read_context.max_output_rows"), Policy.MaxOutputRows);
	Policy.MaxOutputBytes = FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("tool_clusters.read_context.max_output_bytes"), Policy.MaxOutputBytes);
	return Policy;
}

bool FBlueprintHelperReadContextOutputLimiter::IsReadContextCommand(const FString& Command)
{
	static const TSet<FString> ReadContextCommands = {
		TEXT("read_reference_context"),
		TEXT("read_function_chain_context"),
		TEXT("read_blueprint_logic_md"),
		TEXT("read_blueprint_logic_json"),
		TEXT("export_to_json"),
		TEXT("export_logic"),
		TEXT("get_asset_info"),
		TEXT("list_assets"),
		TEXT("search_assets"),
		TEXT("list_graphs"),
		TEXT("list_variables"),
		TEXT("list_event_dispatchers"),
		TEXT("read_blueprint_member_variables"),
		TEXT("read_blueprint_member_defaults"),
		TEXT("read_blueprint_local_variables"),
		TEXT("get_widget_tree"),
		TEXT("get_widget_properties"),
		TEXT("get_datatable_rows"),
		TEXT("get_object_properties"),
		TEXT("read_components"),
		TEXT("read_class_settings")
	};
	return ReadContextCommands.Contains(Command);
}

void FBlueprintHelperReadContextOutputLimiter::ApplyToBridgeResult(const FString& Command, const TSharedPtr<FJsonObject>& ResultJson)
{
	if (!ResultJson.IsValid() || !IsReadContextCommand(Command))
	{
		return;
	}

	const FBlueprintHelperReadContextToolClusterPolicy Policy = FBlueprintHelperToolClusterConfigResolver::LoadReadContextPolicy();
	bool bLimitedRows = false;
	if (Policy.MaxOutputRows > 0)
	{
		bLimitedRows = LimitObjectRows(ResultJson, Policy.MaxOutputRows);
	}

	if (Policy.MaxOutputBytes > 0)
	{
		const int32 ActualBytes = MeasureJsonUtf8Bytes(ResultJson);
		if (ActualBytes > Policy.MaxOutputBytes)
		{
			ReplaceDataWithByteLimitSummary(ResultJson, Policy.MaxOutputBytes, ActualBytes);
			ResultJson->SetBoolField(TEXT("read_context_output_limited"), true);
			ResultJson->SetStringField(TEXT("read_context_limit_reason"), TEXT("max_output_bytes"));
			return;
		}
	}

	if (bLimitedRows)
	{
		ResultJson->SetBoolField(TEXT("read_context_output_limited"), true);
		ResultJson->SetStringField(TEXT("read_context_limit_reason"), TEXT("max_output_rows"));
	}
}

bool FBlueprintHelperReadContextOutputLimiter::LimitObjectRows(const TSharedPtr<FJsonObject>& Object, int32 MaxRows)
{
	if (!Object.IsValid() || MaxRows <= 0)
	{
		return false;
	}

	bool bLimited = false;
	TArray<FString> Keys;
	Object->Values.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		TSharedPtr<FJsonValue>* ValuePtr = Object->Values.Find(Key);
		if (!ValuePtr || !ValuePtr->IsValid())
		{
			continue;
		}

		if ((*ValuePtr)->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> Array = (*ValuePtr)->AsArray();
			if (LimitArrayRows(Array, MaxRows))
			{
				Object->SetArrayField(Key, Array);
				bLimited = true;
			}
			continue;
		}

		if ((*ValuePtr)->Type == EJson::Object)
		{
			bLimited |= LimitObjectRows((*ValuePtr)->AsObject(), MaxRows);
		}
	}
	return bLimited;
}

bool FBlueprintHelperReadContextOutputLimiter::LimitArrayRows(TArray<TSharedPtr<FJsonValue>>& Array, int32 MaxRows)
{
	if (MaxRows <= 0)
	{
		return false;
	}

	bool bLimited = false;
	for (TSharedPtr<FJsonValue>& Value : Array)
	{
		if (Value.IsValid() && Value->Type == EJson::Object)
		{
			bLimited |= LimitObjectRows(Value->AsObject(), MaxRows);
		}
		else if (Value.IsValid() && Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> NestedArray = Value->AsArray();
			if (LimitArrayRows(NestedArray, MaxRows))
			{
				Value = MakeShared<FJsonValueArray>(NestedArray);
				bLimited = true;
			}
		}
	}

	if (Array.Num() > MaxRows)
	{
		Array.SetNum(MaxRows);
		bLimited = true;
	}
	return bLimited;
}

int32 FBlueprintHelperReadContextOutputLimiter::MeasureJsonUtf8Bytes(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	FTCHARToUTF8 Utf8(*JsonText);
	return Utf8.Length();
}

void FBlueprintHelperReadContextOutputLimiter::ReplaceDataWithByteLimitSummary(
	const TSharedPtr<FJsonObject>& Object,
	int32 MaxBytes,
	int32 ActualBytes)
{
	if (!Object.IsValid())
	{
		return;
	}

	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetBoolField(TEXT("truncated"), true);
	Summary->SetStringField(TEXT("reason"), TEXT("read_context max_output_bytes exceeded"));
	Summary->SetNumberField(TEXT("max_output_bytes"), MaxBytes);
	Summary->SetNumberField(TEXT("actual_output_bytes"), ActualBytes);
	Object->SetObjectField(TEXT("data"), Summary);
}
