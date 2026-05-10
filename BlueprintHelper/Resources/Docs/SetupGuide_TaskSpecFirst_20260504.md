# BlueprintHelper Setup Guide — TaskSpec-first Architecture Edition

日期：2026-05-04  
适用范围：BlueprintHelper v0.4 / v0.5 之后的用户引导、Setup Wizard、项目配置、Agent Skill 注入  
目标读者：插件用户、项目维护者、集成 MCP 的 Agent 用户  
文档性质：User Guidance & Setup Layer 文档；不是 Agent Skill 规则全文，也不是 MCP API Reference。

---

## 0. 本版收敛结论

Setup 文档需要从“让用户接通 MCP 工具”升级为“生成 Agent 可消费的 SetupProfile / runtime_profile / 项目引导文件”。

新的 Setup 目标是：

```text
1. 安装 UE 插件和 MCP Server。
2. 配置 UE_ENGINE_DIR / UE_PROJECT_FILE / Bridge / Token。
3. 生成 SetupProfile。
4. 采集用户的安全档位、蓝图/C++边界、命名偏好、输入系统偏好、Review/rollback 策略。
5. 让 runtime_profile 能稳定暴露当前运行时事实。
6. 让 Agent 默认走 TaskSpec-first 工作流。
```

Setup 不应让 Agent 在单次工具调用里临时覆盖安全档位。安全策略必须来自用户确认后的 SetupProfile。

---

## 1. Setup 文档边界

### 1.1 Setup Guide 负责

```text
1. 用户如何安装插件。
2. 用户如何配置 MCP Server。
3. 用户如何配置本地路径、Bridge、Token、风险命令开关。
4. 用户如何运行 /blueprinthelper-setup 或 Setup Wizard。
5. Setup Wizard 应采集哪些偏好。
6. SetupProfile 如何保存与生效。
7. runtime_profile 应暴露哪些摘要。
8. 如何生成或提示生成 CLAUDE.md / AGENTS.md 项目 Marker。
9. 如何运行 diagnostics 验证链路。
10. 常见 setup / runtime 阻断项如何处理。
```

### 1.2 Setup Guide 不负责

```text
1. Agent 如何生成完整 TaskSpec。
2. MCP 每个工具的完整参数。
3. UE 插件内部 C++ 实现。
4. Transaction Journal / Review Store 的完整数据结构。
5. 修改项目 C++ 或 Config 的具体代码策略。
```

---

## 2. Setup 后的目标状态

完成 Setup 后，应满足：

```text
1. UE 插件已安装并可在目标项目中加载。
2. MCP Server 可启动。
3. UE_ENGINE_DIR 和 UE_PROJECT_FILE 为绝对路径。
4. MCP Server 能连接 UE Bridge。
5. runtime_profile 可读取。
6. diagnostics 可返回 data.markdown。
7. SetupProfile 已保存。
8. Agent Skill 或项目 Marker 指向 BlueprintHelper Agent Guide。
9. Agent 默认使用 read_context / read_reference_context → preview_task → execute_task。
```

---

## 3. 推荐目录分层

保持现有插件目录时，建议放置：

```text
BlueprintHelper/
├─ Source/
├─ Content/
├─ Resources/
│  ├─ Docs/
│  │  ├─ SetupGuide_TaskSpecFirst_20260504.md
│  │  └─ AgentGuide_TaskSpecFirst_20260504.md
│  ├─ AgentGuide/
│  │  ├─ 00_Agent_Onboarding_Index_20260504.md
│  │  ├─ Reference/
│  │  └─ Workflows/
│  ├─ Skills/
│  │  └─ BlueprintHelper/
│  │     ├─ SKILL.md
│  │     ├─ taskspec_first_workflow.md
│  │     ├─ setup_runtime_profile.md
│  │     └─ capability_boundary_policy.md
│  └─ Plan/
└─ BlueprintHelper_MCP_Server/
```

项目根目录可生成轻量 Marker：

