# BlueprintHelper AgentGuide / Agent 指南

## 中文

本目录用于指导 AI Agent 正确使用 BlueprintHelper 的 Agent-facing 工具面。

推荐入口：

```text
00_Agent_Onboarding_Index.md
```

新 Agent 应先阅读 onboarding index，再根据用户任务进入 `Reference/`、`Workflows/` 或 `Templates/`。

### 使用边界

- 普通 UE 编辑器资产读写走 CLI 和 TaskSpec-first workflow。
- Editor open/close 由全局 MCP lifecycle allowlist 负责。
- C++、TypeScript、Python、JSON、配置、文档和测试文件使用普通仓库工具。
- 不要把废弃 MCP 普通工具作为 fallback。

## English

This directory guides AI Agents in using the BlueprintHelper Agent-facing tool surface correctly.

Recommended entry:

```text
00_Agent_Onboarding_Index.md
```

New Agents should read the onboarding index first, then move into `Reference/`, `Workflows/`, or `Templates/` depending on the user task.

### Boundaries

- Ordinary UE editor-asset reads/writes use the CLI and TaskSpec-first workflow.
- Editor open/close is owned by the global MCP lifecycle allowlist.
- C++, TypeScript, Python, JSON, config, docs, and test files use normal repository tools.
- Do not use deprecated MCP ordinary tools as fallback.
