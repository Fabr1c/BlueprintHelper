---
description: Run BlueprintHelper initial setup — configure UE paths, verify MCP Bridge connectivity, collect safety preferences, and generate SetupProfile
allowed-tools: Read, Write, AskUserQuestion
---

# BlueprintHelper Setup

你正在运行 BlueprintHelper 初次配置。按以下步骤逐一完成，不要跳过。

---

## 阶段 1：路径配置

### 1.1 获取 UE 引擎目录

询问用户并确认 `UE_ENGINE_DIR` 的绝对路径，文档中用 `<UE_ENGINE_DIR>` 表示该路径。

验证规则：
- 必须是绝对路径
- 路径下必须存在 `Engine/Binaries/Win64/UnrealEditor.exe`（或对应的平台二进制文件）
- 如果不存在，提示用户重新输入

### 1.2 发现项目文件

Agent 使用普通仓库工具在当前项目工作区发现目标 `.uproject` 文件，文档中用 `<PROJECT_FILE>` 表示该路径。

不要把项目路径写入全局 Claude settings、插件 env、SetupProfile 或 RuntimeProfile。项目路径只在调用 `blueprint_open_editor`、`blueprint_build_project` 等工具时作为显式 `project_file` 参数传入。

验证规则：
- 必须是绝对路径
- 必须以 `.uproject` 结尾
- 文件必须存在
- 如果当前工作区下无法唯一确定目标 `.uproject`，停止并询问用户，不要回退到 `UE_PROJECT_FILE`

### 1.3 确认 BlueprintHelper 插件已安装

检查以下条件：
- BlueprintHelper 插件目录存在于项目 `Plugins/` 或引擎 `Engine/Plugins/` 下
- `BlueprintHelper.uplugin` 文件存在且 `VersionName` 与 MCP Server `package.json` 版本兼容

---

## 阶段 2：MCP Server 构建

### 2.1 安装依赖并构建

在 `ClaudePlugin/mcp/` 目录下执行：

```powershell
npm install
npm run build
```

### 2.2 验证 MCP Server 可启动

执行 `node build/index.js --help` 或仅检查 `build/index.js` 存在。

---

## 阶段 3：Bridge 连通性

### 3.1 确认 Unreal Editor 状态

检查以下之一：
1. Unreal Editor 正在运行且已加载 BlueprintHelper 插件
2. 如果 Editor 未运行，确认 `open_editor` 工具可用（依赖 `UE_ENGINE_DIR` 和显式 `project_file` 参数）

### 3.2 验证 Bridge 连接

Bridge 默认地址 `127.0.0.1:54321`。

如果可以调用 MCP 工具，使用 `blueprinthelper_diagnostics` 检查：
- `bridge_status` 应为 `connected`
- 如果不是，报告阻断原因并让用户排查

---

## 阶段 4：User Preference Wizard

**FIRST**：使用 Read 读取 `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`。

如果文件已存在，不要在 setup 中运行偏好更新流程。向用户展示当前偏好摘要，并说明更新已有偏好和 safety profile 请运行 `/blueprint-helper:configure`。如果用户明确要求重新初始化偏好，可继续执行下面的新用户 Flow A 并覆盖该文件。

这些偏好生成独立的用户偏好文件，不写入 SetupProfile、RuntimeProfile 或项目 Marker。SetupProfile 只保存机器可执行配置；用户偏好文件保存 Agent 行为、协作和文档读取约定。

### 原生交互表单规则

所有偏好采集问题都必须调用 `AskUserQuestion` 输出原生交互表单。不要把下面的 YAML 或选项列表当作普通 Markdown 打印给用户，除非当前 Claude 环境没有 `AskUserQuestion` 工具。

每个 Q 都是一个完整的 `AskUserQuestion` payload：

```yaml
Use AskUserQuestion:
  header: "短标题"
  question: "给用户看的问题"
  multiSelect: false
  options:
    - label: "选项名 (Recommended)"
      description: "影响说明"
```

规则：
- 生成完整 `AskUserQuestion` 表单
- 推荐项必须标注 `(Recommended)`。
- `multiSelect: true` 只用于可叠加偏好。
- 用户选择自定义文本时，再用 AskUserQuestion 获取自由文本。
- 用户取消时，输出 `Setup cancelled.`，不要写文件。
- 如果 `AskUserQuestion` 不可用，才退回 Markdown 文本表单，并明确说明当前环境不支持原生表单。

### Flow A：新用户

问题顺序：Safety → Task Flow → Save/Validation → Boundary → Graph/Naming → Input/Asset → Review/Debug → Collaboration。

#### Q1：Safety

