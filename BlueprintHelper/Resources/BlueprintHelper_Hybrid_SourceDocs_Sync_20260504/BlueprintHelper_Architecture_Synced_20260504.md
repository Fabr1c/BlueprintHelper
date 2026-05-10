可以。建议以后把 **BlueprintHelper 总体架构** 固定拆成四个部分：

```text id="igo5rj"
BlueprintHelper = UE 插件侧 + MCP 服务侧 + Agent Skill 侧 + 用户引导侧
```

这四部分分别解决不同问题，不应混写在同一类文档或同一版本目标里。

---

## BlueprintHelper 四部分总分类

| 分类 | 推荐名称 | 核心职责 | 面向对象 |
|---|---|---|---|
| **1** | **UE 插件侧 / UE Plugin Layer** | 在 Unreal Editor 内真正读取、修改、保存资产 | Unreal Editor、蓝图、UMG、DataAsset、DataTable |
| **2** | **MCP 服务侧 / MCP Server Layer** | 把 Agent 请求转换成 UE Bridge 调用，并处理协议、连接、返回格式 | Agent、MCP Client、UE Bridge |
| **3** | **Agent Skill 侧 / Agent Skill Layer** | 告诉 Agent 如何正确使用工具、如何判断蓝图/C++边界、如何选择 LogicMD/LogicJson/RawJson | Claude / Codex / ChatGPT Agent |
| **4** | **用户引导侧 / User Guidance & Setup Layer** | 面向用户完成安装、配置、问答式偏好采集、项目规范生成 | 插件用户、项目维护者 |

这个拆分和之前版本线可以兼容。已有版本规划里已经把主线定义为 **数据表达 → 编辑器接入 → Agent 可用性优化**，其中 v0.1.0 是蓝图 JSON 表达基础，v0.2.0 是 MCP 编辑器接入，v0.3.0 是 Agent 稳定性与通信优化。fileciteturn2file0 后续 Skill、Setup、引导文档正好补上“Agent 如何正确使用”和“用户如何配置”的上层能力。

---

# 1. UE 插件侧 / UE Plugin Layer

## 定位

UE 插件侧是 **真正执行编辑器操作的底层能力层**。

它不负责教 Agent 怎么思考，也不负责用户安装说明；它只负责在 Unreal Editor 内安全、稳定、可回滚地完成资产读取和修改。

## 包含内容

```text id="6i7sq5"
BlueprintHelper.uplugin
Source/BlueprintHelper/
Resources/
Editor Widget / Panel
UE Bridge Server
Blueprint Export / Import
LogicProcessor
Asset / Blueprint / UMG / DataTable / DataAsset 操作
Undo / Redo / Save / Compile / PIE 控制
```

## 主要职责

- 蓝图 RawJson 导出/导入。
- LogicJson / LogicMD 生成。
- AgentImportGraph 导入。
- 资产浏览、搜索、打开、保存。
- 蓝图变量、函数、宏、事件、节点操作。
- UMG Widget 树读取与编辑。
- DataAsset / UObject 属性读写。
- DataTable 行读写。
- 编辑器命令、编译、PIE、Undo/Redo。
- 未来的 Diff、审阅、变更确认、回滚 UI。

## 不应该承担

- 不负责 Agent 提示词。
- 不负责 Codex / Claude Skill 规则。
- 不负责用户问答式 setup。
- 不负责解释“什么时候用蓝图，什么时候用 C++”。

一句话：

> UE 插件侧负责“能不能在 UE 里正确执行”。

---

# 2. MCP 服务侧 / MCP Server Layer

## 定位

MCP 服务侧是 **Agent 与 Unreal Editor 之间的协议适配层**。

它不直接修改 `.uasset`，而是通过 UE Bridge 调用 UE 插件侧能力。它的关键价值是：把 UE 能力包装成 Agent 可调用、可验证、可追踪、低 Token 的工具接口。

## 包含内容

```text id="8i1dr2"
BlueprintHelper_MCP_Server/
src/tools.ts
src/bridge-client.ts
src/config.ts
src/resources.ts
MCP tool schema
MCP resources
Bridge connection / reconnect / timeout
structuredContent / resource_ref / payload protocol
```

