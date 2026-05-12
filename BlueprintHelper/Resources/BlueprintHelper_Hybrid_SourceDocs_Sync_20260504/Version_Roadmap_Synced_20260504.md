可以。建议把版本线统一为 **“数据表达 → 编辑器接入 → Agent 可用性优化”** 这条主线。

已有的规划文档已经把 raw JSON / LogicJson / LogicMD、LogicProcessor、MCP 返回协议、测试验收等职责拆开，这个版本口径可以直接作为 README、CHANGELOG、Release Notes、测试文档的统一描述基础。fileciteturn0file0 现有实战测试用例也已经按 MCP 连通性、蓝图操作、LogicJson/LogicMD、UMG、DataAsset/DataTable、回滚异常、性能 Token 等方向拆分，适合映射到版本验收范围。fileciteturn0file1

## BlueprintHelper 版本统一定义

| 版本 | 版本定位 | 核心目标 | 对外描述 |
|---|---|---|---|
| **v0.1.0** | JSON 蓝图表达基础版 | 建立蓝图与 JSON 的互相转换能力 | 实现蓝图结构的 JSON 导出/导入，为后续 Agent 理解和生成蓝图提供数据基础 |
| **v0.2.0** | MCP 编辑器接入版 | 让 Agent 可以通过 MCP 访问 Unreal Editor | 实现 Agent 对编辑器资产、蓝图、UMG、DataAsset、DataTable 等对象的基础访问与编辑能力 |
| **v0.3.0** | Agent 稳定性与通信优化版 | 提高 Agent 实际操作稳定性，降低上下文与通信成本 | 引入 LogicJson / LogicMD / Agent Import 等优化路径，减少 RawJson Token 消耗，提高蓝图读取、修改、导入的稳定性 |

---

## 推荐统一口径

### v0.1.0：JSON 蓝图转换基础版

**一句话定位：**

> v0.1.0 建立 BlueprintHelper 的蓝图 JSON 表达基础，实现蓝图与 JSON 的基本互相转换能力。

**功能边界：**

- 蓝图结构导出为 RawJson。
- JSON 重新导入或还原为蓝图结构。
- 覆盖大部分常见蓝图节点、Pin、Link、图表结构。
- 重点解决“蓝图如何被文本化表示”的问题。
- 不强调 Agent 编辑器操作能力。
- 不强调 Token 优化。
- 不强调稳定性工作流。

**不要这样描述：**

> v0.1.0 实现 Agent 操作蓝图。

这个说法过早。v0.1.0 更准确是 **数据表达层**，不是 Agent 操作层。

---

### v0.2.0：Agent 编辑器访问基础版

**一句话定位：**

> v0.2.0 在 JSON 表达基础上，通过 MCP 和 UE Bridge 建立 Agent 访问、查询、编辑 Unreal Editor 的基础能力。

**功能边界：**

- MCP Server 基础工具集。
- Agent 可访问 UE 编辑器。
- 资产浏览、搜索、打开、保存。
- 蓝图变量、函数、宏、事件、节点等基础操作。
- UMG Widget 树读取和编辑。
- DataAsset / UObject 属性读写。
- DataTable 行读写。
- PIE、编译、Undo/Redo、编辑器命令等基础集成。
- 重点解决“Agent 如何进入编辑器并执行基础操作”的问题。

**不要这样描述：**

> v0.2.0 主要优化 Token 消耗。

Token 优化是 v0.3.0 的主题，v0.2.0 应定位为 **编辑器接入与基础能力版**。

---

### v0.3.0：Agent 稳定性与通信优化版

**一句话定位：**

> v0.3.0 面向 Agent 实战使用，优化蓝图读取、通信格式、导入协议和操作稳定性，降低 Token 消耗并提升复杂蓝图编辑可靠性。

**功能边界：**

- 新增或强化 LogicJson。
- 新增或强化 LogicMD。
- 保留 RawJson 作为完整结构表达。
- 增加 Agent Import / 简化导入协议。
- 优化 MCP 返回协议，减少多余转义、冗余字段和上下文浪费。
- 优化蓝图逻辑读取，不再要求 Agent 每次读取完整 RawJson。
- 提升 Agent 对函数、图表、节点逻辑的理解效率。
- 强调稳定性、可审查性、低 Token、低歧义。
- 重点解决“Agent 能不能稳定、低成本、可控地操作蓝图”的问题。

**不要这样描述：**

> v0.3.0 只是加了 LogicJson。

LogicJson 只是手段。v0.3.0 的版本主题应是 **Agent 操作稳定性与通信优化**。

---

## 三个版本的主线关系

建议以后统一写成：

```text id="mut8w0"
v0.1.0：让蓝图可以被 JSON 表达。
v0.2.0：让 Agent 可以通过 MCP 访问和编辑 Unreal Editor。
v0.3.0：让 Agent 更稳定、更低 Token、更适合实战地操作蓝图。
```

更正式的版本线：

```text id="bsyopt"
v0.1.0 - Blueprint JSON Foundation
v0.2.0 - MCP Editor Access
v0.3.0 - Agent Stability & Communication Optimization
```

中文版本线：