Use AskUserQuestion:
- header: "Safety"
- question: "选择 BlueprintHelper 写入安全档位："
- multiSelect: false
- options:
  - label: "Conservative (Recommended)"
    description: "允许写入，但必须 preview + 用户确认，不自动 save"
  - label: "ReadOnly"
    description: "只读模式，拒绝所有写入"
  - label: "Standard"
    description: "允许写入，必须 preview，可按请求 save"
  - label: "AutoRepair"
    description: "允许自动修复 BlueprintHelper-owned 内容"
  - label: "Expert"
    description: "允许更少确认和更高风险能力"

#### Q2：Task Flow

Use AskUserQuestion:
- header: "Task Flow"
- question: "TaskSpec-first 不可用或能力缺失时怎么处理？"
- multiSelect: false
- options:
  - label: "Stop and report (Recommended)"
    description: "停止并说明缺口，不回退到底层冻结入口"
  - label: "Ask user"
    description: "先询问用户是否调整目标或授权替代路径"
  - label: "Debug tools only"
    description: "允许使用诊断/debug 工具定位问题"
  - label: "Legacy direct allowed"
    description: "允许直接调用底层 MCP 工具"

#### Q3：Save/Validation

Use AskUserQuestion:
- header: "Save"
- question: "保存和验证策略："
- multiSelect: false
- options:
  - label: "Preview + no auto save (Recommended)"
    description: "preview 是写入门禁，默认不自动 save"
  - label: "Save when requested"
    description: "用户明确要求时可 save"
  - label: "Workflow save"
    description: "通过验证后可由工作流 save"

#### Q4：Boundary

Use AskUserQuestion:
- header: "Boundary"
- question: "Agent 可以触碰哪些工程边界？"
- multiSelect: true
- options:
  - label: "UE assets through MCP (Recommended)"
    description: "BlueprintHelper MCP 只处理 UE 编辑器资产"
  - label: "Repo files through normal tools (Recommended)"
    description: "C++、TS、Python、JSON、文档用普通仓库工具"
  - label: "No C++ edits by default (Recommended)"
    description: "默认不修改 C++ 源码"
  - label: "No reparent by default (Recommended)"
    description: "Parent Class 修改不支持时 stop_and_report"
  - label: "No active tab writes (Recommended)"
    description: "不依赖当前聚焦编辑器标签执行破坏性操作"

#### Q5：Graph/Naming

Use AskUserQuestion:
- header: "Graph"
- question: "Graph Write 和命名偏好："
- multiSelect: true
- options:
  - label: "EG_{FeatureName} graphs (Recommended)"
    description: "新 EventGraph 默认使用 EG_{FeatureName}"
  - label: "Descriptive PascalCase (Recommended)"
    description: "函数和 Custom Event 使用描述型 PascalCase"
  - label: "UE variable style (Recommended)"
    description: "变量使用 bDoorOpen、OpenImpulse 等 UE 常规风格"
  - label: "Do not modify user nodes (Recommended)"
    description: "默认不修改用户已有节点"
  - label: "Do not merge existing exec flow (Recommended)"
    description: "默认不接入用户已有执行流"
  - label: "Reject generic names (Recommended)"
    description: "禁止 NewFunction、DoThing、Temp、MyVar"

#### Q6：Review/Debug

Use AskUserQuestion:
- header: "Review"
- question: "Review、回滚和 Debug 证据偏好："
- multiSelect: true
- options:
  - label: "Enable Journal (Recommended)"
    description: "启用 Transaction Journal"
  - label: "Enable Review Store (Recommended)"
    description: "启用 Review Store"
  - label: "Keep rollback until accepted (Recommended)"
    description: "Pending 保留完整回滚数据，接受后可压缩"
  - label: "Summary-only MCP debug (Recommended)"
    description: "MCP 只查 DebugCase 摘要，不读取 DebugBundle artifact 内容"
  - label: "DebugBundle local export shape (Recommended)"
    description: "UE/本地导出保持 summary.md + artifacts/"

#### Q7：Collaboration

Use AskUserQuestion:
- header: "Workflow"
- question: "Agent 协作和回复偏好："
- multiSelect: true
- options:
  - label: "Read AGENTS first (Recommended)"
    description: "任务开始先读仓库 AGENTS.md 或当前等价规则"
  - label: "Prefer concise Chinese (Recommended)"
    description: "用户未要求时减少括号说明，回复直接"
  - label: "Use parallel workers (Recommended)"
    description: "最大程度并发处理独立读档、diff、测试和实现任务"
  - label: "Precise completion claims (Recommended)"
    description: "工作区脏或未验证时不说完全完成"
  - label: "Write compaction memory (Recommended)"
    description: "上下文接近上限时写入 .codex/memory 进度文档"