## 主要职责

- 注册 MCP Tools。
- 注册 MCP Resources。
- 管理 UE Bridge 连接。
- 处理工具参数校验。
- 处理错误码和错误消息。
- 处理 `request_id` / `trace_id`。
- 处理 `logic_md`、`logic_json`、`raw_json_structured`、`resource_ref` 返回方式。
- 管理大 payload 延迟读取。
- 管理 build_project / open_editor 这类本地生命周期工具。
- 未来处理持久连接、自动重连、Length-Prefixed JSON framing 等通信稳定性问题。

之前 v0.5.0 通信侧计划已经把短连接日志刷屏、持久 BridgeClient、ClientSession、request_id / trace_id、自动重连、timeout、协议降级等列入通信侧改动范围。fileciteturn2file2 这些内容应归到 MCP 服务侧，不应混入 Skill 或用户文档层。

## 不应该承担

- 不直接修改 UE C++ 源码。
- 不直接写 `.uasset`。
- 不替代 UE 插件侧做资产事务。
- 不负责解释项目开发规范。
- 不负责用户偏好问答，只消费 setup 结果。

一句话：

> MCP 服务侧负责“Agent 怎么稳定、低成本地调用 UE 能力”。

---

# 3. Agent Skill 侧 / Agent Skill Layer

## 定位

Skill 侧是 **给 Agent 看的操作规约层**。

它的目标不是给人阅读，而是让 Agent 在被调用时自动知道：

- 这是 UE5.3+ 的 BlueprintHelper 插件项目。
- MCP 工具不是通用文件编辑器。
- 写操作必须指定资产路径和图表。
- 默认先读 LogicMD。
- 结构化分析读 LogicJson。
- 精确保真、导入、Pin 级调试才读 RawJson。
- C++ 源码修改不能走 BlueprintHelper MCP。
- 蓝图和 C++ 的开发边界应按用户 setup profile 判断。

## 包含内容

```text id="l4rmzw"
Skills/
BlueprintHelper/SKILL.md
BlueprintHelper/tool_usage_policy.md
BlueprintHelper/blueprint_cpp_boundary.md
BlueprintHelper/logic_reading_strategy.md
BlueprintHelper/import_strategy.md
BlueprintHelper/error_recovery.md
BlueprintHelper/examples/
```

## 主要职责

- 定义 Agent 调用 MCP 工具的默认顺序。
- 定义 LogicMD / LogicJson / RawJson / resource_ref 的使用策略。
- 定义蓝图编辑前置检查。
- 定义写操作安全规则。
- 定义失败恢复策略。
- 定义何时停止使用 MCP，改用代码工具。
- 定义蓝图命名、函数命名、可搜索性要求。
- 消费用户 setup profile，生成个性化 Agent 行为规则。

已有文档补齐计划中，P0 已经把 **README、MCP QuickStart、Setup 问答、Setup Profile、Claude Skill、MCP API Reference** 列为下一批优先项；P1 还包含 MCP Resources、LogicJson/LogicMD/AgentImportGraph Schema、命名规范、蓝图/C++边界、安全模型。fileciteturn2file1 其中 Claude Skill、工具使用策略、蓝图/C++边界规则应归入 Skill 侧。

## 不应该承担

- 不实现 UE 资产操作。
- 不实现 MCP 协议。
- 不保存用户 UI 配置。
- 不写长篇用户手册。
- 不替代 README。

一句话：

> Agent Skill 侧负责“Agent 应该怎么正确使用 BlueprintHelper”。

---

# 4. 用户引导侧 / User Guidance & Setup Layer

## 定位

用户引导侧是 **面向用户的安装、配置、偏好采集和规范生成层**。

它解决的是用户如何开始使用插件，以及如何把自己的开发偏好传递给 Agent。

## 包含内容

```text id="1eibc3"
README.md
QuickStart.md
Install.md
MCP_Setup.md
Setup Wizard / Setup Profile
Blueprint_CPP_Boundary_QA.md
User_Preferences.md
Troubleshooting.md
Examples.md
Release Notes
CHANGELOG
```

