# BlueprintHelper UMG / Widget Blueprint UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：UMG / Widget Blueprint 字段确认稿  
本文边界：确认 Widget Tree 结构写入、普通 Widget 属性写入、Slot 属性写入、Widget 删除、WidgetTree 读取、Widget 属性读取的 Agent-facing 返回字段与 UE 侧结构体映射。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

UMG / Widget Blueprint 工具簇采用以下字段口径：

```text
1. UMG 第一版按 Widget Tree / 普通属性 / Slot 属性拆工具。
2. add_widget_to_tree 只创建并插入 widget，不设置普通属性。
3. add_widget_to_tree 成功只返回 added_count。
4. add_widget_to_tree 成功不返回 widget_ref / widget_path / parent_ref。
5. name_collision 只支持 fail_if_exists / reuse_if_exists。
6. 不支持 auto_rename / replace_existing。
7. set_widget_property / set_widget_properties 成功只返回 property_result 计数。
8. property_result 成功不返回 invalid_settings；失败才在 error.conflicts 中返回问题项。
9. 批量 property 写默认事务化，invalid 时整批失败。
10. set_widget_slot_property / set_widget_slot_properties 与普通属性分离。
11. remove_widget_from_tree 必须 dry_run。
12. remove dry_run passed 极简。
13. remove blocked / failed 才返回 widget_name / ref。
14. remove 成功只返回 removed_count。
15. UMG 写工具成功不返回 write_ref / transaction_id / review / safety。
16. UMG 写工具成功保留 validation，用于后续 compile/save。
17. read_widget_tree 返回压缩树结构，不默认返回所有属性。
18. read_widget_properties / read_widget_slot_properties 独立读取属性值。
19. 所有 UMG 工具 data.schema 使用短命名。
```

---

## 1. 工具簇边界

第一版覆盖：

```text
add_widget_to_tree
set_widget_property
set_widget_properties
set_widget_slot_property
set_widget_slot_properties
remove_widget_from_tree
read_widget_tree
read_widget_properties
read_widget_slot_properties
```

第一版不覆盖：

```text
Widget Animation
复杂 Binding Graph
UMG 事件绑定图表
Slate Brush 批量资源处理
动态生成复杂 UMG 蓝图逻辑
```

---

## 2. 通用返回原则

UMG 写工具成功返回继续采用极简口径。

成功只返回：

```text
数量
状态
validation
```

成功不返回：

```text
widget_ref
widget_path
parent_ref
slot_ref
widget_tree snapshot
before / after
all_properties
write_ref
transaction_id
review
safety
diagnostics
```

错误或 blocked 场景才返回可定位问题的：

```text
widget_name
property
slot_type
ref
```

---

## 3. data.schema 短命名

UMG 工具 data.schema 使用短命名。

使用：

```text
AddWidgetToTree.v1
SetWidgetProperty.v1
SetWidgetSlotProperty.v1
RemoveWidgetFromTree.v1
RemoveWidgetFromTreeDryRun.v1
ReadWidgetTree.v1
ReadWidgetProperties.v1
ReadWidgetSlotProperties.v1
```

不使用：

```text
BlueprintHelper.AddWidgetToTree.v1
BlueprintHelper.Tools.UMG.AddWidgetToTree.v1
BlueprintHelper.MCP.AddWidgetToTree.v1
```

---

# 4. add_widget_to_tree

## 4.1 工具定位

`add_widget_to_tree` 只负责：

```text
创建一个 Widget 并插入 WidgetTree 的明确父级下。
```

它负责：

```text
widget_class
widget_name
parent_widget_name
slot 插入关系
name_collision 策略
```

它不负责：

```text
设置 Text / Color / Visibility 等普通属性
设置 Slot 细节属性
绑定事件
绑定变量
创建动画
写蓝图图表逻辑
```

普通属性使用：

```text
set_widget_property / set_widget_properties
```

Slot 属性使用：

```text
set_widget_slot_property / set_widget_slot_properties
```

---

## 4.2 operation

```json
"operation": "add_widget_to_tree"
```

---

## 4.3 target

```json
"target": {
  "asset_path": "/Game/UI/WBP_MainMenu",
  "widget_scope": "widget_tree"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | Widget Blueprint 资产路径。 |
| `WidgetScope` | `EBlueprintHelperWidgetScope` | `target.widget_scope` | `string enum` | 是 | 固定为 `widget_tree`。 |

---

## 4.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_widget_to_tree",
  "trace_id": "trace_20260503_2401",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "data": {
    "schema": "AddWidgetToTree.v1",
    "add_widget_result": {
      "added_count": 1
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

成功不返回：

```text
widget_name
widget_ref
widget_path
parent_ref
slot_ref
widget_tree
write_ref
transaction_id
review
safety
```

---

## 4.5 add_widget_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AddedCount` | `int32` | `data.add_widget_result.added_count` | `number` | 是 | 本次新增 Widget 数量。 |
| `bReusedExisting` | `bool` | `data.add_widget_result.reused_existing` | `boolean` | no_op 时 | name_collision=reuse_if_exists 且已存在时返回。 |

