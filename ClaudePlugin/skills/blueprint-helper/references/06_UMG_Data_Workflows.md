# 06 - UMG Data Workflows

For any UMG, DataAsset, DataTable, or UObject write, call `blueprinthelper_request_write_session` after preview only when `write_permission` is disabled. The Editor prompt is intentionally simple accept/reject; rejection stops the write.

UMG、DataAsset、DataTable 和 UObject 属性写入都走 TaskSpec-first。不要直接选择冻结入口。

## UMG

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
3. preview 通过后执行。
4. 写后读取目标行确认。

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

## Object Properties

用 `edit_object_properties` 描述属性路径和值。复杂结构体、枚举、软对象路径和类路径必须使用 UE 可导入文本格式。