```text
CLAUDE.md
AGENTS.md
```

Marker 只负责指向插件文档与当前 SetupProfile 摘要，不应复制完整长文档。

---

## 4. 安装与配置顺序

### 4.1 安装 UE 插件

用户应确认：

```text
1. BlueprintHelper.uplugin 位于项目 Plugins 或 Engine Plugins 下。
2. Unreal Editor 能加载插件。
3. 插件 Bridge Server 可启动并监听本地端口。
4. 插件版本与 MCP Server 版本兼容。
```

如果版本不匹配，Setup 应视为 Blocking，不允许继续写入配置。

### 4.2 安装 MCP Server

用户应确认：

```text
1. BlueprintHelper_MCP_Server 可被目标 Agent 客户端启动。
2. Node / Python / 本地运行依赖满足当前 MCP Server 要求。
3. MCP Server 能读取配置文件或环境变量。
4. Agent 客户端已注册 BlueprintHelper MCP Server。
```

具体安装命令应由 MCP Server README / QuickStart 维护，Setup Guide 只规定配置项和验证流程。

### 4.3 配置本地路径

必须使用绝对路径：

```text
UE_ENGINE_DIR=<absolute path to Unreal Engine>
UE_PROJECT_FILE=<absolute path to .uproject>
```

规则：

```text
1. 不使用相对路径作为最终值。
2. 可允许 MCP Server 展开 ${workspaceFolder}，但展开后必须是绝对路径。
3. open_editor / build_project 依赖这些路径。
4. UE_PROJECT_FILE 必须指向实际 .uproject 文件。
```

### 4.4 配置 Bridge

建议配置项：

```json
{
  "bridge": {
    "host": "127.0.0.1",
    "port": 54321,
    "connect_timeout_ms": 3000,
    "request_timeout_ms": 30000
  }
}
```

Bridge 连接失败不一定表示 Setup 文件错误，可能是 UE Editor 未启动或插件未加载。

### 4.5 配置 Token 与风险命令

建议配置项：

```text
BLUEPRINTHELPER_BRIDGE_TOKEN
BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS
```

规则：

```text
1. 普通 UE 写操作应受 write_permission / token 校验约束。
2. close_editor / exec_console_command 等高风险命令应受 risk_command 控制。
3. risk_command.disabled 只阻断高风险 editor command，不应阻断普通蓝图读写。
4. Token 不应写入 Agent Guide 或项目 Marker。
```

---

## 5. Setup Wizard 必采集项

Setup Wizard 应采集以下项目，并生成 SetupProfile。

### 5.1 基础环境

```text
UE Engine 绝对路径
uproject 绝对路径
MCP Server 路径或启动方式
Bridge host / port
Agent 客户端类型：Claude Code / Codex / ChatGPT Agent / 其他
```

### 5.2 Safety Profile

用户选择：

```text
ReadOnly
Conservative
Standard
AutoRepair
Expert
```

推荐默认：

```text
Conservative
```

Setup UI 必须解释：

```text
ReadOnly：禁止真实写资产。
Conservative：允许写，但高风险必须 dry_run，不自动 save。
Standard：对 owned 内容更主动，仍不默认自动 save。
AutoRepair：只自动修复 BlueprintHelper-owned 内容。
Expert：允许低层高风险能力，但仍进入 Journal / Review。
```

### 5.3 missing_capability_policy

推荐选项：

```text
stop_and_report
ask_user
use_safe_fallback
```

默认：

```text
stop_and_report
```

Agent 遇到 runtime_profile unavailable capability 时按该策略执行。

### 5.4 自动保存策略

推荐选项：

```text
never_auto_save
allow_save_when_requested
allow_workflow_save
```

默认：

```text
never_auto_save
```

说明：Save 是落盘动作，不等同于 Review Accept。

### 5.5 Review / rollback 策略

采集：

```text
Review 默认是否开启
rollback_data 保留策略
accepted transaction 压缩策略
Reject 后是否运行 diagnostics / compile
是否允许 task-level RejectAll
```

