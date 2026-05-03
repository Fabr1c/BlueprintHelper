# BlueprintHelper Agent 侧规则：LogicMD 分组使用规范

日期：2026-05-02  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：LogicMD 分组规则确认稿  
本文边界：只规定 Agent 如何调用和解释 LogicMD，尤其是 `target_graph / blueprint / multi_target` 下的分组语义。UE 字段映射见独立 UE 侧文档。

---

## 1. LogicMD 的职责

LogicMD 是 Agent 默认的蓝图逻辑阅读格式。

Agent 应优先使用 LogicMD 来理解：

```text
1. 蓝图大致做了什么。
2. 图表中有哪些入口事件。
3. 函数 / 事件 / block 之间的大致调用关系。
4. 哪些逻辑属于 BlueprintHelper-owned block。
5. 哪些逻辑属于用户已有区域。
6. 是否存在孤立节点或执行流异常。
```

LogicMD 不用于：

```text
1. 精确 Patch 节点。
2. 精确 Merge 执行流。
3. 精确定位 Pin。
4. 导入 / 还原蓝图。
5. 完整保真备份。
```

---

## 2. 多入口 scope 的分组语义

Agent 必须理解：

```text
scope=target_graph 并不是单个入口。
它表示 UE 侧遍历当前 graph 内的所有 block / entry / user region。
```

因此：

```text
target_graph / blueprint / multi_target 是多入口 scope。
这些 scope 的 LogicMD 应按 group 分段。
```

多入口 scope 包括：

```text
target_graph
blueprint
multi_target
```

单入口 scope 包括：

```text
target_block
target_function
target_event
target_custom_event
```

---

## 3. grouped 字段规则

多入口 scope 下，LogicMD 返回：

```json
{
  "grouped": true
}
```

Agent 应解释为：

```text
1. Markdown 内容已按 group 分段。
2. 每个分段代表一个 block、用户区域、全局事件流、孤立组或未知区域。
3. 不应把整个 markdown 当作单条连续执行流。
```

单入口 scope 不返回 `grouped` 字段。  
Agent 不应期待：

```json
"grouped": false
```

字段不存在表示：

```text
当前 scope 不需要分组字段，通常是单入口读取。
```

---

## 4. Markdown 分段解释规则

多入口 scope 下，Markdown 可能类似：

```md
EG_PhysicsDoor

[BlueprintHelper Block] EG_PhysicsDoor_TogglePhysicsDoor0
TogglePhysicsDoor() -> Branch(bDoorOpen)
  false -> OpenPhysicsDoor()
  true -> ClosePhysicsDoor()

[BlueprintHelper Block] EG_PhysicsDoor_OpenPhysicsDoor0
OpenPhysicsDoor() -> DoorMesh.SetSimulatePhysics(true) -> DoorMesh.AddImpulse(...)

[User Region] UserExistingBeginPlayFlow
BeginPlay() -> ExistingInit()
```

Agent 应理解：

```text
1. [BlueprintHelper Block] 表示 BlueprintHelper-owned block。
2. [User Region] 表示用户已有逻辑区域。
3. [Global Event Flow] 表示 BeginPlay / Tick / Overlap / InputAction 等全局事件流。
4. [Orphan Group] 表示孤立节点区域。
5. 不同 group 不应被自动视作同一条执行链。
```

---

## 5. stats 解释规则

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

Agent 应按以下规则解释：

```text
1. groups 表示当前 scope 被 UE 侧分成多少个逻辑分组。
2. target_graph 下不会返回 graphs / functions。
3. target_function 下不会返回 graphs / functions / events。
4. 不存在的 stats 字段表示不适用于当前 scope，不表示值为 0。
```

---

## 6. target_graph 读取规则

当 `scope=target_graph`：

```text
Agent 不得假设读取结果是一条扁平执行流。
Agent 应按 Markdown 中的 group 分段理解 graph。
Agent 若要修改某个 group，应继续读取该 group 对应的 LogicJson。
```

示例：

```text
用户要求修改 OpenPhysicsDoor。
Agent 先读 target_graph LogicMD。
发现 [BlueprintHelper Block] EG_PhysicsDoor_OpenPhysicsDoor0。
下一步应读取 target_block LogicJson，而不是直接 Patch。
```

---

## 7. LogicMD 到 LogicJson 的升级规则

Agent 应在以下情况从 LogicMD 升级到 LogicJson：

```text
1. 需要 PatchBlueprintGraph。
2. 需要 MergeBlueprintGraph。
3. 需要 ReplaceBlueprintGraph 的精确计划。
4. 需要定位 node_ref / link_ref / node_path。
5. LogicMD 中发现多个 group，需要选择具体 block。
6. LogicMD 中发现用户区域，准备做风险判断。
7. 目标函数 / 图表存在歧义。
```

---

## 8. Agent 禁止行为

Agent 不得：

```text
1. 把 target_graph LogicMD 当作单一入口执行流。
2. 把不同 group 自动串成一条逻辑链。
3. 直接根据 LogicMD 执行 Pin 级 Patch。
4. 将 LogicMD 传给导入工具。
5. 把 grouped 字段缺失理解为 false 值。
6. 把缺失的 stats 字段理解为 0。
```

---

## 9. 最终报告规则

正常完成任务时，Agent 不需要展开完整 LogicMD。

只有以下情况需要提及：

```text
1. 用户要求总结蓝图逻辑。
2. LogicMD 显示目标不存在或逻辑异常。
3. Agent stop_and_report，需要说明已读取哪些只读信息。
4. 用户要求审查当前蓝图结构。
5. 多个 group 影响任务范围选择，需要解释选择了哪个 group。
```

---

## 10. Agent 侧验收标准

```text
1. Agent 默认优先读取 LogicMD 理解蓝图。
2. Agent 能识别 grouped=true。
3. Agent 能按 group 分段阅读 target_graph / blueprint / multi_target。
4. Agent 不把 target_graph LogicMD 当作单入口逻辑。
5. Agent 需要精确编辑时继续读取 LogicJson。
6. Agent 不根据 LogicMD 直接执行 Pin 级 Patch。
7. Agent 不把 LogicMD 当作可导入格式。
8. Agent 不在最终报告中默认展开完整 LogicMD。
```
