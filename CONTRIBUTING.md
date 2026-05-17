# Contributing / 贡献指南

## 中文

BlueprintHelper 当前最需要来自真实 Unreal Editor 使用场景的反馈和 bug 修复。贡献应围绕一个具体问题、一个可复现流程、一个可验证修复展开。

### 优先欢迎

- 真实项目中的 Blueprint、UMG、DataAsset、DataTable 读写问题。
- CLI、TaskSpec、ReadSpec、Bridge、Review 面板或诊断流程中的可复现 bug。
- Agent 实际执行时暴露出的字段模板、错误信息、preview 结果或 execute 结果问题。
- 能降低误写、误删、误编译风险的安全校验修复。
- 修正文档中不准确、不完整或容易误导使用者的内容。

### 暂不优先

- 没有真实使用场景支撑的大型架构重写。
- 只为了风格统一的大面积格式化。
- 绕过 TaskSpec preview / execute 流程的低层写入入口。
- 没有复现步骤、目标资产范围或验证方式的功能请求。

### 报告 Bug

请尽量提供：

- 使用场景：你希望 BlueprintHelper 完成什么实际任务。
- 目标资产：例如 `/Game/Blueprints/BP_Door`，以及 graph、function、widget、table 或 property 范围。
- 执行入口：`bh` CLI、CodexPlugin、ClaudePlugin，还是 MCP allowlist。
- 复现步骤：从启动 Unreal Editor、读取上下文、preview 到 execute 的关键命令或操作。
- 实际结果：错误消息、CLI 输出、artifact 路径、Unreal 日志或截图。
- 期望结果：你认为正确的 BlueprintHelper 行为。
- 环境信息：UE 版本、BlueprintHelper 版本、Node 版本、操作系统、Bridge host/port。

如果问题发生在写入流程，请优先提供 preview 阶段结果。preview 已经失败的问题，不要只提交 execute 后错误。

### 修复建议流程

1. 确认问题边界：CLI 输入、TaskSpec schema、Python compiler、Bridge 路由，还是 UE capability cluster。
2. 用最小资产或 fixture 复现，避免带入真实项目中的无关状态。
3. 先补测试或复现用例，再改实现。无法自动化时，在 PR 中写清楚手动验证步骤。
4. 保持修复范围集中，不顺手重命名大量文件、重排文档结构或调整无关 API。
5. 对写入类修复，确认 preview、execute、result artifact、compile/save 策略和失败路径都能解释清楚。

### 本地验证

常用验证命令：

```powershell
npm.cmd --prefix AgentFaceService\task-core test
npm.cmd --prefix AgentFaceService\cli test
```

涉及 Unreal Editor 资产读写时，建议至少执行：

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
```

只改文档时，可以不跑完整测试，但请确认路径、命令和版本说明仍然准确。

不要恢复、编写或运行已废弃的 MCP 普通工具测试。MCP 只允许验证 editor open、editor close、developer-only exec command，以及不涉及废弃工具面的协议/打包边界测试。普通资产读写、TaskSpec、ReadSpec、诊断和结果查询测试应落在 CLI 或 task-core。

### PR 前检查

- 变更是否来自真实使用问题或明确 bug。
- 是否说明了复现步骤和修复前后的行为差异。
- 是否更新了对应文档或模板。
- 是否避免修改无关文件、生成目录、临时 artifacts 和项目私有资产。
- 是否保留 TaskSpec-first、preview-first 的安全边界。
- 是否列出已运行的验证命令；如果没有运行，说明原因。

## English

BlueprintHelper currently benefits most from feedback and bug fixes grounded in real Unreal Editor usage. Contributions should focus on one concrete problem, one reproducible workflow, and one verifiable fix.

### Preferred Contributions

- Real project issues involving Blueprint, UMG, DataAsset, or DataTable reads/writes.
- Reproducible bugs in CLI, TaskSpec, ReadSpec, Bridge, Review panel, or diagnostics workflows.
- Problems discovered during real Agent execution: field templates, error messages, preview results, or execute results.
- Safety fixes that reduce accidental writes, deletes, or compile risk.
- Small documentation corrections where current docs are inaccurate, incomplete, or misleading.

### Lower Priority

- Large architecture rewrites without a real usage case.
- Broad formatting-only changes.
- Low-level write entries that bypass the TaskSpec preview / execute workflow.
- Feature requests without reproduction steps, target asset scope, or validation method.

### Bug Reports

Please include:

- Use case: what you expected BlueprintHelper to do.
- Target asset: for example `/Game/Blueprints/BP_Door`, plus graph, function, widget, table, or property scope.
- Entry point: `bh` CLI, CodexPlugin, ClaudePlugin, or MCP allowlist.
- Reproduction steps: key commands/actions from editor launch, context read, preview, and execute.
- Actual result: error message, CLI output, artifact path, Unreal log, or screenshot.
- Expected result: the correct BlueprintHelper behavior.
- Environment: UE version, BlueprintHelper version, Node version, OS, Bridge host/port.

For write-flow bugs, prefer the preview result. If preview already failed, do not report only the execute error.

### Suggested Fix Flow

1. Identify the boundary: CLI input, TaskSpec schema, Python compiler, Bridge routing, or UE capability cluster.
2. Reproduce with the smallest asset or fixture possible.
3. Add a test or reproduction case before changing implementation. If automation is not practical, document manual verification in the PR.
4. Keep the fix focused. Do not rename broad file sets, reorganize docs, or adjust unrelated APIs.
5. For write fixes, verify preview, execute, result artifacts, compile/save policy, and failure paths.

### Local Verification

Common commands:

```powershell
npm.cmd --prefix AgentFaceService\task-core test
npm.cmd --prefix AgentFaceService\cli test
```

For Unreal Editor asset reads/writes, run at least:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
```

For documentation-only changes, full tests may be unnecessary, but verify that paths, commands, and version notes remain accurate.

Do not restore, write, or run deprecated MCP ordinary-tool tests. MCP validation is limited to editor open, editor close, developer-only exec command, and protocol/package boundary tests that do not touch the deprecated ordinary tool surface. Ordinary asset read/write, TaskSpec, ReadSpec, diagnostics, and result-query tests belong in CLI or task-core.

### PR Checklist

- The change comes from a real usage problem or explicit bug.
- Reproduction steps and behavior differences are documented.
- Relevant docs or templates are updated.
- Unrelated files, generated directories, temporary artifacts, and private project assets are untouched.
- TaskSpec-first and preview-first safety boundaries remain intact.
- Verification commands are listed; if not run, the reason is stated.