推荐默认：

```text
review_enabled=true
rollback_data_retention=pending_full_then_compact
validate_after_reject=diagnostics
allow_task_level_reject_all=true
```

### 5.6 Blueprint / C++ 边界

Setup 应询问：

```text
1. Agent 是否允许修改 C++？如果允许，是否必须走普通代码工具而非 BlueprintHelper MCP？
2. 玩法逻辑优先写 Blueprint 还是 C++？
3. BlueprintHelper 是否只负责资产层修改？
4. Parent Class / Reparent 是否允许？
5. 配置文件是否允许 Agent 修改？
```

推荐默认：

```text
BlueprintHelper MCP 不修改 C++ / Config / Build.cs。
Parent Class 修改第一版不支持，相关任务 stop_and_report。
```

### 5.7 命名偏好

采集：

```text
feature graph 前缀：默认 EG_{FeatureName}
函数命名风格：描述型 PascalCase
Custom Event 命名风格：描述型 PascalCase
变量命名风格：UE 常规风格，如 bDoorOpen / OpenImpulse
组件命名风格：SceneRoot / DoorMesh / InteractionBox
禁止泛名：NewFunction / DoThing / Temp / MyVar
```

### 5.8 输入系统偏好

采集：

```text
是否允许 Agent 创建 InputAction
是否允许 Agent 修改 InputMappingContext
找不到 IA 时 stop 还是允许创建
多个 IA 候选时是否必须询问用户
```

推荐默认：

```text
不默认创建或修改 IA / IMC。
引用用户明确提供或唯一匹配的 IA。
多个候选 stop_and_report。
IA 事件入口属于 Graph Write，不代表 IMC 映射完成。
```

### 5.9 资源策略

采集：

```text
资产不存在时 fail / create_if_allowed / ask_user
已有同名资产时 error / reuse_if_type_matches
是否允许低层 factory_class / asset_class
是否允许自动打开 / 编译 / 保存
```

推荐默认：

```text
if_target_asset_missing=fail
if_referenced_asset_missing=fail
if_exists=error
allow_low_level_factory=false unless Expert
```

---

## 6. SetupProfile 建议结构

SetupProfile 示例：

```json
{
  "schema": "BlueprintHelper.SetupProfile.v1",
  "profile_name": "default",
  "created_at": "2026-05-04T00:00:00Z",
  "project": {
    "ue_engine_dir": "<UE_ENGINE_DIR>",
    "ue_project_file": "<PROJECT_FILE>"
  },
  "bridge": {
    "host": "127.0.0.1",
    "port": 54321
  },
  "active_profile": {
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report",
    "auto_save_policy": "never_auto_save"
  },
  "task_flow": {
    "default_agent_flow": "taskspec_first",
    "require_preview_before_execute": true,
    "prefer_task_context_pack": true
  },
  "blueprint_cpp_boundary": {
    "blueprinthelper_mcp_edits_cpp": false,
    "allow_reparent": false,
    "parent_class_policy": "stop_and_report"
  },
  "naming": {
    "feature_graph_pattern": "EG_{FeatureName}",
    "function_style": "descriptive_pascal_case",
    "forbidden_names": ["NewFunction", "DoThing", "Temp"]
  },
  "input_policy": {
    "auto_create_input_action": false,
    "auto_edit_input_mapping_context": false,
    "multiple_input_action_matches": "stop_and_report"
  },
  "review": {
    "enabled": true,
    "group_by_task_run_id": true,
    "allow_task_reject_all": true,
    "rollback_data_retention": "pending_full_then_compact"
  }
}
```

实际保存位置由插件实现决定，但建议属于 BlueprintHelper 自己的 Settings，不要把敏感 Token 写入项目 Marker。

---

## 7. runtime_profile 建议摘要

runtime_profile 不是 SetupProfile 全量镜像，也不是工具索引。它是当前运行时事实摘要。

