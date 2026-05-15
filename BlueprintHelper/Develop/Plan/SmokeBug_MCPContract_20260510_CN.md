# Smoke Bug - MCP Contract 2026-05-10

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

来源：`BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

本文记录 MCP 层合同、资源定位、read_context 能力边界问题。

## SMOKE-MCP-20260510-01: AgentGuide 资源定位在新项目插件副本中失败

**优先级**：P1

**现象**

- Smoke 中 MCP regression 为 Python 45/45 PASS，Node 149/152 PASS，3 个 Node 测试失败。
- 失败集中在 AgentGuide path / fixtures。
- 报告记录：AgentGuide file at Plugin `Resources/` path not found from Project root。

**实现证据**

- `tools.ts` 中 `AGENT_GUIDE_INDEX_RELATIVE_PATH` 固定为 `Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md`。
- `resolvePluginResourcePath()` 只从 `process.cwd()` 及若干相对候选目录寻找该路径。
- `tools.regression.test.ts` 期望从 `PLUGIN_ROOT/Resources/AgentGuide` 读取真实 Markdown 文件。

**初步根因**

MCP 运行在新项目拷贝或不同 cwd 时，资源目录没有稳定来源。当前解析逻辑既依赖 cwd，又依赖插件拷贝时包含 `Resources/AgentGuide`，两者任一不成立都会失败。

**建议修复**

- 资源定位优先基于 MCP 包自身路径或显式 `PLUGIN_ROOT`，cwd 只作为 fallback。
- 打包/复制插件时保证 `Resources/AgentGuide` 随插件进入目标项目。
- Node regression 增加“从项目根启动”和“从 ClaudePlugin/mcp 启动”两种 fixture。

## SMOKE-MCP-20260510-02: read_context schema 暗示多种 context，但实现只支持 blueprint_logic

**优先级**：P1

**现象**

- Smoke 已记录 known gap：`read_context` only supports `blueprint_logic`, not `widget_context/component_context`。
- 对 UMG / Component / Data 类任务，Agent 不能通过统一 read_context 获取对应上下文。

**实现证据**

- `blueprinthelper_read_context` 入口在 `input.read_type !== 'blueprint_logic'` 时直接返回 `unsupported_read_type`。
- 当前只会转发 `read_blueprint_logic_md` 或 `read_blueprint_logic_json`。

**初步根因**

TaskSpec-first 入口已经要求所有写入前读取上下文，但 read_context 只完成了 Graph/Logic 第一片能力。

**建议修复**

- 补 `component_context`：组件树、父子关系、组件类、默认属性摘要。
- 补 `widget_context`：WidgetTree、层级、命名、核心属性摘要。
- 补 `data_table_context` / `data_asset_context`：行结构、行摘要、属性摘要。
- 如果短期不能实现，应收紧 schema 和 AgentGuide，避免暴露未支持 read_type。

## SMOKE-MCP-20260510-03: 缺失目标资产的 negative read 返回诊断不够强

**优先级**：P2

**现象**

- Smoke 中 `read_context` negative case 读取 `/Game/NonExistent/FakeAsset`，返回 partial / empty `导出失败`，不是结构化硬错误。
- 这类结果不利于 Agent 自动判断是否应停止写入。

**初步根因**

Bridge 层把部分 UE 导出失败包装成空数据，而不是标准 `issues[]` / `error_code`。

**建议修复**

- 缺失资产统一返回 `target_asset_not_found`。
- `blueprinthelper_preview_task` 应把该错误提升为 blocked。
- 增加 regression：`ReadContextMissingAssetReturnsStructuredIssue`。
