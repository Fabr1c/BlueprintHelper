# BlueprintHelper Agent 导入 JSON 优化变更文档（2026-04-30）

## 一、变更摘要

本次变更规划新增一套面向 Agent 的蓝图导入 JSON 协议：

```text
BlueprintHelper.AgentImportGraph
```

该协议与现有 `BlueprintHelper.JsonToBlueprint` 并存。

核心变更：

1. Agent 导入 JSON 不再要求 `Pos`、GUID、完整 Pin 列表等底层字段。
2. 新增语义节点格式，例如 `event`、`call`、`get`、`set`、`branch`。
3. 新增自动布局能力，由插件负责蓝图节点摆放。
4. 新增 Bridge 命令 `import_agent_graph`。
5. 新增 MCP 工具 `blueprint_import_agent_graph`。
6. 保留原 `blueprint_import_json` 用于 raw JSON 回放。

---

## 二、修改原因

### 2.1 降低 Agent 生成失败率

raw JSON 需要 Agent 生成大量 UE 底层细节，例如节点坐标、Pin、GUID、节点类型。任何字段不匹配都可能导致导入失败。

新增 Agent 导入协议后，Agent 只需要描述：

```text
目标蓝图、目标图表、节点语义、输入值、执行连线、数据连线。
```

其余细节由插件处理。

### 2.2 降低 token 成本

删除以下字段可明显减少 JSON 体积：

```text
Pos / PosX / PosY
NodeGuid / PinGuid / GraphGuid
完整 pins 数组
节点尺寸
编辑器显示状态
编译缓存字段
```

### 2.3 提升蓝图布局一致性

如果坐标由 Agent 随机生成，图表可读性不可控。

将布局下放给插件后，可以统一执行流方向、分支间距、数据节点靠近规则和注释框范围。

### 2.4 保持 raw JSON 职责清晰

raw JSON 继续用于：

- 完整导出。
- 完整导入。
- fixture 回放。
- 调试底层节点结构。
- 兼容旧工具。

AgentImportGraph 用于：

- Agent 创建新蓝图逻辑。
- Agent 追加小段逻辑。
- Agent 以更低 token 成本提交构图意图。

---

## 三、变更范围

### 3.1 新增内容

| 类型 | 名称 | 说明 |
|------|------|------|
| Schema | `BlueprintHelper.AgentImportGraph` | Agent 导入协议 |
| Bridge 命令 | `import_agent_graph` | 导入 Agent 语义 JSON |
| MCP 工具 | `blueprint_import_agent_graph` | Agent-facing 写入工具 |
| Service | `FBlueprintHelperAgentImportProcessor` | 解析和执行导入 |
| Validator | AgentImport validator | 校验字段、节点、连线 |
| Layout | Graph auto layout | 自动摆放节点 |
| Fixture | `Resources/TestFixtures/AgentImport/` | 回归测试 |

### 3.2 修改内容

| 模块 | 修改 |
|------|------|
| Bridge Router | 注册 `import_agent_graph` 命令 |
| MCP Server | 新增工具映射 |
| Json 校验 | 增加 AgentImportGraph schema 识别 |
| 文档 | 增加设计、技术、变更文档 |

### 3.3 不修改内容

| 模块 | 原因 |
|------|------|
| `BlueprintHelper.JsonToBlueprint` raw schema | 避免破坏旧导入 |
| `blueprint_import_json` | 保持回放工具稳定 |
| 现有导出工具 | 读取优化已由 logic_json / logic_md 规划覆盖 |
| C++ 源码编辑能力 | 不属于 BlueprintHelper MCP 的职责 |

---

## 四、行为变化

### 4.1 变更前

Agent 若要导入蓝图，通常需要构造类似 raw JSON 的结构：

```json
{
  "type": "K2Node_CallFunction",
  "id": "Node_1",
  "PosX": 100,
  "PosY": 200,
  "pins": [...],
  "links": [...]
}
```

问题：

- 坐标需要 Agent 决定。
- Pin 容易缺失或错误。
- 节点类型耦合 UE 实现。
- JSON 体积大。

### 4.2 变更后

Agent 可以构造：

```json
{
  "id": "print_hello",
  "kind": "call",
  "function": "/Script/Engine.KismetSystemLibrary:PrintString",
  "inputs": {
    "InString": "Hello"
  }
}
```

插件负责创建真实 `K2Node_CallFunction`、补全 Pin、连接节点并自动布局。

---

## 五、兼容性说明

### 5.1 向后兼容

本变更是新增能力，不删除旧协议。

| 调用方 | 影响 |
|--------|------|
| 旧 MCP Agent | 无影响 |
| 旧 raw JSON fixture | 无影响 |
| `blueprint_import_json` | 无影响 |
| `validate_json` | 可扩展支持新 schema，但旧行为不变 |

### 5.2 向前兼容

`AgentImportGraph` 应支持版本字段：

```json
{
  "version": "1.0"
}
```

后续新增字段时遵循：

1. 新字段默认可选。
2. 旧字段不改变语义。
3. 不同导入模式通过 `mode` 区分。
4. patch 模式不影响 append 模式。

---

## 六、迁移策略

### 6.1 Agent 默认策略迁移

推荐 Agent 写蓝图时采用以下决策：

