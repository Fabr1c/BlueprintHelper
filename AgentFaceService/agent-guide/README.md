# BlueprintHelper AgentGuide / Agent 指南

## 中文

本目录用于指导 AI Agent 正确使用 BlueprintHelper 的 Agent-facing 工具面。

推荐入口：

```text
00_Agent_Onboarding_Index.md
```

新的 Agent 应先阅读 onboarding index，再根据用户任务进入 `Reference/`、`Workflows/`，或读取 CLI catalog 返回的具体 `Templates/` 路径。

### 工具 Catalog

Agent-facing 工具和模板选择由 CLI catalog 负责：

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Agent 只读取 `bh tools templates <tool_id>` 返回的具体模板路径。不要扫描 `Templates/` 目录来选择工具或模板。

### 使用边界

- 普通 UE 编辑器资产读写走 CLI 和 TaskSpec-first workflow。
- Editor open/close 由全局 MCP lifecycle allowlist 负责。
- C++、TypeScript、Python、JSON、配置、文档和测试文件使用普通仓库工具。
- 不要把废弃 MCP 普通工具作为 fallback。

## English

This directory guides AI Agents in using the BlueprintHelper Agent-facing tool
surface correctly.

Recommended entry:

```text
00_Agent_Onboarding_Index.md
```

New Agents should read the onboarding index first, then move into `Reference/`,
`Workflows/`, or concrete `Templates/` paths returned by the CLI catalog
depending on the user task.

### Tool Catalog

Agent-facing tool and template selection is CLI-owned:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Agents should read only the concrete template paths returned by
`bh tools templates <tool_id>` and must not scan `Templates/` to choose tools or
templates.

### Boundaries

- Ordinary UE editor-asset reads/writes use the CLI and TaskSpec-first workflow.
- Editor open/close is owned by the global MCP lifecycle allowlist.
- C++, TypeScript, Python, JSON, config, docs, and test files use normal repository tools.
- Do not use deprecated MCP ordinary tools as fallback.