---

## 4.6 name_collision

第一版只支持：

```text
fail_if_exists
reuse_if_exists
```

不支持：

```text
auto_rename
replace_existing
```

---

## 4.7 name_collision=fail_if_exists 冲突

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_widget_to_tree",
  "trace_id": "trace_20260503_2402",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "error": {
    "code": "widget_name_already_exists",
    "stage": "name_collision_check",
    "message": "A widget with the requested name already exists.",
    "retryable": false,
    "conflicts": [
      {
        "code": "widget_name_already_exists",
        "widget_name": "StartButton"
      }
    ]
  }
}
```

---

## 4.8 name_collision=reuse_if_exists no_op

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_widget_to_tree",
  "trace_id": "trace_20260503_2403",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "data": {
    "schema": "AddWidgetToTree.v1",
    "add_widget_result": {
      "added_count": 0,
      "reused_existing": true
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": false,
    "compiled": false,
    "saved": false
  }
}
```

---

# 5. set_widget_property / set_widget_properties

## 5.1 工具定位

这组工具只负责：

```text
设置 Widget 普通属性。
```

例如：

```text
TextBlock.Text
Button.IsEnabled
Widget.Visibility
Image.Brush
ProgressBar.Percent
```

不负责：

```text
Slot 属性
父子结构
事件绑定
动画
Graph 逻辑
```

---

## 5.2 operation

单属性：

```json
"operation": "set_widget_property"
```

批量属性：

```json
"operation": "set_widget_properties"
```

---

## 5.3 target

```json
"target": {
  "asset_path": "/Game/UI/WBP_MainMenu",
  "widget_scope": "widget_property"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | Widget Blueprint 资产路径。 |
| `WidgetScope` | `EBlueprintHelperWidgetScope` | `target.widget_scope` | `string enum` | 是 | 固定为 `widget_property`。 |

---

## 5.4 单属性成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_property",
  "trace_id": "trace_20260503_2501",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_property"
  },

  "data": {
    "schema": "SetWidgetProperty.v1",
    "property_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
      "changed_count": 1,
      "no_op_count": 0
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

---

## 5.5 批量成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_properties",
  "trace_id": "trace_20260503_2502",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_property"
  },

  "data": {
    "schema": "SetWidgetProperty.v1",
    "property_result": {
      "mode": "batch",
      "requested_count": 3,
      "applied_count": 3,
      "changed_count": 2,
      "no_op_count": 1
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

成功不返回：

```text
invalid_settings
before
after
all_properties
widget_snapshot
property_paths
write_ref
transaction_id
```

---

## 5.6 property_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperPropertyWriteMode` | `data.property_result.mode` | `string enum` | 是 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.property_result.requested_count` | `number` | 是 | 请求设置数量。 |
| `AppliedCount` | `int32` | `data.property_result.applied_count` | `number` | 是 | 应用数量。 |
| `ChangedCount` | `int32` | `data.property_result.changed_count` | `number` | 是 | 实际变更数量。 |
| `NoOpCount` | `int32` | `data.property_result.no_op_count` | `number` | 是 | 无变化数量。 |

---

## 5.7 批量失败

批量 property 写默认事务化：

```text
只要存在 invalid 项，整批失败，不做部分应用。
```

失败不返回 property_result，只通过 error.conflicts 返回问题项。

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_properties",
  "trace_id": "trace_20260503_2503",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_property"
  },

  "error": {
    "code": "invalid_widget_property_settings",
    "stage": "validate_properties",
    "message": "One or more widget property settings are invalid.",
    "retryable": false,
    "conflicts": [
      {
        "code": "property_not_found",
        "widget_name": "TitleText",
        "property": "TextColorWrong"
      }
    ]
  }
}
```

---

# 6. set_widget_slot_property / set_widget_slot_properties

## 6.1 工具定位

这组工具只负责：

```text
设置 Widget 所在父容器 Slot 的属性。
```

例如：

```text
CanvasPanelSlot.Position
CanvasPanelSlot.Size
CanvasPanelSlot.Anchors
HorizontalBoxSlot.Padding
VerticalBoxSlot.Size
OverlaySlot.HorizontalAlignment
```

不负责 Widget 普通属性。

---

## 6.2 operation

