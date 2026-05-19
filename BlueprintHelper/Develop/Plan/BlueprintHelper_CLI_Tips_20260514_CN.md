# BlueprintHelper CLI Tips

更新时间：2026-05-17

本文只记录本地 CLI、PowerShell、JSON 文件、命令入口选择等非插件代码问题。插件内部失败必须通过 CLI/Bridge 返回可诊断的 artifact；不要把插件缺少错误细节的问题转成 Agent 猜测或手工查 UE 面板。

## 0. 总原则

1. 普通资产读写走 BlueprintHelper CLI 和 TaskSpec-first 流程。
2. Agent 主动管理 Unreal Editor 生命周期时，优先使用全局 MCP lifecycle 工具；CLI 的 `open_editor`/`close_editor` 只作为兼容或人工 fallback。
3. 复杂参数先从 `AgentFaceService/agent-guide/Templates` 复制模板，再改字段，并用 `--file` 传入 UTF-8 no BOM JSON。
4. 排查阶段先用 `--format full`；确认无误后再用 `--select` 或 `--fields` 缩短 stdout。
5. 遇到 `cli_error`、`preview_blocked`、`execute_failed`、`compile_failed`，先打开 `artifacts.full_result` 指向的 JSON。
6. 如果 artifact 里只有泛化错误，例如 `Blueprint compile failed with N error(s)`，这是 CLI/Bridge 诊断能力缺口，应修复返回结构，不要求 Agent 猜测。

稳定排查命令：

```powershell
bh.cmd <tool_name> --file .\params.json --select status,summary,artifacts.full_result
bh.cmd <tool_name> --file .\params.json --format full
```

## 1. 命令入口

### 1.1 无业务参数也要传输入源

CLI 需要明确参数输入源。无参数工具使用空 JSON：

```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --format full
```

不要只写：

```powershell
bh.cmd blueprint_get_runtime_profile --format full
```

否则可能返回：

```text
Choose exactly one params input source: --file, --json, or --stdin.
```

### 1.2 直接工具名和 grouped TaskSpec 命令的根对象不同

直接工具名 `blueprinthelper_preview_task` / `blueprinthelper_execute_task` 使用 wrapper：

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped 命令 `task preview` / `task execute` 使用裸 `BlueprintHelper.TaskSpec.v1`：

```powershell
bh.cmd task preview --file .\task.json --format full
bh.cmd task execute --file .\task.json --format full
```

`task.json` 根对象示例：

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "create_asset",
  "target": {
    "asset_path": "/Game/BlueprintHelper/Examples/BP_Example",
    "target_type": "asset"
  },
  "behavior": {},
  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

### 1.3 只调用 Agent-facing CLI 命令

不要把 Bridge 内部函数名当 CLI 命令。普通 Agent 不直接调用：

```text
blueprint_get_asset_info
blueprint_get_datatable_rows
blueprint_get_object_properties
execute_task
blueprinthelper_apply_review_action
```

常用替代入口：

```text
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_query_review_records
blueprinthelper_get_task_result
task preview
task execute
```

`blueprinthelper_apply_review_action` 只用于插件开发或内部验证，不暴露给普通 Agent 工作流。

### 1.4 验证源码改动时使用工作区 CLI

如果正在验证当前仓库的 TypeScript/CLI 改动，先构建并直接运行工作区入口，避免命中全局旧版本：

```powershell
npm.cmd --prefix AgentFaceService\cli run build
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --format full
```

### 1.5 Editor lifecycle

Agent 主动开关编辑器时，使用全局 MCP lifecycle 工具。普通资产读写、TaskSpec preview/execute、diagnostics、debug bundle 查询仍走 CLI。

CLI lifecycle alias 只作为兼容或人工 fallback，不作为 AgentGuide 普通工具选择。

## 2. PowerShell 稳定写法

### 2.1 固定使用 `bh.cmd`

PowerShell 里直接运行 `bh` 可能命中 npm shim `bh.ps1`，并被 ExecutionPolicy 拦截。自动化脚本固定使用：

```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --select status,summary
```

不要在任务中临时修改用户机器的 ExecutionPolicy。

### 2.2 here-string 正文必须从下一行开始

错误写法：

```powershell
@'{"target":{"asset_path":"/Game/BP"}}'@
```

稳定写法：

```powershell
@'
{"target":{"asset_path":"/Game/BP"}}
'@ | Set-Content -LiteralPath params.json -Encoding utf8
```

需要 no BOM 时使用 `.NET` 写文件，见下一条。

### 2.3 复杂 JSON 使用 `--file`

