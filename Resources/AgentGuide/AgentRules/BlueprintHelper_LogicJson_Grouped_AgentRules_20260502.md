# BlueprintHelper Agent 侧规则：LogicJson 分组使用规范

日期：2026-05-02  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：LogicJson 分组规则确认稿  
本文边界：只规定 Agent 如何调用和解释 LogicJson，尤其是 groups、node_ref、link_ref、node 内 links 和 path 反推规则。UE 字段映射见独立 UE 侧文档。

---

## 1. LogicJson 的职责

LogicJson 是 Agent 的结构化蓝图逻辑读取格式，用于：

```text
1. PatchBlueprintGraph 前定位节点、Pin、默认值或连接。
2. MergeBlueprintGraph 前理解执行流接入点。
3. ReplaceBlueprintGraph 前生成 replace plan。
4. Cleanup 前识别 block、group 和 owned 节点范围。
5. 在 LogicMD 不够精确时进行结构化分析。
```

LogicJson 不用于：

```text
1. 直接导入蓝图。
2. 完整保真备份。
3. 替代 RawJson。
4. 默认展开 UE 原始 K2Node / Pin 底层字段。
```

LogicJson 必须：

```json
"importable": false
```

---

## 2. 多入口 scope 使用 groups[]

Agent 必须理解：

```text
scope=target_graph / blueprint / multi_target 时，LogicJson 使用 groups[]。
```

这些 scope 不是单入口：

```text
target_graph
blueprint
multi_target
```

Agent 不应期待：

```json
{
  "logic": {
    "entry": {},
    "nodes": []
  }
}
```

而应读取：

```json
{
  "logic": {
    "groups": []
  }
}
```

---

## 3. 单入口 scope 使用 entry + nodes

以下 scope 可以使用单入口简写：

```text
target_block
target_function
target_event
target_custom_event
target_node
target_pin
```

结构：

```json
{
  "logic": {
    "entry": {},
    "nodes": []
  }
}
```

Agent 应按当前 scope 判断结构，不应强制要求所有 LogicJson 都使用 groups[]。

---

## 4. group 解释规则

group 表示 UE 侧预先分类出的逻辑分组。

常见 group_type：

```text
blueprinthelper_block
user_region
global_event_flow
function_like_region
orphan_group
unknown
```

Agent 应理解：

| group_type | Agent 解释 |
|---|---|
| `blueprinthelper_block` | BlueprintHelper-owned block，通常可用于 Patch / Replace / Cleanup。 |
| `user_region` | 用户已有逻辑区域，修改风险更高。 |
| `global_event_flow` | BeginPlay / Tick / Overlap / InputAction 等全局事件流。Merge 通常涉及这里。 |
| `function_like_region` | 图表中可识别为独立调用链的区域。 |
| `orphan_group` | 孤立节点区域，需要谨慎处理。 |
| `unknown` | 无法稳定分类，修改前应进一步读取或 stop_and_report。 |

---

## 5. entry 解释规则

每个 group 必须有 entry：

```json
{
  "entry": {
    "kind": "custom_event",
    "name": "OpenPhysicsDoor",
    "node_path": "$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]",
    "node_ref": "nodes[OpenPhysicsDoor]"
  }
}
```

Agent 必须理解：

```text
1. entry.node_path 是完整节点路径。
2. entry.node_path 是当前 group 的反推锚点。
3. entry.node_path 不会是 $.graphs[GraphName] 这种非节点路径。
4. entry.node_ref 是当前 group 内的局部引用。
```

---

## 6. node_ref 规则

普通 node 默认只返回：

```json
"node_ref": "nodes[SetSimulatePhysics0]"
```

不默认返回完整 `node_path`。

Agent 如需完整 node_path，应按规则反推：

```text
完整 node_path =
group.entry.node_path 去掉最后一段 nodes[EntryName]
+
node_ref
```

示例：

```text
group.entry.node_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]

node_ref:
nodes[SetSimulatePhysics0]

完整 node_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[SetSimulatePhysics0]
```

Agent 不得把 `node_ref` 当成全局唯一路径。  
`node_ref` 只在当前 group 内有效。

---

## 7. link_ref 规则

link 默认只返回：

```json
"link_ref": "links[1]"
```

不默认返回完整 `link_path`。

Agent 如需完整 link_path，应按规则反推：

```text
完整 link_path =
group.entry.node_path 去掉最后一段 nodes[EntryName]
+
link_ref
```

