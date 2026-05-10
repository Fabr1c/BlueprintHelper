---
description: Configure BlueprintHelper user preferences and safety profile using guided forms
argument-hint: [setup-profile-path]
allowed-tools: Read, Write, AskUserQuestion
---

# BlueprintHelper Configure

你正在更新 BlueprintHelper 用户偏好和 safety profile。这个命令负责已有配置的更新，也可以在偏好文件缺失时生成完整默认偏好文件。

插件命令通过插件根目录 `commands/configure.md` 自动注册。用户可运行：

```text
/blueprint-helper:configure
```

---

## FIRST：读取现有配置

按顺序读取：

1. `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`
2. `$ARGUMENTS` 指定的 SetupProfile 路径，如果用户提供
3. 如果 `$ARGUMENTS` 为空，询问用户 SetupProfile 路径，默认建议 `<ProjectDir>/.blueprinthelper/agent-profile.json`

如果用户不提供 SetupProfile 路径，只更新用户偏好文件，并在最终报告中明确 `SetupProfile not updated`。

记录当前状态：

```text
UserPreferences: exists / missing
SetupProfile: exists / missing / skipped
Current safety_profile: <value or unknown>
Current missing_capability_policy: <value or unknown>
Current auto_save_policy: <value or unknown>
```

---

## 原生交互表单规则

所有配置问题都必须调用 `AskUserQuestion` 输出原生交互表单。不要把下面的 YAML 或选项列表当作普通 Markdown 打印给用户，除非当前 Claude 环境没有 `AskUserQuestion` 工具。

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
- 生成完整的 `AskUserQuestion` 表单。
- 推荐项必须标注 `(Recommended)`。
- `multiSelect: true` 只用于可叠加偏好。
- 选项应根据当前配置动态过滤。
- 如果用户选择自定义文本，再用 AskUserQuestion 获取自由文本。
- 用户取消时，输出 `Configuration cancelled.`，不要写入文件。
- 如果 `AskUserQuestion` 不可用，才退回 Markdown 文本表单，并明确说明当前环境不支持原生表单。

---

## Flow：完整配置更新

问题顺序：Safety Profile → Task Policy → Boundary → Graph/Input/Asset → Review/Debug → Collaboration → Custom Note。

### Q1：Safety Profile

Use AskUserQuestion:
- header: "Safety"
- question: "选择新的 BlueprintHelper safety profile："
- multiSelect: false
- options:
  - label: "Conservative (Recommended)"
    description: "preview 必须通过，不自动 save，不启用高风险命令"
  - label: "ReadOnly"
    description: "只读，不执行真实 UE 资产写入"
  - label: "Standard"
    description: "preview 必须通过，用户请求时可 save"
  - label: "AutoRepair"
    description: "可自动修复 BlueprintHelper-owned 内容"
  - label: "Expert"
    description: "允许更多高风险能力，但仍保留 Journal / Review 约束"
  - label: "Keep current"
    description: "保持当前 safety profile"

### Q2：Task Policy

Use AskUserQuestion:
- header: "Task Policy"
- question: "TaskSpec、fallback 和保存策略："
- multiSelect: true
- options:
  - label: "TaskSpec-first (Recommended)"
    description: "默认走 read context -> TaskSpec -> preview -> execute"
  - label: "Stop on missing capability (Recommended)"
    description: "能力缺失时停止报告，不回退到底层冻结入口"
  - label: "Ask on missing capability"
    description: "能力缺失时先询问用户"
  - label: "Debug tools fallback"
    description: "允许诊断/debug 工具定位问题"
  - label: "Legacy direct fallback"
    description: "允许直接调用底层 MCP 工具"
  - label: "No auto save (Recommended)"
    description: "默认不自动 save"
  - label: "Save when requested"
    description: "用户明确要求时允许 save"
  - label: "Workflow save"
    description: "工作流验证通过后可 save"

### Q3：Boundary

Use AskUserQuestion:
- header: "Boundary"
- question: "工程边界偏好："
- multiSelect: true
- options:
  - label: "UE assets through MCP (Recommended)"
    description: "BlueprintHelper MCP 只处理 UE 编辑器资产"
  - label: "Repo files through normal tools (Recommended)"
    description: "C++、TS、Python、JSON、文档用普通仓库工具"
  - label: "No C++ edits by default (Recommended)"
    description: "默认不修改 C++ 源码"
  - label: "Allow C++ edits when requested"
    description: "用户明确要求时代码编辑可走普通仓库工具"
  - label: "No reparent by default (Recommended)"
    description: "Parent Class 修改不支持时 stop_and_report"
  - label: "No active tab writes (Recommended)"
    description: "不依赖当前聚焦标签页执行破坏性写入"

### Q4：Graph/Input/Asset

Use AskUserQuestion:
- header: "Graph"
- question: "Graph Write、命名、输入和资产策略："
- multiSelect: true
- options:
  - label: "EG_{FeatureName} graphs (Recommended)"
    description: "新 EventGraph 使用 EG_{FeatureName}"
  - label: "Descriptive PascalCase (Recommended)"
    description: "函数和 Custom Event 使用描述型 PascalCase"
  - label: "UE variable style (Recommended)"
    description: "变量使用 UE 常规风格"
  - label: "Reject generic names (Recommended)"
    description: "拒绝 NewFunction、DoThing、Temp、MyVar"
  - label: "Do not modify user nodes (Recommended)"
    description: "默认不修改用户已有节点"
  - label: "Do not merge existing exec flow (Recommended)"
    description: "默认不接入既有执行流"
  - label: "No auto IA/IMC edits (Recommended)"
    description: "默认不创建 IA，不编辑 IMC"
  - label: "Missing assets fail (Recommended)"
    description: "缺失目标或引用资产时停止报告"