## 主要职责

- 安装插件。
- 配置 MCP Server。
- 配置 UE_ENGINE_DIR / UE_PROJECT_FILE。
- 检查 UE Bridge 是否可用。
- 生成 setup profile。
- 通过问答确定用户蓝图/C++边界。
- 记录用户开发偏好。
- 生成 Agent 可消费的规则文件。
- 指导用户如何测试连通性。
- 指导用户如何处理常见错误。
- 解释版本差异和功能范围。

## 不应该承担

- 不直接实现 MCP 工具。
- 不直接修改 UE 资产。
- 不塞入过多 Agent 内部规则。
- 不替代 Skill 自动发现机制。

一句话：

> 用户引导侧负责“用户如何安装、配置，并把个人开发风格交给 Agent”。

---

# 推荐最终架构口径

可以在 README 或架构文档中这样写：

```md id="98xjvx"
# BlueprintHelper Architecture

BlueprintHelper is divided into four major parts:

1. UE Plugin Layer
   Provides the Unreal Editor-side capabilities, including Blueprint export/import,
   LogicJson/LogicMD generation, asset operations, UMG editing, DataAsset/DataTable
   access, compilation, save, undo/redo, and editor command integration.

2. MCP Server Layer
   Exposes the UE Plugin capabilities to AI Agents through MCP tools and resources.
   It manages Bridge communication, tool schemas, request validation, payload formats,
   error handling, and editor lifecycle commands.

3. Agent Skill Layer
   Provides machine-readable rules for AI Agents. It defines how Agents should use
   BlueprintHelper tools, when to read LogicMD, LogicJson, RawJson, or resource_ref,
   how to handle Blueprint/C++ boundaries, and how to perform safe write operations.

4. User Guidance & Setup Layer
   Provides human-facing onboarding, setup, configuration, troubleshooting, and
   preference collection. It generates setup profiles and development-boundary rules
   that can be consumed by the Agent Skill layer.
```

---

# 四部分和版本规划的关系

| 版本 | 主体归属 | 说明 |
|---|---|---|
| **v0.1.0** | UE 插件侧 | 蓝图 JSON 表达、RawJson 导入导出 |
| **v0.2.0** | UE 插件侧 + MCP 服务侧 | Agent 通过 MCP 访问 Unreal Editor |
| **v0.3.0** | UE 插件侧 + MCP 服务侧 | LogicJson / LogicMD / AgentImport / Token 优化 / 稳定性 |
| **v0.4.0** | UE 插件侧 + 用户引导侧 | Diff、审阅、确认、回滚、安全工作流 |
| **v0.5.0** | MCP 服务侧 + Skill 侧 + 用户引导侧 | Setup、蓝图/C++边界、Agent 偏好注入、通信稳定性 |
| **v0.6.0** | Skill 侧 + MCP 服务侧 | 大型项目索引、跨蓝图搜索、引用追踪、函数命名规范 |
| **v1.0.0** | 四部分全部收敛 | 稳定 API、稳定协议、完整文档、完整测试 |

---

# 推荐目录分类

不一定要立刻改真实目录，但长期可以按这个逻辑整理：

```text id="ot7td9"
BlueprintHelper/
├─ UEPlugin/
│  ├─ BlueprintHelper.uplugin
│  ├─ Source/
│  ├─ Content/
│  └─ Resources/
│
├─ BlueprintHelper_MCP_Server/
│  ├─ src/
│  ├─ package.json
│  └─ README.md
│
├─ Skills/
│  └─ BlueprintHelper/
│     ├─ SKILL.md
│     ├─ tool_usage_policy.md
│     ├─ blueprint_cpp_boundary.md
│     ├─ logic_format_strategy.md
│     └─ examples/
│
└─ Docs/
   ├─ README.md
   ├─ QuickStart.md
   ├─ Setup.md
   ├─ MCP_API_Reference.md
   ├─ Troubleshooting.md
   ├─ VersionRoadmap.md
   └─ Changelog.md
```

如果不想改目录，可以保持现有插件目录，只在文档中采用这个逻辑分类：

