---
name: blueprint-helper
description: Use when working with Unreal Engine Blueprints, UMG widgets, DataAssets, DataTables, or any UE editor assets. Provides MCP tools for reading and modifying Unreal Editor assets through a local Bridge connection. Always use this skill before making any MCP calls to BlueprintHelper tools.
---

# BlueprintHelper Skill — TaskSpec-first

BlueprintHelper 是 UE5.3+ 的 Agent 编辑辅助系统。通过 MCP Server 桥接 AI Agent 与运行中的 Unreal Editor，使用 TaskSpec-first 架构安全地操作编辑器资产。

## 默认流程

```text
get_runtime_profile → read_task_context → build TaskSpec → preview_task → execute_task → report summary
```

不要把复杂 UE 资产任务拆成大量底层 MCP 调用。底层工具簇是 TaskPlan capability、debug / expert 工具和测试入口。

## 关键规则

- runtime_profile.active_profile 是 safety_profile 唯一来源。
- diagnostics 只定位问题，不替代 runtime_profile。
- LogicMD 用于理解，LogicJson 用于结构化分析，RawJson/resource_ref 用于保真、导入或 Pin/GUID 级调试。
- Asset Factory 只创建资产。
- add_component 只创建组件和 attachment。
- Class Settings 不写图表逻辑，不支持第一版 reparent。
- Enhanced Input 默认不编辑 IA / IMC。
- Append/Replace/Patch/Merge 是 Graph Write capability，不是普通 Agent 默认直调入口。
- preview_blocked、missing capability、rollback blocked/failed 时 stop_and_report。

## 安全写入前置检查

任何写入操作前必须完成：

1. 确认用户有目标 UE 项目且 Unreal Editor 正在运行，或 MCP server 配置了 `UE_ENGINE_DIR` 和 `UE_PROJECT_FILE`。
2. 确认 Bridge 可达。
3. 识别精确的目标资产路径，例如 `/Game/Blueprints/BP_Player`。
4. 编辑图表节点时识别精确的目标图表，例如 `EventGraph`。
5. 优先使用 TaskSpec-first 写入流程。
6. 如果 `write_permission` 被禁用，在 preview 之后 execute 之前调用 `blueprinthelper_request_write_session`。
7. 不要对交互式写入请求或注入 `BLUEPRINTHELPER_BRIDGE_TOKEN` / `auth_token`。
8. 不要依赖当前聚焦的编辑器标签页进行破坏性操作。

## 边界规则

使用 BlueprintHelper MCP 处理：
- 通过运行的 Unreal Editor 读写现有 UE 资产。
- 创建或修改蓝图图表、变量、函数、宏、节点、链接和事件分发器。
- 读写 UMG widget tree 和 widget 属性。
- 读写 UObject / DataAsset 属性。
- 读写 DataTable 行。
- 编译、打开、保存、验证、导入或导出蓝图相关编辑器资产。

使用普通仓库工具处理：
- C++ / TypeScript / Python / config 编辑。
- 搜索源文件。
- 添加文档文件。
- 更新构建脚本。
- 编写 AGENTS.md / memory / 项目说明。

## 参考文档

需要详细 API 参考和工作流程时，读取以下文件：

- `references/00_Agent_Onboarding_Index.md` — Agent 引导索引
- `references/01_Preflight_And_Boundary.md` — 预检和边界
- `references/02_Capability_Index.md` — 能力索引
- `references/03_Runtime_Profile_And_Diagnostics.md` — 运行时配置与诊断
- `references/04_MCP_Field_Templates.md` — MCP 字段模板
- `references/04_TaskSpec_Edit_Blueprint_Workflow.md` — TaskSpec 编辑蓝图工作流
- `references/05_Edit_Blueprint_Workflow.md` — 编辑蓝图工作流
- `references/06_UMG_Data_Workflows.md` — UMG 和数据工作流
- `references/07_Safety_Validation_And_Recovery.md` — 安全验证与恢复