示例：

```text
group.entry.node_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]

link_ref:
links[1]

完整 link_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].links[1]
```

`link_ref` 只在当前 group 内有效。

---

## 8. links 移入 source node

LogicJson 不返回顶层：

```json
"links": []
```

links 放在 source node 内：

```json
{
  "node_ref": "nodes[SetSimulatePhysics0]",
  "links": [
    {
      "link_ref": "links[1]",
      "type": "exec",
      "from_pin": "Then",
      "to_node": "nodes[AddImpulse0]",
      "to_pin": "Execute"
    }
  ]
}
```

Agent 应理解：

```text
1. node.links 表示从当前 node 发出的 outgoing links。
2. link 内不写 from_node，因为 source node 就是当前 node。
3. to_node 是目标 node_ref。
4. to_node 只在当前 group 内有效。
```

---

## 9. 执行流阅读规则

Agent 阅读执行流时应从 entry 开始：

```text
entry.node_ref
→ 当前 node.links
→ to_node
→ 目标 node.links
→ ...
```

这比顶层 links 更适合 Agent 连贯理解。

如果 Agent 需要查询某个 node 的输入来源，需要反查其他 node 的 outgoing links。第一版 LogicJson 不提供 incoming_refs。

---

## 10. kind 规则

LogicJson 默认返回语义 kind：

```text
event
custom_event
call_function
branch
sequence
variable_get
variable_set
component_get
literal
return
macro
delegate_bind
delegate_call
timeline
unknown
```

Agent 不应期待默认出现 UE 原始类名：

```text
K2Node_CallFunction
K2Node_IfThenElse
K2Node_VariableGet
```

UE 原始类名属于 RawJson 或未来 debug / pin_detail 模式。

---

## 11. stats 解释规则

多入口 scope 下 stats 可能包含：

```json
{
  "groups": 3,
  "events": 3,
  "nodes": 21,
  "exec_links": 18,
  "data_links": 6,
  "orphan_nodes": 0
}
```

Agent 应按 scope 解读：

```text
1. groups 只在多入口 scope 下出现。
2. target_graph 下不会返回 graphs / functions。
3. target_function 下不会返回 graphs / functions / events。
4. 不存在的 stats 字段表示不适用于当前 scope，不表示值为 0。
```

---

## 12. Patch / Merge 使用规则

Agent 在 Patch / Merge 前应：

```text
1. 读取相关 target 的 LogicJson。
2. 使用 group / entry / node_ref / link_ref 定位目标。
3. 如工具需要完整 path，则按反推规则构造 node_path / link_path。
4. 不使用显示名作为唯一定位依据。
5. 遇到 user_region / global_event_flow 修改时按高风险处理。
```

Merge 特别规则：

```text
1. 接入已有执行流通常涉及 global_event_flow 或 user_region。
2. Agent 不得用 Append 替代 Merge 接入已有执行流。
3. target pin / insert_strategy 仍必须明确。
```

---

## 13. Agent 禁止行为

Agent 不得：

```text
1. 把 node_ref 当成全局路径。
2. 跨 group 使用 node_ref / link_ref。
3. 期待顶层 logic.links。
4. 期待 link 内有 from_node。
5. 期待普通 node 默认有完整 node_path。
6. 把 LogicJson 当作可导入格式。
7. 使用 display name 作为唯一定位依据。
8. 把 user_region 当作 BlueprintHelper-owned block。
```

---

## 14. Agent 最终报告规则

正常任务完成时，Agent 不需要展开完整 LogicJson。

只有以下情况需要提及：

```text
1. 用户要求结构化审查。
2. 目标定位不唯一。
3. 出现 user_region / global_event_flow 高风险修改。
4. stop_and_report 需要说明已读取的结构化信息。
5. Patch / Merge 失败需要说明定位点。
```

---

## 15. Agent 侧验收标准

```text
1. Agent 能识别 groups[]。
2. Agent 能识别单入口 scope 的 entry + nodes。
3. Agent 能按 group.entry.node_path 反推 node_path。
4. Agent 能按 group.entry.node_path 反推 link_path。
5. Agent 理解 node.links 是 outgoing links。
6. Agent 不期待顶层 logic.links。
7. Agent 不期待 link.from_node。
8. Agent 不跨 group 使用 node_ref / link_ref。
9. Agent 不把 LogicJson 当作可导入格式。
10. Agent 在精确编辑前使用 LogicJson 而不是 LogicMD。
