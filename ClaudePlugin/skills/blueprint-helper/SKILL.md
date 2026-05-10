---
name: blueprint-helper
description: Use when working with Unreal Engine Blueprints, UMG widgets, DataAssets, DataTables, or other UE editor assets through BlueprintHelper.
---

# BlueprintHelper Skill

## Non-negotiable SideAgent routing

When this skill is loaded for UE editor asset work, the Main Agent must not call BlueprintHelper MCP tools directly.

The Main Agent may only:
- read repository files, `AGENTS.md`, this skill, and references;
- decide task scope, ask the user for missing targets, and summarize results;
- dispatch a SideAgent with a concise task package.

The SideAgent must:
- read `references/09_SideAgent_Tool_Execution.md`;
- call BlueprintHelper MCP tools, including read and preflight tools such as `blueprinthelper_read_agent_guide`, `blueprint_get_runtime_profile`, `blueprinthelper_diagnostics`, `blueprinthelper_read_context`, `blueprinthelper_preview_task`, `blueprinthelper_execute_task`, and `blueprinthelper_get_task_result`;
- return translated results to the Main Agent with tool names, important arguments, status, blockers, validation results, and next action.

If the platform cannot start a SideAgent or the Main Agent cannot delegate, stop and report `sideagent_unavailable`. Do not silently bypass this rule by calling BlueprintHelper MCP tools from the Main Agent.

Normal repository tools remain allowed for C++, TypeScript, Python, JSON, docs, tests, config, `AGENTS.md`, and memory files. This exception does not include BlueprintHelper MCP tools or UE editor asset operations.

BlueprintHelper 是 UE 编辑器资产操作入口。`SKILL.md` 只负责让主 Agent 判断任务、读取索引、分派 SideAgent；工具参数和返回结果处理规则在 references 中。

## 主 Agent 入口职责

当用户要求操作蓝图或其他 UE 编辑器资产时：

1. 读取 `references/08_User_Preferences.md` 和 `references/00_Agent_Onboarding_Index_20260504.md`。
2. 判断用户需求是否缺少目标资产、目标图表或创建/修改策略。
3. 如果缺少关键目标，先问用户，不启用写入工具。
4. 如果需要调用 BlueprintHelper 工具，给 SideAgent 一个精简任务包，并要求它读取 `references/09_SideAgent_Tool_Execution.md`。
5. 接收 SideAgent 翻译后的结果，再由主 Agent 决定继续、请求确认或回复用户。

不要把整个 `SKILL.md` 原文传给 SideAgent。SideAgent 只接收任务包和需要读取的 reference 路径。

## SideAgent 任务包

主 Agent 下发给 SideAgent 的任务包只包含执行所需信息：

- 用户目标
- 目标资产路径和目标图表
- 是创建新资产还是修改已有资产
- 安全档位和写入授权要求
- 允许使用的 BlueprintHelper 工具
- 停止条件
- 返回格式要求

示例：用户说“在蓝图实现一个可以开关的物理门”时，如果目标资产未知，主 Agent 应先询问“修改已有门蓝图还是创建新的 `BP_PhysicsDoor`”。确认后再分派 SideAgent。

## 停止条件

以下情况主 Agent 不继续推进写入：

- 目标资产或创建策略不明确。
- Bridge 不可达。
- runtime_profile 不允许目标写入。
- preview 被阻断。
- capability 缺失。
- 写入授权被拒绝。
- SideAgent 返回的结果无法判断是否满足用户目标。

## 边界摘要

BlueprintHelper MCP 只用于 UE 编辑器资产：Blueprint、UMG、DataAsset、DataTable、编译、保存、打开、PIE/editor 命令和诊断。

C++、TypeScript、Python、JSON、配置、文档、AGENTS 和 memory 文件使用普通仓库工具。

## References

- `references/08_User_Preferences.md` — 用户偏好、协作规则、Debug/Review 约定
- `references/00_Agent_Onboarding_Index_20260504.md` — Agent 引导索引
- `references/09_SideAgent_Tool_Execution.md` — SideAgent 工具调用和结果翻译协议
- `references/01_Preflight_And_Boundary.md` — 预检和边界
- `references/02_TaskSpec_First_Tool_Selection.md` — TaskSpec-first 工具选择
- `references/03_Runtime_Profile_And_Diagnostics.md` — runtime_profile 和 diagnostics
- `references/04_MCP_Field_Templates_20260507.md` — MCP 字段模板
- `references/04_TaskSpec_Edit_Blueprint_Workflow.md` — TaskSpec 编辑蓝图工作流
- `references/05_Edit_Blueprint_Workflow.md` — 旧编辑蓝图工作流
- `references/06_UMG_Data_Workflows.md` — UMG 和数据工作流
- `references/07_Safety_Validation_And_Recovery.md` — 安全验证与恢复
