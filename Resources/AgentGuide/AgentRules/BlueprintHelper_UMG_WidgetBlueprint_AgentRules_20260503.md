# BlueprintHelper Agent 侧规则：UMG / Widget Blueprint 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：UMG / Widget Blueprint Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 UMG / Widget Blueprint 工具，包括 WidgetTree 增删、普通属性写入、Slot 属性写入、读工具、成功极简返回、错误定位和 validation 规则。UE 字段映射见独立文档。

---

## 1. 工具簇边界

第一版 UMG 工具簇覆盖：

```text
Widget Tree 结构写入
普通 Widget 属性写入
Slot 属性写入
Widget 删除
WidgetTree 读取
Widget 普通属性读取
Widget Slot 属性读取
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

## 2. 工具拆分规则

Agent 必须按职责选择工具：

```text
add_widget_to_tree：创建并插入 Widget。
set_widget_property / set_widget_properties：设置普通 Widget 属性。
set_widget_slot_property / set_widget_slot_properties：设置 Slot 属性。
remove_widget_from_tree：删除 WidgetTree 节点。
read_widget_tree：读取压缩 WidgetTree。
read_widget_properties：读取普通属性。
read_widget_slot_properties：读取 Slot 属性。
```

Agent 不应让 add_widget_to_tree 同时设置普通属性或 Slot 属性。

---

## 3. data.schema 短命名

UMG 工具使用短命名：

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

Agent 不应期待 BlueprintHelper / MCP / Tools 前缀。

---

# 4. add_widget_to_tree

## 4.1 职责

`add_widget_to_tree` 只负责：

```text
创建 Widget 并插入明确父级。
```

它不负责：

```text
普通属性设置
Slot 属性设置
绑定事件
创建动画
写 Graph 逻辑
```

---

## 4.2 成功返回解释

成功返回：

```json
{
  "add_widget_result": {
    "added_count": 1
  }
}
```

Agent 只读取：

```text
added_count
validation
```

Agent 不应期待：

```text
widget_name
widget_ref
widget_path
parent_ref
slot_ref
widget_tree
write_ref
transaction_id
```

成功后如果需要确认树结构，应调用：

```text
read_widget_tree
```

---

## 4.3 name_collision

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

如果 `fail_if_exists` 冲突，工具失败，并在 error.conflicts 返回 widget_name。

如果 `reuse_if_exists` 命中已有 Widget，返回：

```text
status=no_op
added_count=0
reused_existing=true
```

---

# 5. set_widget_property / set_widget_properties

## 5.1 职责

这组工具只设置普通 Widget 属性。

不设置：

```text
Slot 属性
父子结构
事件绑定
动画
Graph 逻辑
```

---

## 5.2 成功返回解释

单属性成功：

```json
{
  "property_result": {
    "mode": "single",
    "requested_count": 1,
    "applied_count": 1,
    "changed_count": 1,
    "no_op_count": 0
  }
}
```

批量成功：

```json
{
  "property_result": {
    "mode": "batch",
    "requested_count": 3,
    "applied_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  }
}
```

Agent 不应期待：

```text
invalid_settings
before
after
all_properties
property_paths
widget_snapshot
```

---

## 5.3 批量事务化

批量 property 写默认事务化：

```text
只要存在 invalid 项，整批失败，不做部分应用。
```

失败时不返回 property_result，只通过 error.conflicts 返回问题项。

---

# 6. set_widget_slot_property / set_widget_slot_properties

## 6.1 职责

这组工具只设置 Widget 所在父容器 Slot 的属性。

例如：

```text
CanvasPanelSlot.Position
CanvasPanelSlot.Size
CanvasPanelSlot.Anchors
HorizontalBoxSlot.Padding
VerticalBoxSlot.Size
OverlaySlot.HorizontalAlignment
```

普通 Widget 属性必须使用 set_widget_property / set_widget_properties。

---

## 6.2 成功返回解释

Slot 属性写成功也使用 property_result 计数。

Agent 不能用普通属性工具设置 Slot 属性，也不能用 Slot 属性工具设置普通 Widget 属性。

---

## 6.3 Slot 类型错误

如果 Slot 属性不适用于当前 Slot 类型，工具失败，并在 error.conflicts 返回：

```text
widget_name
slot_type
property
```

---

# 7. remove_widget_from_tree

## 7.1 职责

`remove_widget_from_tree` 删除一个明确 WidgetTree 节点。

第一版不支持模糊删除。

不负责：

```text
删除绑定 Graph 逻辑
删除动画轨道
删除引用该 widget 的蓝图逻辑
级联清理外部引用
```

---

## 7.2 必须 dry_run

remove_widget_from_tree 是破坏性操作，必须 dry_run。

dry_run passed：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
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
```

---

## 7.3 成功返回解释

成功返回：

```json
{
  "remove_widget_result": {
    "removed_count": 1
  }
}
```

Agent 不应期待 removed widget ref。

错误 / blocked 时才读取 widget_name / ref。

---

# 8. read_widget_tree

read_widget_tree 返回压缩 WidgetTree：

```json
{
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
      }
    ]
  }
}
```

Agent 可用它理解树结构和后续定位。

默认不返回：

```text
所有属性
Slot 属性
绑定
动画
Graph 引用
```

---

# 9. read_widget_properties / read_widget_slot_properties

普通属性读取使用：

```text
read_widget_properties
```

Slot 属性读取使用：

```text
read_widget_slot_properties
```

这两个读工具允许返回 values，因为读取属性是它们的职责。

写工具不回显 before / after。

---

# 10. validation

UMG 写工具成功保留 validation：

```text
should_compile
should_save
compiled
saved
```

原因：

```text
WidgetTree 结构变化可能需要 compile/save。
属性变化通常至少需要 save。
```

读工具不返回 validation。

---

# 11. 不返回事务信息

UMG 写工具成功不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

事务、Journal、Review 是 UE 侧内部审计系统。

---

# 12. Agent 禁止行为

Agent 不得：

```text
1. 用 add_widget_to_tree 同时设置普通属性。
2. 用普通属性工具设置 Slot 属性。
3. 用 Slot 属性工具设置普通属性。
4. 期待 add_widget_to_tree 成功返回 widget_ref。
5. 期待 property 写成功返回 before / after。
6. 在 remove_widget_from_tree 未 dry_run passed 前正式删除。
7. 忽略 widget_has_bindings_or_references。
8. 期待 UMG 写工具返回 transaction_id。
9. 期待 read_widget_tree 返回所有属性。
```

---

# 13. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 新增 / 删除 / 修改了几个 Widget 或属性。
2. 是否需要 compile / save。
3. 如果失败，报告 widget_name / property / slot_type 等错误定位信息。
```

不默认报告：

```text
transaction_id
review_status
widget_path
完整 WidgetTree
before / after
```

---

# 14. 验收标准

```text
1. Agent 能区分 WidgetTree / 普通属性 / Slot 属性工具。
2. Agent 能解析 added_count。
3. Agent 能处理 name_collision。
4. Agent 能解析 property_result。
5. Agent 知道批量属性写是事务化的。
6. Agent 能处理 remove dry_run。
7. Agent 能解析 removed_count。
8. Agent 能使用 read_widget_tree 理解压缩树结构。
9. Agent 不期待 UMG 写工具返回 transaction_id。
10. Agent 不期待写工具回显 before / after。