建议返回：

```json
{
  "schema": "BlueprintHelper.RuntimeProfile.v1",
  "version": "0.5.0-dev",
  "bridge_status": "connected",
  "config_status": "valid",
  "write_permission": {
    "enabled": true,
    "reason": "ok"
  },
  "risk_command": {
    "enabled": false,
    "reason": "risk_command_missing"
  },
  "active_profile": {
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report"
  },
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": []
  }
}
```

不得在 runtime_profile 中放：

```text
完整 MCP tool schema
完整 SetupProfile 私密配置
Token
完整命名偏好长文
完整蓝图/C++边界问卷
完整 Review 历史
```

---

## 8. Project Marker 生成规则

Setup 可建议生成 `CLAUDE.md` / `AGENTS.md`，但不得静默写入。用户必须确认。

Marker 应保持短小：

```md
# BlueprintHelper Project Marker

This project uses BlueprintHelper for UE Editor asset operations.

Agent default workflow:
get_runtime_profile → read_context / read_reference_context → build TaskSpec → preview_task → execute_task → report summary.

Do not use BlueprintHelper MCP to edit C++ / Config / Build.cs / Target.cs files.
Use BlueprintHelper only for Unreal Editor asset operations.

Read:
- BlueprintHelper/Resources/Docs/AgentGuide_TaskSpecFirst_20260504.md
- BlueprintHelper/Resources/Docs/SetupGuide_TaskSpecFirst_20260504.md
```

Marker 不应包含：

```text
Token
本地绝对路径
完整 SetupProfile
完整 Agent Guide
完整 MCP schema
```

---

## 9. Setup 验证流程

### 9.1 Static diagnostics

不要求 UE Editor 正在运行。检查：

```text
version.match / version.mismatch
settings.valid / settings.unavailable
global_guidance.present / global_guidance.missing
skill_entry.valid / skill_entry.invalid
project_marker.present / project_marker.missing
```

规则：

```text
version.mismatch：Blocking。
settings.unavailable：Blocking。
global_guidance.missing 或 skill_entry.invalid：提示运行 setup。
project_marker.missing：通常 Warning，不自动写入。
```

### 9.2 Runtime diagnostics

要求 UE / MCP / Bridge 链路可测。检查：

```text
ue_editor.running / ue_editor.not_running
mcp_server.available / unavailable
bridge.connected / disconnected
runtime_profile.available / unavailable
config_status.valid / unavailable
write_permission.enabled / disabled
risk_command.enabled / disabled
```

规则：

```text
bridge.disconnected Blocking：不得调用 UE 写工具。
runtime_profile.unavailable Blocking：不得进入写入阶段。
config_status.unavailable：stop_and_report，提示关闭 UE 后运行 setup 或 reload。
write_permission.disabled：对写任务阻断，对只读任务不阻断。
risk_command.disabled：只阻断 high-risk editor command。
```

Diagnostics 成功执行但发现 Blocking 时仍应：

```text
ok=true
status=completed
```

Blocking 写在 `data.markdown`。

### 9.3 最小读写 smoke test

建议 Setup 后执行：

```text
1. get_runtime_profile。
2. diagnostics_runtime。
3. read_context 读取一个用户指定或测试 Blueprint；必要时 read_reference_context 读取引用影响面。
4. preview_task 一个不写资产的只读或 dry_run 示例。
5. 若用户允许，执行一个低风险测试任务，并编译但不自动保存。
```

ReadOnly Profile 下只做 1-4。

---

## 10. Setup 修改生效规则

### 10.1 通过 Setup Wizard 修改

```text
1. 用户在插件 UI 或 Setup 流程中确认。
2. 写入 Settings。
3. 立刻生效或通过明确 reload 生效。
4. 记录 Setup log / Settings change log。
5. runtime_profile 显示当前实际生效值。
```

### 10.2 直接修改 Settings 文件

