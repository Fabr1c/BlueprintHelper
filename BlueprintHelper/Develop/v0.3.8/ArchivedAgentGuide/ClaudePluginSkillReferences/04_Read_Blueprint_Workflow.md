# 04 - Read Blueprint Workflow

普通读取只使用 `blueprinthelper_read_context`。

## Size Gate Before LogicMd

不要在不清楚图表大小时直接读取整个图表的 `logic_md`。先用轻量视图估算规模：

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
    "format": "summary",
    "detail": "brief",
    "max_items": 80
  }
}
```

如果返回的 stats、summary 或结构化结果显示节点数量大于 80，或者结果被截断，不要再请求整个图表的 `logic_md`。改用分块读取：

- 只读用户目标相关的 graph、function、event、custom_event 或 block。
- 需要锚点时优先读 `logic_json`，并只取 grouped block 中的稳定引用。
- 无法定位目标 block 时，先向主 Agent 回交需要更明确目标，不要扩大成全图读取。

## Logic Summary

仅当图表规模已知且不超过 80 个节点，或目标范围已经缩小到局部图表时，才读取 `logic_md`：

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
    "detail": "normal",
    "max_items": 80
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
    "detail": "normal",
    "max_items": 80
  }
}
```

只从返回的 grouped block 中使用 `block_id`、`group_entry_node_path`、`node_ref`、`pin_ref` 和必要的 `link_ref`。不要根据显示名或当前焦点猜锚点。

## Reference Context

高风险修改前使用 `blueprinthelper_read_reference_context` 读取影响面，例如变量、函数、Widget 或 DataTable 行引用。
