# 06 - UMG Data Workflows

For any UMG, DataAsset, DataTable, or UObject write, call `blueprinthelper_request_write_session` after preview only when `write_permission` is disabled. The Editor prompt is intentionally simple accept/reject; rejection stops the write.

If source control is enabled for the target assets, also run `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` after preview and before execute. Stop on `checked_out_by_other`, conflict, unavailable provider, checkout failure, or not-editable status.

UMG、DataAsset、DataTable 和 UObject 属性写入都走 TaskSpec-first。不要直接选择冻结入口。

## Validation Policy

- WidgetBlueprint / UMG writes may request `validation.should_compile=true`.
- DataAsset instances, DataTable, UserDefinedStruct, InputAction, InputMappingContext, and plain UObject property writes must use `validation.should_compile=false`.
- A Blueprint class used as a DataAsset type is created with `asset_type=blueprint_class`, usually `parent_class=PrimaryDataAsset`, and must use `validation.should_compile=true`.
- These data assets do not have a Blueprint compile step. Validate them by read-back: fields, row struct, rows, asset class, or property values.
- Do not treat `no_op` reuse as failure when the existing fixture passes read-back.

## UMG

- Root widget creation: use `edit_umg_widget` with `kind: "create_widget"` and omit `parent_widget_name`, or pass it as an empty string. `widget_class` selects the root type; the UE layer still validates whether the class can be used as a root/container.
- Child widget creation: set `parent_widget_name` to the parent/root widget name. Do not use WidgetBlueprint asset creation as a substitute for WidgetTree edits.

1. 用 `blueprinthelper_read_context` 读取目标 Widget Blueprint 摘要。
2. 用 `edit_umg_widget` 描述控件创建、属性更新或删除。
3. preview 通过后执行。
4. 写后读取上下文确认树结构或关键属性。

TaskSpec behavior:

```json
{
  "widget_strategy": "widget_blueprint_edit",
  "changes": [
    {
      "kind": "update_widget_property",
      "widget_name": "TitleText",
      "property_name": "Text",
      "value": "New Title"
    }
  ]
}
```

## DataTable

1. 读取目标表上下文或 reference context，确认 row struct 和 RowName。
2. 用 `edit_data_table` 描述 add、update 或 delete。
3. 设置 `validation.should_compile=false`。
4. preview 通过后执行。
5. 写后读取目标行确认。

TaskSpec behavior:

```json
{
  "row_strategy": "row_edit",
  "rows": [
    {
      "action": "update",
      "row_name": "Pistol",
      "fields": {
        "Damage": "25"
      }
    }
  ]
}
```

## DataAsset

DataAsset instance creation requires a concrete `UDataAsset` subclass. Do not instantiate abstract base classes. When a smoke or workflow needs a project-owned DataAsset with editable fields, create the DataAsset Blueprint class first, then create the DA instance from that class.

1. Create a Blueprint class asset with `asset_type=blueprint_class`, `parent_class=PrimaryDataAsset`, and `validation.should_compile=true`.
2. Create the DataAsset instance with `asset_type=data_asset`, `data_asset_class` set to the Blueprint class asset path or generated class path, and `validation.should_compile=false`.
3. Do not use `/Script/Engine.DataAsset` or `/Script/Engine.PrimaryDataAsset` as the DA instance class. They are base classes, and `PrimaryDataAsset` is abstract.
4. Validate the DA instance by read-back: asset exists and its class is the generated class from the Blueprint class asset.

TaskSpec behavior for the DA instance:

```json
{
  "asset_strategy": "ensure_asset",
  "asset": {
    "asset_type": "data_asset",
    "data_asset_class": "/Game/BlueprintHelper/Smoke/BP_BHSmokeDataAssetClass",
    "collision_policy": "reuse_if_exists"
  }
}
```

## Object Properties

用 `edit_object_properties` 描述属性路径和值。复杂结构体、枚举、软对象路径和类路径必须使用 UE 可导入文本格式。DataAsset 或普通 UObject 属性写入设置 `validation.should_compile=false`，并通过属性 read-back 验证。
