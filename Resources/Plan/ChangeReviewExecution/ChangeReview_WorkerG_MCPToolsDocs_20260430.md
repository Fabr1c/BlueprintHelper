# Worker G MCP Tools And Agent Docs Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 在 MCPServer 中暴露 Change Review 工具，并更新 Agent 使用规则和用户文档。

**Architecture:** MCP 工具薄封装 Bridge review 命令。只读工具不需要用户授权，approve/reject 工具描述中明确要求用户显式指令。旧工具不改名，不改变原有参数。

**Tech Stack:** TypeScript、MCP SDK、zod、BridgeClient。

---

## 写入边界

允许修改：

```text
MCPServer/src/tools.ts
Resources/AGENT.md
```

允许新增：

```text
Resources/Rules/AgentReviewPolicy.md
Resources/Docs/MCP_ReviewTools.md
```

不允许修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/Private/SHelperMainWidget.cpp
```

## 新增 MCP 工具

```text
blueprint_review_begin_session
blueprint_review_list_pending
blueprint_review_get_session
blueprint_review_get_summary
blueprint_review_open_diff
blueprint_review_approve
blueprint_review_reject
blueprint_review_export_markdown
```

## 授权规则

不需要用户显式授权：

```text
blueprint_review_list_pending
blueprint_review_get_session
blueprint_review_get_summary
```

需要明确目标 session：

```text
blueprint_review_open_diff
blueprint_review_export_markdown
```

需要用户明确指令：

```text
blueprint_review_approve
blueprint_review_reject
```

## 任务

### Task G1: 增加 review 工具注册

**Files:**

- Modify: `MCPServer/src/tools.ts`

- [ ] 在 editor-bound tools 区域新增 8 个 review 工具。
- [ ] 每个工具只调用对应 Bridge 命令。
- [ ] 复用 `toToolResult` 和 `toErrorResult`。
- [ ] 不改变已有工具名称、schema 或返回处理。

验收：

- `npm run build` 通过。
- 旧工具注册不受影响。

### Task G2: 给写工具透传 review 字段

**Files:**

- Modify: `MCPServer/src/tools.ts`

- [ ] 对首批写工具加入可选 `review_policy`、`review_session_id`、`review_display_name`。
- [ ] 首批写工具至少包括 `blueprint_add_variable`、`blueprint_delete_nodes`、`blueprint_set_object_property`。
- [ ] 参数不传时不改变现有 payload，除非 Worker D 已决定默认 pending。

验收：

- TypeScript 类型检查通过。
- 旧调用方可以不传新字段。

### Task G3: 更新 AGENT 规则

**Files:**

- Modify: `Resources/AGENT.md`
- Create: `Resources/Rules/AgentReviewPolicy.md`

- [ ] 增加 Change Review 工具说明。
- [ ] 明确 Agent 不能默认 approve/reject。
- [ ] 明确写操作应显式传目标资产和图表。
- [ ] 明确收到 pending review 后应提示用户到面板审阅。

验收：

- 文档不和现有 MCP 使用规则冲突。

### Task G4: MCP 工具文档

**Files:**

- Modify or Create: `Resources/Docs/MCP_ReviewTools.md`

- [ ] 每个工具包含用途、输入、输出、授权规则。
- [ ] 包含示例响应。
- [ ] 明确 `blueprint_get_logic` 与 review summary 的区别。

验收命令：

```powershell
npm run build
```

执行目录：

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
```

预期：构建通过。