```json
"operation": "set_widget_slot_property"
```

```json
"operation": "set_widget_slot_properties"
```

---

## 6.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_slot_properties",
  "trace_id": "trace_20260503_2601",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_slot"
  },

  "data": {
    "schema": "SetWidgetSlotProperty.v1",
    "property_result": {
      "mode": "batch",
      "requested_count": 4,
      "applied_count": 4,
      "changed_count": 4,
      "no_op_count": 0
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

字段映射沿用 `property_result`。

---

## 6.4 失败：Slot 类型不匹配

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_slot_property",
  "trace_id": "trace_20260503_2602",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_slot"
  },

  "error": {
    "code": "slot_type_mismatch",
    "stage": "validate_slot",
    "message": "The requested slot property is not valid for the widget's current parent slot type.",
    "retryable": false,
    "conflicts": [
      {
        "code": "slot_type_mismatch",
        "widget_name": "StartButton",
        "slot_type": "VerticalBoxSlot",
        "property": "Anchors"
      }
    ]
  }
}
```

---

# 7. remove_widget_from_tree

## 7.1 工具定位

`remove_widget_from_tree` 删除一个明确 WidgetTree 节点。

第一版只允许明确 widget_name / widget path，不支持模糊删除。

它不负责：

```text
删除绑定 Graph 逻辑
删除动画轨道
删除引用该 widget 的蓝图逻辑
级联清理外部引用
```

若存在引用，应 blocked 或失败。

---

## 7.2 operation

```json
"operation": "remove_widget_from_tree"
```

---

## 7.3 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_widget_from_tree",
  "trace_id": "trace_20260503_2701",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "data": {
    "schema": "RemoveWidgetFromTreeDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

---

## 7.4 dry_run blocked

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_widget_from_tree",
  "trace_id": "trace_20260503_2702",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "data": {
    "schema": "RemoveWidgetFromTreeDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "widget_has_bindings_or_references"
      ],
      "conflicts": [
        {
          "code": "widget_has_bindings_or_references",
          "widget_name": "StartButton",
          "message": "The widget is referenced by bindings or graph logic."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 7.5 正式成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_widget_from_tree",
  "trace_id": "trace_20260503_2703",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "data": {
    "schema": "RemoveWidgetFromTree.v1",
    "remove_widget_result": {
      "removed_count": 1
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

成功不返回 removed widget ref。

---

## 7.6 remove_widget_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `RemovedCount` | `int32` | `data.remove_widget_result.removed_count` | `number` | 是 | 删除 Widget 数量。 |

---

## 7.7 正式失败：widget 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_widget_from_tree",
  "trace_id": "trace_20260503_2704",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "widget_scope": "widget_tree"
  },

  "error": {
    "code": "widget_not_found",
    "stage": "resolve_widget",
    "message": "The requested widget was not found.",
    "retryable": false,
    "failed_item": {
      "type": "widget",
      "widget_name": "StartButton"
    }
  }
}
```

---

# 8. read_widget_tree

## 8.1 工具定位

`read_widget_tree` 返回压缩 WidgetTree 结构，不默认返回所有属性。

---

## 8.2 operation

```json
"operation": "read_widget_tree"
```

---

## 8.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_widget_tree",
  "trace_id": "trace_20260503_2801",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "read_scope": "widget_tree"
  },

  "data": {
    "schema": "ReadWidgetTree.v1",
    "widget_tree": {
      "root": "CanvasRoot",
      "widgets": [
        {
          "widget_name": "CanvasRoot",
          "widget_class": "CanvasPanel",
          "children": [
            "TitleText",
            "StartButton"
          ]
        },
        {
          "widget_name": "TitleText",
          "widget_class": "TextBlock",
          "children": []
        },
        {
          "widget_name": "StartButton",
          "widget_class": "Button",
          "children": []
        }
      ]
    }
  }
}
```

---

## 8.4 widget_tree 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Root` | `FString` | `data.widget_tree.root` | `string` | 是 | 根 Widget 名。 |
| `Widgets` | `TArray<FBlueprintHelperWidgetTreeItem>` | `data.widget_tree.widgets` | `array<object>` | 是 | 压缩 Widget 列表。 |

`widgets[]` 字段：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `WidgetName` | `FString` | `data.widget_tree.widgets[].widget_name` | `string` | 是 | Widget 名。 |
| `WidgetClass` | `FString` | `data.widget_tree.widgets[].widget_class` | `string` | 是 | Widget class。 |
| `Children` | `TArray<FString>` | `data.widget_tree.widgets[].children` | `array<string>` | 是 | 子 Widget 名列表。 |

默认不返回：

```text
all properties
slot properties
bindings
animations
graph references
```

