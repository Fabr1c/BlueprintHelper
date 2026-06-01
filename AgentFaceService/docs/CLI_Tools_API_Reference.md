# BlueprintHelper CLI 工具 API 参考

文档版本: `2026-05-31`
适用实现: `BlueprintHelper v0.5.4`

这份文档只描述当前实现，不保留旧的乱码写法或已废弃的兼容叙述。

## 适用范围

本文档面向可以运行 shell 的 Agent，说明 `bh` CLI 当前暴露的工具面、输入形状、模板入口和边界。

核心规则只有一条:

- 普通任务走 CLI。
- 编辑器打开/关闭走全局 MCP 生命周期工具。
- 写入流程必须 TaskSpec-first。
- 复杂 JSON 输入优先复制模板，再替换占位符后通过 `--file` 或 `--stdin` 传入。

## 统一输入输出约定

### 输入优先级

1. `--file` 适合复用模板和较大的 JSON。
2. `--stdin` 适合脚本生成的 JSON。
3. `--json` 只适合 `{}` 之类的小字面量，或者非常短的手写对象。

不要把生成出来的大 JSON 直接塞进 inline `--json`，PowerShell 很容易在转义层面出问题。

### 输出投影

- `--select` 和 `--fields` 都可用。
- 日常 Agent 循环只取需要的字段，通常只保留 `status`、`summary`、`preview_id`、`task_run_id`、`artifacts.full_result`。
- `--format summary|json|full` 控制 CLI 自身输出级别。
- `--develop` 只用于调试时补充 timing 信息。
- `--expert` 只在需要原始 Bridge / trace 调试结果时使用。
- `--artifact-dir` 和 `--max-bytes` 是通用输出控制参数。

### 结果形状

- stdout 的常规摘要是 `BlueprintHelper.CliResult.v1`。
- `artifacts.full_result` 是较完整的 `BlueprintHelper.CliFullResult.v1`。
- `artifacts.debug_result` 只在 `--expert` 场景返回，对应 `BlueprintHelper.CliDebugResult.v1`。
- `blueprinthelper_get_task_result` 读取完成的任务结果或 `TaskRunJournal`。

### 读取格式约定

`blueprinthelper_read_context` 使用 `ReadSpec` 的 `view.format`，常见值是:

- `logic_flow`
- `logic_md`
- `logic_json`

其中:

- `logic_flow` 适合简单 function / event / custom event。
- `logic_md` 适合更大、分支更多的入口。
- `logic_json` 用于需要稳定锚点的 patch / merge / debug。
- `logic_flow` 和 `logic_md` 不是锚点来源。
- 不要再使用 `view.format=summary` 作为 ReadSpec 格式。

## 模板导航

先从这里开始找模板:

- `AgentFaceService/agent-guide/Templates/INDEX.md`
- `AgentFaceService/agent-guide/Templates/SEMANTIC_INDEX.md`
- `AgentFaceService/agent-guide/Templates/read/SEMANTIC_INDEX.md`
- `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md`

模板目录分工如下:

| 目录 | 用途 |
|---|---|
| `Templates/` | 环境、诊断、指南、授权、结果查询、调试摘要、审查摘要等支持型输入。 |
| `Templates/read/` | ReadSpec、ReferenceContext、FunctionChainContext 相关输入。 |
| `Templates/write/` | TaskSpec 预览 / 执行包装器、裸 TaskSpec、资源创建与编辑模板。 |

原则很简单:

1. 先选对模板。
2. 复制一份到工作文件。
3. 替换占位符。
4. 再调用 CLI。

## 命令总览

### 运行态与诊断

| 命令 | 类型 | 输入 | 推荐模板 | 说明 |
|---|---|---|---|---|
| `blueprint_get_runtime_profile` | 运行态读取 | `{}` | `Templates/blueprint_get_runtime_profile_template.json` | 读取当前运行 Editor / Bridge 的 runtime profile。 |
| `blueprinthelper_diagnostics` | 本地诊断 | `{}` | `Templates/blueprinthelper_diagnostics_template.json` | 只做本地静态安装 / 配置诊断，不触发 UE Bridge。 |
| `blueprinthelper_diagnostics_runtime` | 运行态诊断 | `{}` | `Templates/blueprinthelper_diagnostics_runtime_template.json` | 通过运行中的 Editor / Bridge 做运行态诊断。 |
| `blueprinthelper_read_agent_guide` | 指南读取 | `{}` | `Templates/blueprinthelper_read_agent_guide_template.json` | 读取 AgentGuide 入口索引。 |

