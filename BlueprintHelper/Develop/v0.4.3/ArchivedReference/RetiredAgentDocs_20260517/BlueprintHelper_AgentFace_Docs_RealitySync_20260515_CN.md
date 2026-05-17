# BlueprintHelper AgentFace Docs Reality Sync 2026-05-15 / 2026-05-16

## Goal

检查 AgentFace / AgentGuide / CodexPlugin / ClaudePlugin 文档中会误导普通 Agent 的旧说明，并同步到 2026-05-15 的当前现实：

- 普通资产读写以 BlueprintHelper CLI 为入口。
- Editor open/close 由全局 BlueprintHelper MCP lifecycle 工具负责；CLI lifecycle helper 仅作兼容/手动 fallback。
- AgentGuide 模板优先：复制 JSON 模板、修改副本、用 CLI `--file` 调用。
- TaskSpec 命令分清两种根形态：直接工具名入口优先 `task_spec` wrapper；`task preview` / `task execute` 分组命令使用裸 `BlueprintHelper.TaskSpec.v1`。
- `blueprinthelper_apply_review_action` 只属于插件开发/内部验证，不暴露给普通 Agent。

2026-05-16 继续同步到当前实现：

- `AgentFaceService/mcp/src/server/lifecycle-only.ts` 和 `editor-lifecycle-tools.ts` 仍是 lifecycle-only MCP companion；文档中“旧 MCP 生命周期已废弃 / lifecycle 走 CLI 主线”的说法需要改回 global MCP lifecycle + CLI fallback。
- `AgentFaceService/cli/src/cli/run.ts` 当前实现支持直接工具名、`task preview/execute/result`、`context read`、`bridge ping/call`；CLI API 文档需要覆盖这些 grouped commands。
- `blueprinthelper_apply_review_action` 当前在 shared registry 中有 schema，但仍只作为插件开发/内部验证字段记录，不进入普通 Agent 模板。
- CLI JSON 模板必须能被 Node `JSON.parse` 直接读取；模板文件不应带 UTF-8 BOM。
- MCP build 当前会编译 MCP regression test fixtures；这些 fixtures 也需要跟随 `TaskPlan.execution_policy.review_baseline_dirty_asset_policy` 当前必填字段。

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
| 2026-05-16 implementation resync | done | 修正 CLI docs / Claude docs 中 lifecycle CLI-mainline 旧口径；补充 grouped CLI commands 和 internal Review action shape；修复 CodexPlugin 脚本路径；去掉一个模板 BOM。 |
| MCP test fixture compile sync | done | `AgentFaceService/mcp/src/tests/mcp/task-tools.regression.test.ts` 的 create_asset TaskPlan fixture 补齐 `review_baseline_dirty_asset_policy`。 |

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

2026-05-16 rerun:

| Check | Result | Notes |
|---|---|---|
| `npm --prefix AgentFaceService/task-core run build` | pass | TypeScript build passed. |
| `npm --prefix AgentFaceService/cli run build` | pass | CLI build passed; includes nested task-core build. |
| `npm --prefix AgentFaceService/mcp run build` | pass | Initial failure exposed stale test fixture; after adding `review_baseline_dirty_asset_policy`, MCP build passed. |
| stale lifecycle/path scan | pass | No `MCP lifecycle wiring is deprecated`, `deprecated MCP`, `旧 MCP 生命周期路径已废弃`, `CLI-only, no editor lifecycle MCP`, or stale `plugins/blueprint-helper` source paths remain. |
| CLI help smoke | pass | Built CLI help lists direct tools plus `open_editor`, `close_editor`, `task preview/execute/result`, `context read`, and `bridge ping/call`. |
| AgentGuide template JSON/schema check | pass | 64 JSON templates parsed; no template BOM; 60 schema-backed checks passed; 18 TS-compiler-supported TaskSpecs compiled; 13 Python-owned task types were schema-valid and intentionally skipped by TS fallback compiler. |