```text
1. 属于非受控外部修改。
2. 默认需要重启 UE Editor / MCP Server 后生效。
3. 运行中检测到变化时可提示 reload。
4. 不允许静默切换到更高权限 Profile。
```

### 10.3 禁止 per-call 覆盖

SetupProfile 不可被单次工具调用覆盖。

禁止：

```text
temporary_profile
per_call_profile
one-shot Expert
force_write
no_review
no_journal
```

---

## 11. 与 TaskSpec-first Agent Guide 的关系

Setup Guide 输出给 Agent Guide 的只有摘要和事实来源：

```text
SetupProfile → runtime_profile.active_profile
SetupProfile → project marker / skill entry
SetupProfile → naming_preference_summary
SetupProfile → blueprint_cpp_boundary_summary
SetupProfile → missing_capability_policy
```

Agent Guide 不读取完整 Setup 问卷，也不修改 SetupProfile。

---

## 12. 兼容旧直调工具的迁移

旧流程：

```text
Agent 直接调用 asset_create → add_component → set_component_properties → add_interface → append_graph → compile → save
```

新流程：

```text
Agent 生成 TaskSpec
→ preview_task
→ execute_task
→ UE Task Runtime 调用内部 capability
```

迁移规则：

```text
1. 保留底层工具作为 debug / expert / 测试入口。
2. 普通 Agent Skill 不再主推底层工具序列。
3. /agentplan 输出 TaskSpec / TaskPlan 摘要，而不是完整底层 MCP 调用列表。
4. Review UI 按 task_run_id 分组展示用户可理解的任务。
```

---

## 13. Troubleshooting 矩阵

| 现象 | 可能原因 | 用户动作 | Agent 动作 |
|---|---|---|---|
| `runtime_profile.unavailable` | SetupProfile 缺失、MCP/UE 版本不匹配、Bridge 未连接 | 运行 setup / 重启 UE / 检查版本 | 不进入写入；可调用 diagnostics |
| `bridge.disconnected` | UE Editor 未开、插件未加载、端口错误 | 打开目标项目并启用插件 | stop_and_report |
| `write_permission.disabled` | Token 缺失或 Profile 禁止写 | 配置 Token 或切换 Profile | 只读任务可继续；写任务停止 |
| `risk_command.disabled` | 高风险命令未启用 | 显式启用风险命令 | 只阻断 close_editor / exec_console_command |
| `version.mismatch` | UE 插件与 MCP Server 不兼容 | 升级或回退版本 | 不继续 setup / 写入 |
| `project_marker.missing` | 未生成 CLAUDE.md / AGENTS.md | 可选择生成 | 不自动写项目文件 |
| `preview_blocked` | TaskSpec 合法但策略或状态阻断 | 修改目标或授权 | 修 TaskSpec 或 stop |
| `context_stale` | 上下文过期 | 无需手动处理 | 重新 read_context / read_reference_context |
| `ProfilePolicyViolation` | 工具参数与 SetupProfile 冲突 | 通过 Setup Wizard 调整 | 不自动降级执行 |

---

## 14. Setup 验收标准

```text
1. SetupProfile 可被保存并被 runtime_profile 摘要化。
2. safety_profile 只通过 runtime_profile.active_profile 暴露给 Agent。
3. runtime_profile.tool_capabilities 使用 unavailable_only。
4. diagnostics 返回 data.markdown，Blocking 不等于工具失败。
5. Project Marker 不包含 Token 和本地绝对路径。
6. Agent Skill 指向 TaskSpec-first 流程。
7. 旧底层工具被标注为 capability / debug / expert / 测试入口。
8. Setup 不允许 per-call Profile override。
9. ReadOnly 下 execute_task 写资产被阻断。
10. Conservative 下高风险任务 preview / dry_run 阻断后不会 execute。
11. task_run_id / TaskRunJournal 用于任务级 Review 分组。
12. 用户能通过 diagnostics 明确定位 Bridge、config、runtime_profile、write_permission 问题。
```