### 读取类

| 命令 | 类型 | 输入 | 推荐模板 | 说明 |
|---|---|---|---|---|
| `blueprinthelper_read_context` | UE 资产上下文读取 | `BlueprintHelper.ReadSpec.v1` 根对象 | `Templates/read/SEMANTIC_INDEX.md` + 对应 read 模板 | 读取 Blueprint、UMG、DataAsset、DataTable、对象属性、图上下文等。 |
| `blueprinthelper_read_context_capabilities` | ReadContext 能力矩阵 | `{}` | `Templates/read/blueprinthelper_read_context_capabilities_template.json` | 只读本地能力矩阵，不接触 UE 资产。 |
| `blueprinthelper_read_reference_context` | 引用 / 依赖上下文 | `ReferenceContextPack.v1` 请求对象 | `Templates/read/SEMANTIC_INDEX.md` + 对应 reference 模板 | 用于重命名、删除、签名变更、依赖风险分析。 |
| `blueprinthelper_read_function_chain_context` | 函数链索引 | function / event / custom_event 链路请求对象 | `Templates/read/blueprinthelper_read_function_chain_context_template.json` | 只返回紧凑索引，不返回完整函数体。 |
| `blueprinthelper_find_assets` | 资产路径发现 | `BlueprintHelper.FindAssetsRequest.v1` 请求对象 | `Templates/blueprinthelper_find_assets_template.json` | 在未知 Unreal `asset_path` 时先用 AssetRegistry 缩小候选，再继续读上下文或写流程。 |
| `blueprinthelper_capture_screenshot` | 编辑器截图证据 | `BlueprintHelper.CaptureScreenshotRequest.v1` 请求对象 | `Templates/blueprinthelper_capture_screenshot_template.json` | 打开资产、可选定位 `graph_name + block_ref/node_ref`，然后保存真实编辑器截图证据。 |

`blueprinthelper_read_reference_context` 的模板家族包括:

- `blueprinthelper_read_reference_context_safety_template.json`
- `blueprinthelper_read_reference_context_dependencies_template.json`
- `blueprinthelper_read_reference_context_external_dependents_template.json`
- `blueprinthelper_read_reference_context_function_template.json`
- `blueprinthelper_read_reference_context_member_variable_template.json`
- `blueprinthelper_read_reference_context_local_variable_template.json`
- `blueprinthelper_read_reference_context_event_dispatcher_template.json`

`blueprinthelper_read_function_chain_context` 的典型字段如下:

