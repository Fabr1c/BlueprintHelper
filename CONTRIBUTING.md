# Contributing

BlueprintHelper 当前更需要来自真实 Unreal Editor 使用场景的反馈和 bug 修复，而不是大范围重构或抽象设计。提交贡献时，请尽量围绕一个具体问题、一个可复现流程、一个可验证修复来展开。

## 贡献重点

优先欢迎以下类型的贡献：

- 真实项目中遇到的 Blueprint、UMG、DataAsset、DataTable 读写问题。
- CLI、TaskSpec、ReadSpec、Bridge、Review 面板或诊断流程中的可复现 bug。
- Agent 实际执行时暴露出的字段模板、错误信息、预览结果或执行结果问题。
- 能降低误写、误删、误编译风险的安全校验修复。
- 对现有文档中不准确、不完整、容易误导实际使用者的地方做小范围修正。

暂不优先以下类型的贡献：

- 没有真实使用场景支撑的大型架构重写。
- 只为了风格统一的大面积格式化。
- 绕过 TaskSpec preview / execute 流程的低层级写入入口。
- 没有复现步骤、没有目标资产范围、没有验证方式的功能请求。

## 报告 Bug

提交 bug 时，请尽量包含这些信息：

- 使用场景：你要让 BlueprintHelper 完成什么实际任务。
- 目标资产：例如 `/Game/Blueprints/BP_Door`，以及具体 graph、function、widget、table 或 property 范围。
- 执行入口：使用的是 `bh` CLI、CodexPlugin、ClaudePlugin，还是 MCP 生命周期入口。
- 复现步骤：从启动 Unreal Editor、读取上下文、preview 到 execute 的关键命令或操作。
- 实际结果：错误消息、CLI 输出、artifact 路径、Unreal 日志或截图。
- 期望结果：你认为正确的 BlueprintHelper 行为。
- 环境信息：UE 版本、BlueprintHelper 版本、Node 版本、操作系统，以及是否使用自定义 Bridge host/port。

如果问题发生在写入流程，请优先提供 preview 阶段的结果。preview 已经失败的问题，不要只提交 execute 后的错误。

## 修复 Bug 的建议流程

1. 先确认问题边界：这是 CLI 输入问题、TaskSpec schema 问题、Python compiler 问题、Bridge 路由问题，还是 UE 侧 capability cluster 问题。
2. 用最小资产或 fixture 复现问题，避免把真实项目中的无关资产状态带入修复。
3. 先补测试或复现用例，再改实现。无法自动化时，在 PR 里写清楚手动验证步骤。
4. 保持修复范围集中。修 bug 时不要顺手重命名大量文件、重排文档结构或调整无关 API。
5. 对写入类修复，确认 preview、execute、result artifact、compile/save 策略和失败路径都能解释清楚。

## 本地验证

常用验证命令：

```powershell
npm.cmd --prefix AgentFaceService\task-core test
npm.cmd --prefix AgentFaceService\cli test
```

如果只改文档，可以不跑完整测试，但请至少确认 Markdown 中的路径、命令和版本说明仍然准确。

涉及 Unreal Editor 资产读写时，建议执行一条只读检查和一条 preview 检查：

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
```

不要在没有 preview 结果的情况下直接提交写入类行为变更。

## 提交 PR 前检查

- 变更是否来自一个真实使用问题或明确 bug。
- 是否说明了复现步骤和修复前后的行为差异。
- 是否更新了对应文档或模板。
- 是否避免修改无关文件、生成目录、临时 artifacts 和项目私有资产。
- 是否保留 TaskSpec-first、preview-first 的安全边界。
- 是否列出已运行的验证命令；如果没有运行，说明原因。

## 文档贡献

文档更新应优先服务实际使用：

- 写清楚什么时候用 BlueprintHelper，什么时候用普通源码工具。
- 命令示例优先使用可复制的 `bh ... --json` 或 `--file` 形式。
- 对复杂 JSON，优先放模板路径，而不是在正文里塞过长片段。
- 如果某能力还不稳定，直接写限制和已知风险，不要包装成完整能力。

## 安全边界

BlueprintHelper 的普通编辑路径应保持：

```text
read context -> author TaskSpec -> preview -> request write session when needed -> execute -> inspect result
```

贡献中不要引入让普通 Agent 绕过 preview、授权、TaskSpec 校验或结果检查的路径。低层级 Bridge、legacy MCP 或 expert/debug 能力只应服务内部诊断和兼容场景。
