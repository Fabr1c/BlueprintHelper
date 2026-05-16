# BlueprintHelper AgentFace Docs Reality Sync 2026-05-15

## Goal

检查 AgentFace / AgentGuide / CodexPlugin / ClaudePlugin 文档中会误导普通 Agent 的旧说明，并同步到 2026-05-15 的当前现实：

- 普通资产读写以 BlueprintHelper CLI 为入口。
- Editor open/close 由全局 BlueprintHelper MCP lifecycle 工具负责；CLI lifecycle helper 仅作兼容/手动 fallback。
- AgentGuide 模板优先：复制 JSON 模板、修改副本、用 CLI `--file` 调用。
- TaskSpec 命令分清两种根形态：直接工具名入口优先 `task_spec` wrapper；`task preview` / `task execute` 分组命令使用裸 `BlueprintHelper.TaskSpec.v1`。
- `blueprinthelper_apply_review_action` 只属于插件开发/内部验证，不暴露给普通 Agent。

## Scope

已检查并更新：

| Area | Status | Notes |
|---|---|---|
| Root docs | done | 更新 `README.md` 中 CLI / global MCP / template-first 口径。 |
| AgentFaceService docs | done | `AgentFaceService/README.md` 从 deprecated MCP 口径改为 global MCP lifecycle companion。 |
| CLI docs | done | `CLI_Tools_API_Reference.md`、`Install_CLI_QuickStart.md`、`TaskSpec_CLI_QuickStart.md` 同步 wrapper vs bare TaskSpec、模板入口、global MCP lifecycle。 |
| Canonical AgentGuide | done | `Resources/AgentGuide` onboarding、tool selection、field templates、TaskSpec workflow 已更新。 |
| Codex plugin docs | done | Codex README/AGENTS/SKILL/reference mirrors 已同步。 |
| Claude plugin docs | done | Claude README/AGENTS/SKILL/reference mirrors 已同步。 |
| Packaged Codex plugin mirror | done | `plugins/blueprint-helper` 当前内容已与新口径一致；批量覆盖时遇到短暂文件锁，后续扫描确认关键旧口径未残留。 |

## Findings

- 用户关于 `CLITips` 的猜测成立：旧文档中确实存在多处容易导致 Agent 字段错误的说明，包括 TaskSpec 根对象形态、`args` wrapper、`project_file` lifecycle 参数、以及直接暴露内部 Review action 的边界不清。
- 模板目录需要成为 AgentGuide 的第一入口，而不只是补充示例；文档已改为“copy template -> edit file -> CLI `--file`”。
- 读上下文模板与写 TaskSpec 模板需要分离；根目录只保留 runtime profile、diagnostics、guide、write session、task result、debug/review-summary 这类非逻辑上下文读取模板。

## Verification

Completed:

| Check | Result | Notes |
|---|---|---|
| Static stale-phrase scan | pass | `deprecated/frozen`、`Do not prefer CLI`、`CLI-only`、`必须把 TaskSpec 包`、`只接受根字段 task_spec`、`调用时必须传显式 project_file` 等旧口径无命中。 |
| Internal Review action scan | pass | `blueprinthelper_apply_review_action` 只以 plugin-development/internal 或 omitted-from-Agent-facing-templates 形式出现。 |
| `npm --prefix AgentFaceService/task-core run build` | pass | TypeScript build passed. |
| `npm --prefix AgentFaceService/cli run build` | pass | CLI build passed; includes nested task-core build. |
| AgentGuide template JSON/schema check | pass | 64 JSON templates parsed; 60 schema-backed checks passed; 18 TS-compiler-supported TaskSpecs compiled; 13 Python-owned task types were schema-valid and intentionally skipped by TS fallback compiler. |