```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "custom_event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

`target_type` 只接受 `function`、`event`、`custom_event`。

`blueprinthelper_find_assets` 的典型字段如下:

```json
{
  "schema": "BlueprintHelper.FindAssetsRequest.v1",
  "query": "Player",
  "path_prefixes": ["/Game"],
  "asset_types": ["blueprint"],
  "asset_classes": ["/Script/Engine.Blueprint"],
  "recursive": true,
  "limit": 25,
  "include_plugin_content": false,
  "include_engine_content": false,
  "include_redirectors": false
}
```

`blueprinthelper_capture_screenshot` 的典型字段如下:

```json
{
  "schema": "BlueprintHelper.CaptureScreenshotRequest.v1",
  "asset_path": "/Game/Blueprints/BP_Player.BP_Player",
  "graph_name": "EventGraph",
  "node_ref": "nodes[0]",
  "label": "bp_player_eventgraph",
  "capture_target": "auto",
  "settle_delay_ms": 250
}
```

`block_ref` 和 `node_ref` 是 Agent-facing 的可读定位字段。只要提供 `block_ref` 或 `node_ref`，就必须同时提供 `graph_name`。这个 CLI 工具是整合工具簇，会串联 `open_asset`、可选的 `focus_blueprint_editor_target`，然后按请求类型调用截图 primitive：asset-only 请求调用 `capture_editor_screenshot` 截当前 Window；Graph / Block / Node 请求调用 `capture_focused_graph_screenshot`，只输出 Graph 区域的多张独立 PNG。结果会返回 `screenshots[]`、`screenshot_count`、`capture_scope`，并用 `screenshot` 作为第一张 PNG 的兼容别名。

P0 不支持输入 `cursor`，`FindAssets.v1` 结果也不返回 `total_count` 或 `next_cursor`。需要更精确的结果时，缩小 `query`、`path_prefixes`、`asset_types` 或 `asset_classes`。

### 写入类

| 命令 | 类型 | 输入 | 推荐模板 | 说明 |
|---|---|---|---|---|
| `blueprinthelper_preview_task` | TaskSpec 预览 | `{ "task_spec": { ... } }` 包装器 | `Templates/write/blueprinthelper_preview_task_wrapper_template.json` | 直接工具名入口使用包装器。 |
| `blueprinthelper_execute_task` | TaskSpec 执行 | `{ "task_spec": { ... } }` 包装器 | `Templates/write/blueprinthelper_execute_task_wrapper_template.json` | 只能在 preview 成功后执行。 |
| `blueprinthelper_request_write_session` | 写权限申请 | 项目作用域或资产列表作用域请求对象 | `Templates/blueprinthelper_request_write_session_project_template.json` / `Templates/blueprinthelper_request_write_session_assets_template.json` | preview 提示 write permission disabled 时再申请。 |
| `blueprinthelper_get_task_result` | 结果查询 | `{ "task_run_id": "..." }` 或 `--id` | `Templates/blueprinthelper_get_task_result_template.json` | 读取已完成任务结果或 journal。 |

`blueprinthelper_request_write_session` 的常见形状:

```json
{
  "reason": "____",
  "scope": "project",
  "ttl_seconds": 900
}
```

资产列表作用域则多一个 `asset_paths` 数组。

### 调试与审查

| 命令 | 类型 | 输入 | 推荐模板 | 说明 |
|---|---|---|---|---|
| `blueprinthelper_get_debug_case` | DebugCase 读取 | `{ "debug_case_id": "..." }` | `Templates/blueprinthelper_get_debug_case_template.json` | 读取单个摘要型 DebugCase。 |
| `blueprinthelper_list_debug_cases` | DebugCase 列表 | `{ "limit": 20 }` | `Templates/blueprinthelper_list_debug_cases_template.json` | 列出可用 DebugCase。 |
| `blueprinthelper_export_debug_bundle` | DebugBundle 导出 | `{ "debug_case_id": "..." }` | `Templates/blueprinthelper_export_debug_bundle_template.json` | 导出本地 DebugBundle manifest。 |
| `blueprinthelper_query_review_records` | ReviewRecord 查询 | `{ "asset_path": "...", "task_run_id": "...", "pending_only": true }` | `Templates/blueprinthelper_query_review_records_template.json` | 查询审查摘要记录。 |

`blueprinthelper_apply_review_action` 是专家 / 插件开发内部命令，不属于普通 Agent 工作流，也不应该放进普通模板入口。

## 群组命令

群组命令和直接工具名共享同一后端，只是输入形状不同。

| 群组命令 | 输入根对象 | 推荐模板 | 说明 |
|---|---|---|---|
| `bh task preview` | 裸 `BlueprintHelper.TaskSpec.v1` | `Templates/write/task_preview_bare_taskspec_template.json` | 不要再包 `task_spec`。 |
| `bh task execute` | 裸 `BlueprintHelper.TaskSpec.v1` | `Templates/write/task_execute_bare_taskspec_template.json` | 不要再包 `task_spec`。 |
| `bh task result` | `task_run_id` | `Templates/blueprinthelper_get_task_result_template.json` | 读取任务结果。 |
| `bh context read` | 裸 `BlueprintHelper.ReadSpec.v1` | `Templates/read/SEMANTIC_INDEX.md` | 读取资产上下文。 |
| `bh bridge ping` | 无 JSON 输入 | 无 | 只做 Bridge 连通性探测。 |
| `bh bridge call` | `--command <read_only_command>` | 无 | 只允许只读 Bridge 命令，普通 Agent 应优先使用命名工具。 |

### 预览 / 执行包装器与裸 TaskSpec

直接工具名版本:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

群组命令版本:

```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

