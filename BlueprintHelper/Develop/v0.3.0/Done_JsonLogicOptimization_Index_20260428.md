# BlueprintHelper JSON / Agent 逻辑视图优化规划索引（2026-04-28）

## 文档目的

本组文档用于规划 BlueprintHelper 当前 MCP / Bridge 返回给 Agent 的 JSON 优化方案。重点不是替换现有 `BlueprintHelper.JsonToBlueprint` 可回放协议，而是在其上新增一层面向 Agent 理解、压缩和诊断的逻辑视图。

现有导出 JSON 同时承担两类职责：

1. **可回放协议**：用于 Json -> Blueprint 的还原、增量修改和测试 fixture。
2. **Agent 读图协议**：用于让 IDE / CLI Agent 理解蓝图执行逻辑。

这两类职责的字段需求不同。可回放协议需要完整节点、Pin、GUID、坐标和结构细节；Agent 读图则更需要事件入口、执行流、分支、循环、变量读写、函数调用和数据依赖摘要。

因此本组文档提出新增 `LogicProcessor` 派生层，保留原始 JSON 的完整性，同时给 MCP 工具新增短逻辑 JSON / Markdown 输出。

---

## 文档列表

### 1. 总览与原因

[Module_BlueprintHelper_JsonLogicOptimization_Overview_20260428.md](Module_BlueprintHelper_JsonLogicOptimization_Overview_20260428.md)

说明为什么现有 MCP JSON 返回需要优化、哪些问题不应通过简单删字段解决，以及整体目标边界。

### 2. LogicProcessor 模块设计

[Module_BlueprintHelper_LogicProcessor_Design_20260428.md](Module_BlueprintHelper_LogicProcessor_Design_20260428.md)

定义 `FBlueprintHelperLogicProcessor` 的职责、输入输出、内部中间图结构、节点语义分类和 Markdown / Logic JSON 生成策略。

### 3. Bridge / MCP 返回协议优化

[Module_BlueprintHelper_MCP_ReturnProtocol_Optimization_20260428.md](Module_BlueprintHelper_MCP_ReturnProtocol_Optimization_20260428.md)

规划新增 `export_logic` 命令或扩展 `export_to_json` 的 `format` 参数，规范 `raw_json`、`logic_json`、`logic_md` 三类返回。

### 4. Raw JSON Schema 兼容增强

[Module_BlueprintHelper_RawJsonSchema_CompatibilityPlan_20260428.md](Module_BlueprintHelper_RawJsonSchema_CompatibilityPlan_20260428.md)

规划在不破坏导入兼容的前提下，为 links、pins、graphs 增加可选语义字段，例如 `kind`、`from_pin_type`、`to_pin_type`。

### 5. 实施计划与验收标准

[Module_BlueprintHelper_LogicOptimization_ImplementationPlan_20260428.md](Module_BlueprintHelper_LogicOptimization_ImplementationPlan_20260428.md)

给出分阶段实现顺序、文件落点、接口草案、风险控制和验收标准。

### 6. 测试与 Fixture 计划

[Module_BlueprintHelper_LogicOptimization_TestPlan_20260428.md](Module_BlueprintHelper_LogicOptimization_TestPlan_20260428.md)

规划用于验证逻辑摘要正确性的测试 fixture、回归用例和 Agent 侧验证方法。

---

## 推荐阅读顺序

1. 先读总览与原因，确认优化边界。
2. 再读 LogicProcessor 模块设计，确认核心实现方式。
3. 再读 Bridge / MCP 返回协议优化，确认外部调用方式。
4. 再读 Raw JSON Schema 兼容增强，确认不会破坏现有导入协议。
5. 最后读实施计划和测试计划，进入开发落地。

---

## 总体结论

推荐将 BlueprintHelper 的导出能力拆成三种视图：

| 视图 | 目标用户 | 主要用途 | 是否可导入回放 |
|------|----------|----------|----------------|
| `raw_json` | 插件、测试、Agent 写回链路 | 完整还原蓝图结构 | 是 |
| `logic_json` | Agent / IDE 工具 | 结构化理解执行逻辑 | 否 |
| `logic_md` | Agent / 人类审阅 | 快速阅读和诊断 | 否 |

最终目标链路：

```text
UEdGraph / Blueprint
        ↓
FBlueprintToTextConverter
        ↓
raw BlueprintHelper.JsonToBlueprint JSON
        ↓
FBlueprintHelperLogicProcessor
        ↓
logic_json / logic_md
        ↓
Bridge / MCP Tool 返回给 Agent
```