```text
创建或追加简单逻辑 -> blueprint_import_agent_graph
完整回放已有蓝图 -> blueprint_import_json
精确复制 raw fixture -> blueprint_import_json
需要理解现有蓝图 -> blueprint_get_logic / blueprint_get_logic_json
需要底层节点细节 -> blueprint_export_raw_json
```

### 6.2 文档迁移

新增文档后，应在规划索引中补充：

```text
Module_BlueprintHelper_AgentImportJson_Design_20260430.md
Module_BlueprintHelper_AgentImportJson_TechnicalSpec_20260430.md
Module_BlueprintHelper_AgentImportJson_ChangePlan_20260430.md
```

---

## 七、实施计划

### 阶段 1：协议与校验

目标：让插件能识别 `BlueprintHelper.AgentImportGraph`。

任务：

1. 新增 schema 常量。
2. 新增 JSON parser。
3. 新增字段校验。
4. 禁止或 warning `Pos`、GUID、完整 Pin 列表。
5. 支持 dry run。

验收：

- 最小 JSON 可通过校验。
- 缺少目标蓝图时报错。
- 重复节点 id 报错。
- 包含 `Pos` 时给 warning 或 strict error。

### 阶段 2：最小节点导入

目标：实现基本构图闭环。

支持节点：

```text
event
custom_event
call
get
set
branch
sequence
comment
```

支持连线：

```text
exec
data
```

验收：

- 可导入 `BeginPlay -> PrintString`。
- 可导入 `BeginPlay -> Set Variable`。
- 可导入 `Branch true/false`。
- 导入后蓝图可编译。

### 阶段 3：自动布局

目标：删除 Agent 坐标依赖。

任务：

1. 入口节点布局。
2. 执行流层级布局。
3. 分支 Y 轴分流。
4. 数据节点靠近消费节点。
5. 孤立节点区域。
6. 注释框包围盒。

验收：

- 导入 JSON 不含任何 Pos 字段。
- 生成蓝图节点有稳定坐标。
- 多次导入同一 JSON 的布局一致。

### 阶段 4：Bridge / MCP 集成

目标：Agent 可通过 MCP 调用新协议。

任务：

1. Bridge Router 新增 `import_agent_graph`。
2. MCP Server 新增 `blueprint_import_agent_graph`。
3. 响应中返回 created_nodes、created_links、warnings、compiled、saved。
4. 错误响应加入 path 和 suggestion。

验收：

- MCP 调用可完成导入。
- 错误 JSON 可返回结构化诊断。
- 旧 `blueprint_import_json` 不受影响。

### 阶段 5：扩展节点与 patch 准备

目标：覆盖更多蓝图场景。

扩展节点：

```text
switch
loop
cast
spawn_actor
bind_delegate
unbind_delegate
broadcast
make_struct
break_struct
timeline
```

patch 前置能力：

- 稳定节点匹配。
- 导入前 dry run diff。
- 与审阅面板集成。

---

## 八、测试与验收标准

### 8.1 功能验收

| 编号 | 场景 | 标准 |
|------|------|------|
| AIJ-001 | 最小 BeginPlay + PrintString | 导入成功，编译成功 |
| AIJ-002 | 不含 Pos 字段 | 自动生成稳定布局 |
| AIJ-003 | 变量声明 + set | 自动创建变量并连接 |
| AIJ-004 | Branch | True / False 分支正确 |
| AIJ-005 | 错误函数路径 | 返回 `UnknownFunction` |
| AIJ-006 | 错误 Pin | 返回 `InvalidLinkEndpoint` |
| AIJ-007 | dry_run | 不修改蓝图但返回计划 |
| AIJ-008 | save=false | 编译后不保存资产 |

### 8.2 回归验收

| 项目 | 标准 |
|------|------|
| raw JSON 导入 | 原有 fixture 全部通过 |
| MCP 旧工具 | 无 breaking change |
| logic_json / logic_md | 不被误标为 importable |
| AgentImportGraph | 不要求 GUID / Pos |

---

## 九、风险与控制

| 风险 | 等级 | 控制方式 |
|------|------|----------|
| Pin 名称匹配错误 | 中 | 提供 available pins 诊断 |
| 函数路径解析失败 | 中 | 支持 symbols 和候选建议 |
| 自动布局不美观 | 中 | 先保证稳定，再逐步优化 |
| patch 模式误删节点 | 高 | 第一阶段不实现 patch |
| 和 raw JSON 概念混淆 | 中 | schema、命令、工具名明确区分 |
| Agent 生成过度简略 | 中 | validator 返回缺失字段路径 |

---

## 十、回滚方案

由于本变更为新增协议，回滚方式简单：

1. 隐藏 MCP 工具 `blueprint_import_agent_graph`。
2. Bridge 停止注册 `import_agent_graph`。
3. 保留文档和 fixture，不影响旧工具。
4. 继续使用 `blueprint_import_json`。

不需要迁移旧数据，也不需要修改已有蓝图资产。

---

## 十一、最终变更结论

本变更应作为 BlueprintHelper 的新增导入能力实现。

推荐落地顺序：

```text
文档与 schema
  -> Validator
  -> event/call/get/set/branch 最小节点导入
  -> 自动布局
  -> Bridge / MCP 工具
  -> fixture / 回归测试
  -> 更多节点类型
  -> patch / 审阅面板集成
```

完成后，Agent 不再需要生成 `Pos`、GUID 和完整 Pin 列表，蓝图导入将从“底层节点快照导入”升级为“语义构图意图导入”。
