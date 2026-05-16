# BlueprintHelper

BlueprintHelper 是一个面向 Unreal Engine 编辑器资产的辅助插件，核心目标是让 AI Agent 能够通过本地 CLI 和 Unreal Editor Bridge 安全地读取、预览和修改蓝图相关资产。

当前版本：`v0.4.1`

## 插件用途

BlueprintHelper 用于把 Agent 的高层编辑意图转换成 Unreal Editor 内可验证、可预览、可执行的资产操作。它不是通用源码编辑器，而是专注于编辑器资产工作流：

- 读取 Blueprint 图表、变量、函数、宏、组件、接口、节点和连线信息。
- 通过 `BlueprintHelper.TaskSpec.v1` 描述写入意图，并在执行前生成预览。
- 修改 Blueprint 逻辑、结构、组件、类设置、事件分发器等编辑器资产内容。
- 读取和修改 UMG Widget 树、Widget 属性、DataAsset 属性和 DataTable 行数据。
- 执行编译、保存、PIE、诊断等 Unreal Editor 相关操作；Agent 控制 Editor 启动/关闭时使用全局 MCP 生命周期工具。

## 使用范围

适合使用 BlueprintHelper 的场景：

- AI Agent 需要理解现有 Blueprint 资产结构。
- 需要批量或半自动生成 Blueprint 节点、函数、变量、组件或 UMG 控件。
- 需要在执行写入前检查变更范围、预览结果并降低误改风险。
- 需要把编辑器资产操作纳入可追踪的 CLI 工作流。
- 需要在 Codex、Claude Code 等 Agent 环境中访问 Unreal Editor 资产。

不适合使用 BlueprintHelper 的场景：

- C++、TypeScript、Python、JSON、配置文件、文档等普通仓库文件编辑。
- 通用文件搜索、代码搜索、构建脚本维护。
- 绕过预览和授权流程的低层级 Bridge 写入。
- 依赖当前编辑器焦点标签页进行破坏性操作，除非用户明确要求。

## 核心特色

- CLI-first 普通资产入口：默认通过 `bh <tool_name>` 访问 TaskSpec、ReadSpec、诊断和结果查询能力，适合普通 shell-capable Agent 集成。
- TaskSpec-first 写入架构：Agent 只提交语义化 `BlueprintHelper.TaskSpec.v1`，底层 TaskPlan 由 task-core 和编译器生成。
- 写入前预览：普通编辑流程先读取上下文，再 preview，最后 execute，减少盲写和误操作。
- Template-first JSON 编写：AgentGuide 提供可复制模板，减少 CLI 字段形态和 shell 转义错误。
- Unreal Editor Bridge：通过本地 Bridge 与正在运行的 Unreal Editor 通信，读写真实编辑器资产。
- 多资产面支持：覆盖 Blueprint、UMG、DataAsset、DataTable、编译保存、打开资产和运行时诊断。
- 安全边界清晰：源码文件仍由常规仓库工具处理，编辑器资产由 BlueprintHelper 处理。
- Agent 插件适配：仓库包含 CodexPlugin、ClaudePlugin 和共享的 AgentFaceService 运行时。

## 架构概览

```text
Agent
  -> BlueprintHelper CLI for ordinary reads/writes
  -> AgentFaceService task-core
  -> TaskSpec compiler / read router
  -> Unreal Editor Bridge
  -> UE Task Runtime
  -> BlueprintHelper capability clusters
```

Editor lifecycle boundary:

```text
Agent -> global BlueprintHelper MCP lifecycle tool -> open/close target Unreal Editor
```

普通写入流程：

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_task_context 或 blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> blueprinthelper_request_write_session when needed
-> blueprinthelper_execute_task
-> blueprinthelper_get_task_result when needed
```

## 仓库结构

- `BlueprintHelper/`：Unreal Engine Editor 插件主体。
- `AgentFaceService/task-core/`：Bridge client、TaskSpec schema、任务编排和工具注册。
- `AgentFaceService/cli/`：Agent 使用的 CLI 入口。
- `AgentFaceService/mcp/`：全局 MCP 生命周期入口和兼容/调试支持。
- `CodexPlugin/`：Codex 插件封装和 skill。
- `ClaudePlugin/`：Claude Code 插件封装和文档。

## 快速开始

1. 将仓库放入 Unreal 项目的插件目录，例如 `YourProject/Plugins/BlueprintHelper`。
2. 在 Unreal Editor 中启用 BlueprintHelper，并按需重新编译项目。
3. 构建共享 task-core 和 CLI：

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

4. 启动目标 Unreal Editor 项目；Agent 工作流需要代管启动/关闭时使用全局 MCP 生命周期工具。
5. 使用 CLI 检查运行状态：

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

如果 `bh` 不在 PATH 中，可以直接使用：

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

## 版本同步

`v0.4.1` 同步到以下层级：

- Unreal 插件：`BlueprintHelper/BlueprintHelper.uplugin`
- CLI：`AgentFaceService/cli/package.json`
- 共享 task-core：`AgentFaceService/task-core/package.json`
- MCP 生命周期/兼容入口：`AgentFaceService/mcp/package.json`
- Codex 插件 manifest：`CodexPlugin/.codex-plugin/plugin.json`
- Claude 插件 manifest：`ClaudePlugin/.claude-plugin/plugin.json`

## 许可证

MIT License. See `LICENSE`.