### 更新已有偏好

已有用户偏好和 safety profile 的更新流程已迁移到 `ClaudePlugin/commands/configure.md`。

使用方式：

```text
/blueprint-helper:configure
```

Setup 只负责首次配置。如果用户在 setup 中要求修改已有偏好，停止当前偏好采集并转交 configure 命令，不要在 setup 中复制更新流程。

### 处理逻辑

新用户：
1. 从 Conservative 默认集开始。
2. 应用每个表单选择。
3. 生成用户偏好文件预览。

### 写入前预览

写入前必须展示：

```text
User Preferences Preview

Safety: Conservative
Task Flow: TaskSpec-first, stop_and_report
Boundary: UE assets via MCP; repo files via normal tools; no C++ by default
Debug: MCP summary-only; local DebugBundle summary.md + artifacts/
Collaboration: AGENTS first; parallel workers; precise completion claims

Write to:
ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

然后用原生确认表单询问：

Use AskUserQuestion:
- header: "Save"
- question: "Save these preferences?"
- multiSelect: false
- options:
  - label: "Save (Recommended)"
    description: "写入用户偏好文件"
  - label: "Cancel"
    description: "不写入文件并退出 setup"

如果用户确认，写入 `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`。如果无变化，输出 `No changes needed - user preferences unchanged.`

### 用户偏好文件输出格式

必须写成 Markdown，使用固定标题：

```markdown
# 08 - BlueprintHelper User Preferences
```

必须包含：
- `schema: BlueprintHelper.UserPreferences.v1`
- `generated_by: ClaudePlugin/commands/setup.md`
- `saved_at`
- `source: setup_user_preference_wizard`
- `## Purpose`
- `## Active Preferences`
- `## SetupProfile Separation`
- `## Collaboration Preferences`
- `## Debug And Review Preferences`
- `## Manual Notes`

---

## 阶段 5：生成 SetupProfile

基于用户答案构造 `BlueprintHelper.SetupProfile.v1` JSON，写入项目配置目录。

推荐的保存路径：`<ProjectDir>/.blueprinthelper/agent-profile.json`

示例结构参见 `Resources/Docs/Setup/SetupProfile_Example.json`。

注意：不要把 `08_User_Preferences.md` 的长文本偏好写入 SetupProfile。SetupProfile 只保存安全档位、fallback、自动保存、边界等可执行配置摘要。不要把项目 `.uproject` 路径、`project_file` 或旧的 `UE_PROJECT_FILE` 写入 SetupProfile。

---

## 阶段 5.5：全局 Claude Settings 预检

在进入验证阶段前，使用 Read 读取 `~/.claude/settings.json`。

检查规则：
- `env.UE_ENGINE_DIR` 应存在，并指向有效 UE Engine 目录
- 不要写入或更新 `env.UE_PROJECT_FILE`
- 如果发现已有 `env.UE_PROJECT_FILE`，报告它已被 BlueprintHelper 插件弃用且会被忽略；可建议用户手动清理，但 setup 不自动修改全局 settings
- 项目 `.uproject` 路径继续由 Agent 从当前工作区发现，并在工具调用时显式传入 `project_file`

---

## 阶段 6：验证

### 6.1 runtime_profile 可读

调用 `blueprinthelper_get_runtime_profile`，验证：
- `active_profile.safety_profile` 与用户选择一致
- `bridge_status` 为 `connected`
- `config_status` 为 `valid`

### 6.2 diagnostics 通过

调用 `blueprinthelper_diagnostics`，确认：
- 无 Blocking 项
- Warning 项已知且可接受
- Info 项显示链路完整

### 6.3 生成项目 Marker（可选）

如果项目根目录存在 `CLAUDE.md` 或 `AGENTS.md`，询问是否需要添加 BlueprintHelper 引用指针：

```markdown
## BlueprintHelper

本项目使用 BlueprintHelper 进行 UE 编辑器资产操作。Agent 请遵循 skill `blueprint-helper` 的 TaskSpec-first 流程。
SetupProfile: <ProjectDir>/.blueprinthelper/agent-profile.json
UserPreferences: ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

---

## 阶段 7：报告

Setup 完成后输出摘要：

```text
BlueprintHelper Setup 完成

UE Engine:  <UE_ENGINE_DIR>
UE Project: <discovered .uproject used as project_file only>
Bridge:     <host:port> — <status>
Safety:     <safety_profile>
Entry Mode: task_spec_first
Fallback:   <fallback_policy>

SetupProfile 已保存至: <path>
UserPreferences 已保存至: ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
```

如果任何阶段被阻断，停止并报告具体阻断原因，不要跳过。