PowerShell 对引号、反斜杠和 JSON 大括号的转义容易把参数拆坏。复杂 JSON 不走 `--json`，先写 UTF-8 no BOM 文件：

```powershell
$path = Join-Path (Get-Location) 'params.json'
$json = @{
  limit = 5
} | ConvertTo-Json -Depth 16
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
bh.cmd blueprinthelper_list_debug_cases --file $path --format full
```

### 2.4 Windows PowerShell 5.1 不支持 `utf8NoBOM`

`Set-Content -Encoding utf8NoBOM` 在 Windows PowerShell 5.1 可能报无法转换枚举值；`Set-Content -Encoding utf8` 又可能写入 UTF-8 BOM。CLI JSON 文件统一使用：

```powershell
$json = $object | ConvertTo-Json -Depth 40
$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'task.json'), $json, $encoding)
```

### 2.5 不要用 `$profile` 做普通变量名

`$PROFILE` 是 PowerShell 预定义变量，大小写不敏感。处理 `agent-profile.json` 等配置时，使用 `$agentProfile`、`$runtimeProfile` 这类变量名。

### 2.6 不依赖 `ConvertFrom-Json -AsHashtable`

Windows PowerShell 5.1 不支持 `ConvertFrom-Json -AsHashtable`。跨环境脚本可以用 `PSCustomObject` 属性访问，或用 Node.js 处理 JSON。

### 2.7 外部命令参数不要内联 `(Join-Path ...)`

外部命令不一定按 PowerShell 表达式预期接收参数。先赋值再传：

```powershell
$file = Join-Path $base 'read_object_bp.json'
bh.cmd blueprinthelper_read_context --file $file --format full
```

### 2.8 `WriteAllText` 与 `-replace` 分开写

错误风险写法：

```powershell
[System.IO.File]::WriteAllText($path, $text -replace 'a','b', $encoding)
```

稳定写法：

```powershell
$newText = $text -replace 'a', 'b'
[System.IO.File]::WriteAllText($path, $newText, $encoding)
```

### 2.9 长文本或 Markdown 修改使用 patch

长 here-string 可能触发 Windows 命令行长度限制，例如 `CreateProcessAsUserW failed: 206`。修改文档或源码时优先使用 `apply_patch`，或把复杂输入落到文件后再调用 CLI。

### 2.10 复杂 `rg` pattern 使用单引号

PowerShell 中混用双引号、管道符和反斜杠时，`rg` pattern 很容易被 shell 拆坏。优先写：

```powershell
rg -n 'Make.*StringArray|FJsonValueString|SetArrayField' 'BlueprintHelper\Source\BlueprintHelper\Private'
```

### 2.11 当前一次性脚本避免 `??`

当前 PowerShell 环境里，一次性脚本不要使用 null coalescing `??`。使用显式 `if` 或提前计算变量，避免解析错误。

## 3. 输出与 artifact

`--select` 和 `--fields` 会裁剪 stdout，也可能隐藏错误字段。排查 CLI 参数、JSON、schema 或 UE compile 问题时，先用：

```powershell
bh.cmd <tool_name> --file .\params.json --format full
```

如果 stdout 太短，打开 `artifacts.full_result`。不要把“stdout 被裁剪”和“artifact 缺少诊断”混为同一个问题。

UE compile 失败时，artifact 应包含可读的 compiler results，例如 `data.compile_result.compiler_results[]` 或等价 markdown 摘要。

## 4. 常见字段错误

### 4.1 `read_context` 使用 ReadSpec 根对象

`blueprinthelper_read_context` 参数根对象就是 `BlueprintHelper.ReadSpec.v1` 字段，不要再包 `args`。

