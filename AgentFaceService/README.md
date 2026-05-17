# BlueprintHelper AgentFaceService

## 中文

`AgentFaceService` 是 Claude 和 Codex 插件壳共享的运行层。插件壳只保留各自的 manifest、skills、commands、subagents 和少量入口文档；CLI、Python 编排、Bridge client、Agent-facing 指南和全局 MCP lifecycle companion 都放在这里。

### 内容

- `agent-guide/`: Agent-facing onboarding、工作流、字段说明和 JSON 模板的规范位置。
- `docs/`: CLI、TaskSpec 和共享契约文档的规范位置。
- `task-core/`: Bridge client、TaskSpec schema、Python 编排、共享 task runner、结果 helper 和活动工具注册表。
- `cli/`: shell-capable Agents 使用的 BlueprintHelper CLI transport。
- `mcp/`: 长驻全局 MCP companion，用于 editor launch/lifecycle。
- `scripts/`: 共享 package build helper。

### 安装与构建

安装、构建和打包说明集中在 `docs/Install_CLI_QuickStart.md`。

CLI 入口：

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js --help
```

全局 MCP lifecycle companion 入口：

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\mcp\build\index.js
```

## English

`AgentFaceService` is the runtime layer shared by the Claude and Codex plugin shells. Plugin shells should keep only their manifests, skills, commands, subagents, and small entry docs; CLI, Python orchestration, Bridge client code, Agent-facing guidance, and the global MCP lifecycle companion live here.

### Contents

- `agent-guide/`: canonical Agent-facing onboarding, workflows, field guidance, and JSON templates.
- `docs/`: canonical CLI, TaskSpec, and shared contract documentation.
- `task-core/`: Bridge client, TaskSpec schemas, Python orchestration, shared task runner, result helpers, and active tool registry.
- `cli/`: BlueprintHelper CLI transport used by shell-capable Agents.
- `mcp/`: long-lived global MCP companion for editor launch/lifecycle.
- `scripts/`: shared package build helpers.

### Install And Build

Install, build, and packaging notes live in `docs/Install_CLI_QuickStart.md`.

CLI entry:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js --help
```

Global MCP lifecycle companion entry:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\mcp\build\index.js
```
