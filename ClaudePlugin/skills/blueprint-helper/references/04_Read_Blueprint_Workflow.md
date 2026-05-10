# 04 - Read Blueprint Workflow

普通读取只使用 `blueprinthelper_read_context`。

## Logic Summary

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_md",
    "detail": "normal"
  }
}
```

## Structured Anchors

需要 patch 或 merge 锚点时读取 `logic_json`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_json",
    "detail": "normal"
  }
}
```

只从返回的 grouped block 中使用 `block_id`、`group_entry_node_path`、`node_ref`、`pin_ref` 和必要的 `link_ref`。不要根据显示名或当前焦点猜锚点。

## Reference Context

高风险修改前使用 `blueprinthelper_read_reference_context` 读取影响面，例如变量、函数、Widget 或 DataTable 行引用。