---

# 9. read_widget_properties

## 9.1 operation

```json
"operation": "read_widget_properties"
```

---

## 9.2 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_widget_properties",
  "trace_id": "trace_20260503_2802",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "read_scope": "widget_property"
  },

  "data": {
    "schema": "ReadWidgetProperties.v1",
    "properties": {
      "widget_name": "TitleText",
      "property_count": 2,
      "values": {
        "Text": "Start Game",
        "Visibility": "Visible"
      }
    }
  }
}
```

---

# 10. read_widget_slot_properties

## 10.1 operation

```json
"operation": "read_widget_slot_properties"
```

---

## 10.2 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_widget_slot_properties",
  "trace_id": "trace_20260503_2803",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/UI/WBP_MainMenu",
    "read_scope": "widget_slot"
  },

  "data": {
    "schema": "ReadWidgetSlotProperties.v1",
    "slot_properties": {
      "widget_name": "StartButton",
      "slot_type": "CanvasPanelSlot",
      "property_count": 3,
      "values": {
        "Position": [100, 200],
        "Size": [300, 80],
        "ZOrder": 1
      }
    }
  }
}
```

---

# 11. validation

UMG 写工具成功保留：

```json
"validation": {
  "should_compile": true,
  "should_save": true,
  "compiled": false,
  "saved": false
}
```

普通属性 / Slot 属性通常：

```json
"validation": {
  "should_compile": false,
  "should_save": true,
  "compiled": false,
  "saved": false
}
```

read 工具不返回 validation。

---

# 12. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperWidgetErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperWidgetStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

---

# 13. UE 侧建议结构体

```cpp
struct FBlueprintHelperAddWidgetResultData
{
    FString Schema; // AddWidgetToTree.v1
    FBlueprintHelperAddWidgetResult AddWidgetResult;
};

struct FBlueprintHelperAddWidgetResult
{
    int32 AddedCount = 0;
    bool bReusedExisting = false; // no_op only
};

struct FBlueprintHelperWidgetPropertyResultData
{
    FString Schema; // SetWidgetProperty.v1 or SetWidgetSlotProperty.v1
    FBlueprintHelperPropertyWriteResult PropertyResult;
};

struct FBlueprintHelperPropertyWriteResult
{
    FString Mode; // single | batch
    int32 RequestedCount = 0;
    int32 AppliedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;
};

struct FBlueprintHelperRemoveWidgetResultData
{
    FString Schema; // RemoveWidgetFromTree.v1
    FBlueprintHelperRemoveWidgetResult RemoveWidgetResult;
};

struct FBlueprintHelperRemoveWidgetResult
{
    int32 RemovedCount = 0;
};

struct FBlueprintHelperReadWidgetTreeResultData
{
    FString Schema; // ReadWidgetTree.v1
    FBlueprintHelperWidgetTreeSummary WidgetTree;
};

struct FBlueprintHelperWidgetTreeSummary
{
    FString Root;
    TArray<FBlueprintHelperWidgetTreeItem> Widgets;
};

struct FBlueprintHelperWidgetTreeItem
{
    FString WidgetName;
    FString WidgetClass;
    TArray<FString> Children;
};

struct FBlueprintHelperReadWidgetPropertiesResultData
{
    FString Schema; // ReadWidgetProperties.v1
    FBlueprintHelperWidgetProperties Properties;
};

struct FBlueprintHelperReadWidgetSlotPropertiesResultData
{
    FString Schema; // ReadWidgetSlotProperties.v1
    FBlueprintHelperWidgetSlotProperties SlotProperties;
};
```

明确不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
FString WidgetRef
FString WidgetPath
FString ParentRef
FString SlotRef
```

---

# 14. 验收标准

```text
1. add_widget_to_tree 成功只返回 added_count。
2. add_widget_to_tree 成功不返回 widget_ref / widget_path / parent_ref。
3. name_collision 只支持 fail_if_exists / reuse_if_exists。
4. set_widget_property / set_widget_properties 成功只返回 property_result 计数。
5. property_result 成功不返回 invalid_settings。
6. property 批量写 invalid 时整批失败。
7. slot property 与普通 property 分离。
8. remove_widget_from_tree 必须 dry_run。
9. remove 成功只返回 removed_count。
10. blocked / failed 才返回 widget_name / ref。
11. UMG 写工具成功不返回 write_ref / transaction_id / review / safety。
12. UMG 写工具成功保留 validation。
13. read_widget_tree 返回压缩树结构。
14. read_widget_tree 不默认返回所有属性。
15. read_widget_properties / read_widget_slot_properties 独立读取属性值。
16. 所有 UMG data.schema 使用短命名。