### Q5：Review/Debug

Use AskUserQuestion:
- header: "Review"
- question: "Review、rollback 和 Debug 证据策略："
- multiSelect: true
- options:
  - label: "Enable Journal (Recommended)"
    description: "启用 Transaction Journal"
  - label: "Enable Review Store (Recommended)"
    description: "启用 Review Store"
  - label: "Keep rollback until accepted (Recommended)"
    description: "Pending 保留完整回滚数据，接受后可压缩"
  - label: "Validate after reject (Recommended)"
    description: "Reject 后运行 diagnostics 或 compile 检查"
  - label: "Summary-only MCP debug (Recommended)"
    description: "MCP 只查 DebugCase 摘要"
  - label: "DebugBundle summary.md + artifacts (Recommended)"
    description: "本地 DebugBundle 导出保持 summary.md + artifacts/"

### Q6：Collaboration

Use AskUserQuestion:
- header: "Workflow"
- question: "Agent 协作偏好："
- multiSelect: true
- options:
  - label: "Read AGENTS first (Recommended)"
    description: "任务开始先读仓库 AGENTS.md 或当前等价规则"
  - label: "Prefer concise Chinese (Recommended)"
    description: "用户未要求时减少括号说明，中文回复直接"
  - label: "Use parallel workers (Recommended)"
    description: "独立读档、diff、测试和实现任务尽量并发"
  - label: "Precise completion claims (Recommended)"
    description: "工作区脏或未验证时不说完全完成"
  - label: "Write compaction memory (Recommended)"
    description: "上下文接近上限时写入 .codex/memory 进度文档"

### Q7：Custom Note

Use AskUserQuestion:
- header: "Custom Note"
- question: "是否添加或更新用户自定义偏好备注？"
- multiSelect: false
- options:
  - label: "Keep current (Recommended)"
    description: "保留当前备注"
  - label: "Enter custom text"
    description: "用 AskUserQuestion 采集一段不超过 500 字的备注"
  - label: "Remove"
    description: "清空当前备注"

---

## Safety Profile Mapping

将 Q1 和 Q2 映射到 SetupProfile。写入时合并现有 JSON，保留未知字段。

### Conservative

```json
{
  "active_profile": {
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report",
    "auto_save_policy": "never_auto_save"
  },
  "safety": {
    "safety_profile": "Conservative",
    "preview_required": true,
    "allow_auto_save": false,
    "allow_high_risk_editor_commands": false,
    "allow_temporary_profile_override": false
  },
  "agent": {
    "agent_entry_mode": "task_spec_first",
    "fallback_when_task_tools_unavailable": "stop_and_report",
    "missing_capability_policy": "stop_and_report"
  }
}
```

### ReadOnly

- `safety_profile`: `ReadOnly`
- `preview_required`: true
- `allow_auto_save`: false
- `allow_high_risk_editor_commands`: false
- `fallback_when_task_tools_unavailable`: `stop_and_report`

### Standard

- `safety_profile`: `Standard`
- `preview_required`: true
- `allow_auto_save`: false unless Q2 selected `Workflow save`
- `allow_high_risk_editor_commands`: false

### AutoRepair

- `safety_profile`: `AutoRepair`
- `preview_required`: true
- `allow_auto_save`: false unless Q2 selected `Workflow save`
- 自动修复仅限 BlueprintHelper-owned 内容

### Expert

- `safety_profile`: `Expert`
- `preview_required`: true
- `allow_high_risk_editor_commands`: true
- 不允许写入 token 或绕过 Journal / Review

---

## User Preference File Mapping

写入 `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`。

必须保留固定标题：

```markdown
# 08 - BlueprintHelper User Preferences
```

必须包含：
- `schema: BlueprintHelper.UserPreferences.v1`
- `generated_by: ClaudePlugin/commands/configure.md`
- `saved_at`
- `source: configure_user_preference_wizard`
- `## Purpose`
- `## Active Preferences`
- `## SetupProfile Separation`
- `## Collaboration Preferences`
- `## Debug And Review Preferences`
- `## Manual Notes`

如果现有文件中有人工备注，除非 Q7 选择 Remove，否则保留并追加新备注。

---

## 写入前预览

写入前必须展示：

```text
BlueprintHelper Configure Preview

SafetyProfile:
  <old> -> <new>

SetupProfile:
  Path: <path or skipped>
  missing_capability_policy: <old> -> <new>
  auto_save_policy: <old> -> <new>

UserPreferences:
  Task Flow: <summary>
  Boundary: <summary>
  Graph/Input/Asset: <summary>
  Review/Debug: <summary>
  Collaboration: <summary>

Write files:
  - ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
  - <SetupProfile path, if provided>
```

然后用原生确认表单询问：

Use AskUserQuestion:
- header: "Save"
- question: "Save these changes?"
- multiSelect: false
- options:
  - label: "Save (Recommended)"
    description: "写入用户偏好文件，并在提供路径时更新 SetupProfile"
  - label: "Cancel"
    description: "不写入文件并退出 configure"

守卫：
- 用户取消：输出 `Configuration cancelled.`
- 无变化：输出 `No changes needed - configuration unchanged.`
- SetupProfile 路径缺失：只写用户偏好文件，并在报告里写 `SetupProfile not updated`

---

## 写入后报告

成功后输出：

```text
BlueprintHelper Configure saved

UserPreferences: ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md
SetupProfile: <path or not updated>
Safety: <safety_profile>
Entry Mode: task_spec_first
Fallback: <fallback_policy>
Auto Save: <auto_save_policy>
```