```text id="rol31q"
BlueprintHelper/Resources/Plan/
BlueprintHelper/Resources/Docs/
BlueprintHelper/Resources/Skills/
BlueprintHelper/BlueprintHelper_MCP_Server/
BlueprintHelper/Source/
```

---

# 最终定义

建议把 BlueprintHelper 的总体定义改成：

> **BlueprintHelper 是一个由 UE 插件侧、MCP 服务侧、Agent Skill 侧、用户引导侧组成的 UE5 Agent 编辑辅助系统。UE 插件侧负责编辑器内能力，MCP 服务侧负责协议与通信，Agent Skill 侧负责 Agent 行为规则，用户引导侧负责安装、配置、偏好采集与开发边界定义。**

这个四分法比“插件 + MCP”更准确，也能容纳后续 Skill、Setup、引导、Profile、审阅工作流。
---

# 2026-05-04 混合 TaskSpec / TaskPlan 架构同步

## 同步结论

本文件原有“四层架构”不推翻，但需要补充一个新的任务编排切分：

```text
Agent
→ MCP Agent-facing Task Tools
→ Python / MCP Task Compiler
→ UE Plugin Task Runtime
→ Existing UE Capability Clusters
```

原四层仍保留：

```text
BlueprintHelper = UE 插件侧 + MCP 服务侧 + Agent Skill 侧 + 用户引导侧
```

新增口径不是第五个平级层，而是把原 MCP 服务侧和 UE 插件侧之间的任务职责拆清楚：

```text
Python / MCP Task Compiler：负责 TaskSpec 校验、上下文打包、语义检查、suggested_patch、TaskPlan 生成。
UE Plugin Task Runtime：负责 TaskPlan 执行、TOCTOU 检查、task_run_id、transaction_id、Journal / Review / rollback、compile/save。
```

## UE 插件侧职责修正

UE 插件侧仍负责真实编辑器操作，但新增 Task Runtime 职责：

```text
FBlueprintHelperTaskRuntime
FBlueprintHelperTaskPlanValidator
FBlueprintHelperTaskRunJournalService
FBlueprintHelperTaskExecutionContext
FBlueprintHelperTaskRollbackCoordinator
FBlueprintHelperTaskReviewGrouper
```

UE 插件侧应该负责：

```text
1. 接收已编译的 TaskPlan，而不是直接理解 Agent 原始自然语言。
2. 执行前重新读取 UE 当前状态并做 TOCTOU 检查。
3. 执行 TaskPlan steps。
4. 每个真实写操作生成 transaction_id。
5. 整个 TaskPlan 执行生成 task_run_id。
6. 写 TaskRunJournal / Transaction Journal / Review snapshot。
7. 失败时按已执行写步骤逆序 rollback。
8. 执行 compile / diagnostics / save。
```

UE 插件侧不建议负责：

```text
1. Agent-facing TaskSpec suggested_patch。
2. JSONPath / JsonPatch 级错误修正建议。
3. 自然语言 goal → TaskSpec。
4. 不同 Agent 客户端兼容。
5. Agent 行为策略文本。
```

## MCP 服务侧职责修正

MCP 服务侧不再默认向 Agent 暴露大量底层工具。普通 Agent-facing 工具应收敛为：

```text
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
```

底层工具簇仍保留，但默认作为：

```text
1. Python / MCP Task Compiler 可用的 capability。
2. UE Task Runtime 内部 step operation。
3. Debug / Expert / 测试入口。
4. MCP API Reference 的低层能力说明。
```

## Agent Skill 侧职责修正

Agent Skill 不再教 Agent 手动拼大量底层工具调用，而是教 Agent：

```text
1. 先 read_task_context 获取 TaskContextPack。
2. 基于 ContextPack 生成 TaskSpec。
3. 调 preview_task。
4. 根据结构化 error.issues 修正 TaskSpec。
5. preview 通过后 execute_task。
6. 最终报告 task_run 摘要，不默认报告 transaction_id / journal path。
```

## 用户引导侧职责修正

用户文档需要说明：BlueprintHelper 的普通使用入口是任务级工具，不是底层工具。底层工具仍存在，但主要用于调试、测试和高级 Expert 场景。