规则:

- `blueprinthelper_preview_task` / `blueprinthelper_execute_task` 使用包装器模板。
- `task preview` / `task execute` 使用裸 TaskSpec。
- 不要把裸 TaskSpec 和包装器混用。

## 生命周期边界

Agent 负责的编辑器打开 / 关闭是全局 MCP 生命周期工具，不是普通 CLI 资产流程。

应使用:

- `mcp__blueprint_helper__blueprint_open_editor`
- `mcp__blueprint_helper__blueprint_close_editor`

不要把 `bh open_editor` / `bh close_editor` 当成普通 Agent 兼容路径。CLI lifecycle 调用会返回 `lifecycle_mcp_required`；如果 lifecycle MCP 不可用，Agent 应报告 `lifecycle_mcp_unavailable`，不能改用 CLI 启动/关闭 Editor。

## 典型工作流

### TaskSpec-first 写入循环

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_find_assets when asset_path is unknown
-> bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

### 读上下文优先级

1. 先用 `blueprinthelper_read_context_capabilities` 或 `blueprinthelper_read_agent_guide` 确认环境和入口。
2. Unreal `asset_path` 未知时，先用 `blueprinthelper_find_assets` 获取候选；已知时直接进入 `blueprinthelper_read_context`。
3. 拿到一个明确的 Unreal `asset_path` 后，再用 `blueprinthelper_read_context` 读取目标资产。
4. 需要影响分析时再上 `blueprinthelper_read_reference_context`。
5. 需要链路追踪时再上 `blueprinthelper_read_function_chain_context`。

### Unknown Asset Path Workflow
1. Unknown Unreal `asset_path` -> `bh blueprinthelper_find_assets --file <find-assets.json> --select status,artifacts.full_result`
2. Known Unreal `asset_path` -> `bh blueprinthelper_read_context --file <read-spec.json> --select status,artifacts.full_result`
3. Write request -> resolve one explicit Unreal `asset_path` before `preview_task`
4. No filesystem `.uasset` path inference
5. If multiple candidates are returned, narrow or ask for confirmation before writes

### 写权限申请规则

- 先 preview。
- 只有 preview 提示 `write_permission` 被禁用时才申请写会话。
- `scope` 只允许 `project` 或 `asset_list`。
- `asset_list` 作用域必须携带 `asset_paths`。
- `ttl_seconds` 通常使用模板默认值 `900`。

## 关键限制

- 普通 Agent 不要使用 `blueprinthelper_apply_review_action`。
- 不要把旧的普通 MCP 工具当作 CLI fallback。
- 不要把 `summary` 当成 ReadSpec 的 `view.format`。
- 不要把生成出来的大 JSON 直接塞进 inline `--json`。
- 不要在没有 preview 的情况下直接执行写入。

## 常用示例

### 读取运行态

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

### 读取 ReadContext 能力矩阵

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

### 发现未知资产路径

```powershell
bh blueprinthelper_find_assets --file .\find-assets.json --select status,artifacts.full_result
bh blueprinthelper_find_assets --help
```

### 预览 TaskSpec

```powershell
bh blueprinthelper_preview_task --file .\preview_wrapper.json --select status,preview_id,summary,artifacts.full_result
```

### 执行 TaskSpec

```powershell
bh task execute --file .\task_spec.json --format summary
```

### 申请写权限

```powershell
bh blueprinthelper_request_write_session --file .\write_session.json --select status,summary
```

### 读取任务结果

```powershell
bh blueprinthelper_get_task_result --id task_123 --select status,artifacts.full_result
```

## 维护约定

这份文档必须跟着 `AgentFaceService/cli/src/cli/help.ts`、`AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts` 和 `AgentFaceService/agent-guide/Templates/` 的实际内容更新。

如果 CLI 新增了命令、模板或输入形状，先更新实现，再更新这份参考文档。