稳定写法：

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "object_property_context",
  "target": {
    "asset_path": "/Game/Asset",
    "target_type": "object_property",
    "property_path": "SomeProperty"
  },
  "view": {
    "format": "summary"
  }
}
```

不要把 `format` 放到顶层；使用 `view.format`。

### 4.2 ReadSpec 常用组合

| 需求 | `read_type` | `target.target_type` |
| --- | --- | --- |
| 资产摘要 | `asset_context` | `asset` |
| 蓝图逻辑摘要 | `blueprint_logic` | `blueprint` |
| 图表上下文 | `graph_context` | `graph` |
| 图表逻辑 JSON | `blueprint_logic` | `graph` |
| 函数逻辑 | `blueprint_logic` | `function` |
| Event 逻辑 | `blueprint_logic` | `event` |
| CustomEvent 逻辑 | `blueprint_logic` | `custom_event` |
| 组件 | `component_context` | `blueprint` |
| 变量 | `variable_context` | `member_variable` |
| Event Dispatcher | `variable_context` | `event_dispatcher` |
| Widget tree | `widget_context` | `blueprint` |
| Widget property | `widget_context` | `widget` |
| DataTable | `data_table_context` | `data_table` |
| DataTable row | `data_table_context` | `data_table_row` |
| DataAsset | `data_asset_context` | `data_asset` |
| UObject 属性 | `object_property_context` | `object_property` 或 `property` |
| BlueprintHelper-owned block | `blueprint_logic` | `block` |

### 4.3 TaskSpec 字段是 `validation`

不要写旧字段：

```json
{
  "validation_policy": {
    "should_compile": true,
    "should_save": false
  }
}
```

稳定写法：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

DataAsset、DataTable、普通 UObject 属性写入通常 `should_compile=false`。Blueprint class、WidgetBlueprint、蓝图图表和签名修改按需要 `should_compile=true`。

### 4.4 跨编辑器重启或持久化验证需要 `should_save=true`

`validation.should_save=false` 只保证当前编辑器内存态。后续流程会重启编辑器、跨进程读取、或要求磁盘持久化时，TaskSpec 应设置：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 4.5 `review_baseline_dirty_asset_policy` 合法值

只使用：

```text
block
save_before_archive
allow_stale_disk_snapshot
```

不要写 `allow`、`allow_auto_save` 等旧值。

### 4.6 `blueprinthelper_export_debug_bundle` 只接收 `debug_case_id`

错误方向：

```json
{
  "asset_path": "/Game/Asset",
  "reason": "manual"
}
```

稳定写法：

```json
{
  "debug_case_id": "<debug_case_id>"
}
```

先通过 `blueprinthelper_list_debug_cases` 或 `blueprinthelper_get_debug_case` 找到 `debug_case_id`。

### 4.7 Graph statement 调用函数使用 `kind=call`

不要写旧值 `call_function`。稳定写法：

```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "hello"
    }
  }
}
```

### 4.8 `scope_policy.graph_name` 不要等于 CustomEvent 名

`append_new_owned_graph` 中，`scope_policy.graph_name` 不应与 `entries[].name` 的 Custom Event 名相同，否则 execute 编译阶段可能因为 UE 生成同名图表/函数失败。

### 4.9 组件重名策略按当前模板字段写

不要使用旧模板值 `reuse_existing`。组件 TaskSpec 模板使用 `on_name_conflict=reuse_if_exists`；编译后的 TaskPlan 内部字段可表现为 `name_collision_policy=reuse_if_exists`。

## 5. Preview/Execute 工作流

1. 写入前必须 preview。
2. `preview_blocked` 是阻断状态，不要只检查 `blocked` 字段。
3. `asset_already_exists` 是 preview 阻断，不是崩溃；应改资产名，或切换到 edit/update 类 TaskSpec。
4. DataTable add row 遇到同名 row 已存在时，使用唯一 row name，或改用 update row 流程。
5. 排查 preview artifact 时不要并行跑多个读取同类 artifact 的 preview；按顺序执行，避免读取混到相近输出。
6. Preview blocked 时，修 TaskSpec 或停止报告，不要绕到底层 capability 工具。

## 6. 模板优先流程

模板目录：

```text
AgentFaceService/agent-guide/Templates
AgentFaceService/agent-guide/Templates/read
AgentFaceService/agent-guide/Templates/write
```

推荐流程：

```powershell
Copy-Item -LiteralPath AgentFaceService\agent-guide\Templates\write\task_preview_bare_taskspec_template.json -Destination .\task.json
# 修改 task.json 中的占位字段
bh.cmd task preview --file .\task.json --format full
```

根目录模板用于 runtime/profile/diagnostics/debug/write-session 等非逻辑上下文读取；`read` 目录用于获取资产逻辑上下文；`write` 目录用于 TaskSpec-first preview/execute 和写入场景。

## 7. 本地源码和文档操作

1. 修改 Markdown、JSON 模板或源码时，优先用 patch，避免 PowerShell 长 here-string 和编码问题。
2. 复制 UE 源码快照前，先用 `rg` 确认真实 Public/Private 路径。
3. 不要把 UE Engine `.cpp` 快照误复制进插件 `Source` 目录，除非任务明确要求。

示例：

```powershell
rg -n 'SReadOnlyHierarchyView' 'E:\UE_5.6\Engine\Source\Editor' -g '*.h' -g '*.cpp'
```

## 8. 快速索引

| 现象 | 稳定处理 |
| --- | --- |
| `Choose exactly one params input source` | 加 `--json "{}"`、`--file` 或 `--stdin` |
| `bh.ps1` 被 ExecutionPolicy 拦截 | 使用 `bh.cmd` |
| JSON parse 出现 BOM 或乱码 | 用 `[System.Text.UTF8Encoding]::new($false)` 写文件 |
| PowerShell 把 `--json` 拆成命令名一部分 | 改用 UTF-8 no BOM `--file` |
| `task preview` 报根字段缺失 | grouped 命令传裸 TaskSpec，不传 `{ "task_spec": ... }` |
| 直接工具名 preview/execute 报缺少 `task_spec` | 直接工具名传 `{ "task_spec": { ... } }` |
| `unsupported command` | 检查是否调用了 Bridge 内部名或旧 CLI 名 |
| `validation_policy` 无效 | 改为 `validation` |
| `allow_auto_save` 无效 | 改为 `block`、`save_before_archive` 或 `allow_stale_disk_snapshot` |
| Debug bundle 参数无效 | 只传 `debug_case_id` |
| `--fields` 看不到错误细节 | 用 `--format full` 并打开 `artifacts.full_result` |
| CLI `npm run build` 把 `../task-core` 解析到 Codex sandbox cwd | 先在 `AgentFaceService/task-core` 跑 `npm.cmd run build`，再在 `AgentFaceService/cli` 依次跑 `node ..\scripts\clean-build.mjs` 和 `node ..\scripts\run-tsc.mjs` |

## UBT RulesError：Saved/BuildPlugin_* 打包产物导致 Build.cs 重名

现象：执行项目编译时，UBT 在规则扫描阶段报告 `The namespace '<global namespace>' already contains a definition for 'BlueprintHelper'`，路径指向 `Plugins/BlueprintHelper/Saved/BuildPlugin_UE53/Source/BlueprintHelper/BlueprintHelper.Build.cs` 或 `Saved/BuildPlugin_UE56/Source/BlueprintHelper/BlueprintHelper.Build.cs`。

原因：`Saved/BuildPlugin_*` 是打包插件输出目录，但其中仍包含同名模块规则文件。项目编译时 UBT 会扫描到这些 `.Build.cs`，与真实插件源码下的 `BlueprintHelper.Build.cs` 发生重名冲突。

处理：编译前清理或隔离 `Plugins/BlueprintHelper/Saved/BuildPlugin_*` 打包输出目录，再重新执行项目编译。该错误不是业务 C++ 源码编译错误。
## 2026-05-18 MCP open_editor agent-profile BOM
- 现象：MCP open_editor 返回 PROJECT_AGENT_PROFILE_INVALID_JSON，错误包含 Unexpected token '﻿'。
- 原因：项目 .blueprinthelper/agent-profile.json 带 UTF-8 BOM，MCP JSON parser 不接受 BOM。
- 处理：将该文件重写为 UTF-8 no BOM 后重试 open_editor。

## 2026-05-18 install 脚本覆盖 AutoRepair profile
- 现象：preview 通过但 execute 返回 Bridge write failed，runtime diagnostics 显示 write_permission.disabled / write_session_missing。
- 原因：测试 install 脚本后项目 .blueprinthelper/agent-profile.json 的 active_profile.safety_profile 被覆盖为 Conservative。
- 处理：将 safety_profile 切回 AutoRepair，并保持 safety.write_approval_required=false、approval_bypass=true；必要时重启 Editor 让 Bridge 重新加载配置。


## PowerShell ExecutionPolicy: npm.ps1 被拦截
- 现象：在 PowerShell 里运行 
pm run build 可能报 无法加载文件 ... npm.ps1，因为在此系统上禁止运行脚本。
- 原因：PowerShell 优先解析 
pm.ps1，受 ExecutionPolicy 限制。
- 稳定做法：在 Codex/PowerShell 自动化中使用 
pm.cmd run build。

## 2026-05-19 PowerShell rg regex quoting
- Symptom: `rg` with a complex regex containing quotes/backslashes reports PowerShell parser errors such as `TerminatorExpectedAtEndOfString`, or ripgrep receives the path as part of the regex.
- Cause: PowerShell quote parsing and regex escaping can interact badly when the pattern includes both `TEXT(\"...\")` style fragments and a Windows path argument.
- Stable workaround: use `rg -F` fixed-string searches for C++ snippets, split complex alternation into multiple `rg -F` commands, or put the regex in a variable before invoking `rg`.