```text id="5qx2aj"
v0.1.0 - 蓝图 JSON 表达基础版
v0.2.0 - MCP 编辑器接入版
v0.3.0 - Agent 稳定性与通信优化版
```

---

## 推荐 README 版本摘要

可以直接放进 README：

```md id="733a2d"
## Version Roadmap

### v0.1.0 - Blueprint JSON Foundation

v0.1.0 focuses on establishing the JSON representation layer of BlueprintHelper.  
This version implements the basic conversion between Unreal Engine Blueprints and JSON, allowing Blueprint graphs, nodes, pins, and links to be exported, inspected, and reconstructed through structured data.

### v0.2.0 - MCP Editor Access

v0.2.0 introduces the MCP-based editor access layer.  
This version allows AI Agents to connect to Unreal Editor through the BlueprintHelper CLI and UE Bridge, enabling basic asset browsing, Blueprint editing, UMG operations, DataAsset/DataTable access, compilation, saving, undo/redo, and editor command execution.

### v0.3.0 - Agent Stability & Communication Optimization

v0.3.0 improves practical Agent workflows.  
This version introduces optimized communication formats such as LogicJson and LogicMD, reduces RawJson dependency, lowers token consumption, improves import stability, and makes Blueprint reading and editing more reliable for AI Agents in real-world use.
```

---

## 推荐 CHANGELOG 归类规则

以后更新日志建议按这几个栏目写，避免版本混乱：

```md id="zp5ktr"
## v0.3.0

### Added
- LogicJson export for compact Blueprint logic representation.
- LogicMD export for human-readable Agent context.
- Agent graph import path for simplified Blueprint generation.
- Local lifecycle tools for project build and editor launch.

### Changed
- Optimized MCP return payloads to reduce token usage.
- Reduced dependency on full RawJson for common Agent workflows.
- Improved Blueprint operation stability by requiring more explicit asset and graph context.

### Fixed
- Improved handling of Blueprint graph import edge cases.
- Reduced ambiguity in node and pin interpretation.
- Improved validation before write operations.

### Compatibility
- RawJson remains the complete fidelity format.
- LogicJson / LogicMD are optimized views and should not replace RawJson for full-fidelity backup.
```

---

## 功能归属矩阵

| 功能 | v0.1.0 | v0.2.0 | v0.3.0 |
|---|---:|---:|---:|
| RawJson 导出 | 核心 | 保留 | 保留 |
| RawJson 导入 | 核心 | 保留 | 保留 |
| 蓝图结构 JSON 表达 | 核心 | 保留 | 保留 |
| MCP Server | - | 核心 | 强化 |
| UE Bridge 通信 | - | 核心 | 强化 |
| Agent 访问编辑器 | - | 核心 | 强化 |
| 资产浏览/打开/保存 | - | 核心 | 保留 |
| 蓝图变量/函数/节点编辑 | - | 核心 | 强化 |
| UMG 编辑 | - | 核心 | 保留 |
| DataAsset/DataTable 操作 | - | 核心 | 保留 |
| LogicJson | - | - | 核心 |
| LogicMD | - | - | 核心 |
| Token 消耗优化 | - | - | 核心 |
| Agent Import 简化导入 | - | - | 核心 |
| 操作稳定性优化 | - | 基础 | 核心 |
| 实战测试体系 | - | 基础验证 | 重点验收 |

---

## 后续版本建议

为了保持版本线清晰，后续可以这样规划：

| 版本 | 建议定位 |
|---|---|
| **v0.4.0** | Agent 审阅、Diff、变更确认、回滚工作流 |
| **v0.5.0** | 用户侧 Setup、蓝图/C++开发边界配置、Agent 偏好注入 |
| **v0.6.0** | 大型项目上下文索引、函数命名规范、跨蓝图搜索与引用追踪 |
| **v1.0.0** | 稳定 API、稳定 MCP 协议、完整测试覆盖、可发布生产版本 |

v0.4.0 不建议继续塞进 v0.3.0。v0.3.0 的边界已经很明确：**降低 Agent 操作成本，提高稳定性**。审阅、Diff、用户确认流属于更高一层的安全工作流，适合单独作为 v0.4.0。
---

# 2026-05-04 版本规划同步：混合 TaskSpec / TaskPlan 架构

## 新增版本口径

v0.4 / v0.5 之间新增一条实现主线：

```text
TaskSpec / TaskPlan Hybrid Orchestration
```

它不是推翻 v0.1-v0.3，而是在 v0.3 Agent 可用性优化之后补充任务级稳定性。

建议版本归属：

```text
v0.4.x：UE Task Runtime / task_run_id / TaskRunJournal / Review 分组。
v0.5.x：MCP/Python Task Compiler / TaskContextPack / preview_task / execute_task / Agent Skill 规则。
```

## 更新后的主线

```text
v0.1：蓝图 JSON 表达基础。
v0.2：MCP 编辑器接入。
v0.3：LogicMD / LogicJson / 通信优化 / Agent 操作稳定性。
v0.4：UE 侧审阅、事务、Task Runtime、任务级 Review。
v0.5：MCP/Python Task Compiler、TaskSpec、TaskContextPack、Agent Skill 任务流。
v0.6：大型项目上下文索引、跨蓝图引用、任务级检索。
```
