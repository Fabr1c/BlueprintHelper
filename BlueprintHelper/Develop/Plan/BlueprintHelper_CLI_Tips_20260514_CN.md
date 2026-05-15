# BlueprintHelper CLI Tips

日期：2026-05-14

用途：记录 BlueprintHelper CLI 调用过程中真实遇到的问题、原因和稳定写法，供后续测试与 Agent 执行参考。

## 1. 无参 CLI 命令也需要显式参数输入源

错误命令：

```powershell
bh.cmd blueprint_get_runtime_profile --format full
```

现象：

```text
Choose exactly one params input source: --file, --json, or --stdin.
```

稳定写法：

```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --format full
```

原因：

CLI 当前统一要求每次调用恰好提供一个参数输入源，即使该工具没有业务参数，也需要使用 `--json "{}"`、`--file empty.json` 或 `--stdin`。

## 2. PowerShell here-string 标题行后不能直接跟 JSON 内容

错误写法：

```powershell
@'{"target":{"asset_path":"/Game/BP"}}'@
```

现象：

```text
here-string 标题后面和行尾之前不允许包含任何字符。
```

稳定写法：

```powershell
@'
{"target":{"asset_path":"/Game/BP"}}
'@ | Set-Content -LiteralPath params.json -Encoding utf8
```

更推荐的 no BOM 写法：

```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, '{"target":{"asset_path":"/Game/BP"}}', $enc)
```

## 3. 旧版 Windows PowerShell 不支持 `ConvertFrom-Json -AsHashtable`

错误写法：

```powershell
$profile = $json | ConvertFrom-Json -AsHashtable
```

现象：

```text
找不到与参数名称“AsHashtable”匹配的参数。
```

稳定策略：

1. 需要保留未知字段并写 JSON 时，优先用 Node.js 读写。
2. 如果必须用 Windows PowerShell 5.1，避免依赖 `-AsHashtable`，改用 `PSCustomObject` 或手写对象。

推荐 Node.js 写法：

```powershell
@'
const fs = require('fs');
const path = 'D:/UEProjects/Template/.blueprinthelper/agent-profile.json';
const profile = JSON.parse(fs.readFileSync(path, 'utf8'));
profile.active_profile = profile.active_profile || {};
profile.active_profile.safety_profile = 'AutoRepair';
fs.writeFileSync(path, JSON.stringify(profile, null, 2) + '\n', 'utf8');
'@ | node -
```

## 4. 不要在 PowerShell 中使用 `$profile` 作为普通变量名

问题：

PowerShell 变量名大小写不敏感，`$profile` 会碰到内置 `$PROFILE` 自动变量语义，容易把用户 PowerShell profile 路径对象误写进 JSON。

本轮现象：

`agent-profile.json` 曾被污染为 PowerShell profile 路径对象，后续已用 Node.js 恢复。

稳定写法：

```powershell
$agentProfile = @{}
$agentProfile['schema'] = 'BlueprintHelper.AgentProfile.v1'
```

## 5. `--select` 会裁切结果，排查错误时应改用 `--format full`

问题：

`--select status,summary,artifacts.full_result` 适合正常查看阶段结果，但可能隐藏 CLI 解析错误和 Bridge 错误细节。

排查写法：

```powershell
bh.cmd <tool_name> --json "{}" --format full
```

## 6. CLI 参数文件建议使用 UTF-8 no BOM

推荐写法：

```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, $jsonText, $enc)
bh.cmd <tool_name> --file $p --format full
```

原因：

复杂 JSON、中文字段、以及后续跨 Node/PowerShell/UE Bridge 解析时，UTF-8 no BOM 更稳定，能减少编码和非法 JSON 风险。

## 7. 避免用 PowerShell here-string 直接管道复杂中文 Markdown 到 Node

问题：

PowerShell 命令字符串中同时包含中文、Markdown 反引号和 JavaScript template string 时，可能在传给 Node 后出现乱码或脚本解析错误。

本轮现象：

```text
SyntaxError: Unexpected token '??'
```

稳定策略：

1. 修改 Markdown 文档时优先使用 `apply_patch`。
2. 如果必须用 Node 脚本写文档，尽量从外部文件读取正文，避免在命令行内嵌复杂中文 Markdown。
3. Markdown 正文里有反引号时，不要直接塞进 JavaScript template string；改用数组行拼接或 JSON 字符串文件。

## 8. PowerShell 中复杂 `--json` 容易被错误转义成命令名

错误写法：

```powershell
bh.cmd blueprint_get_asset_info --json "{\"asset_path\":\"/Game/BP\"}" --format full
```

本轮现象：

```text
Unsupported BlueprintHelper CLI command: blueprint_get_asset_info --json ...
```

稳定策略：

1. PowerShell 下复杂 JSON 优先写入 UTF-8 no BOM 文件，再用 `--file`。
2. 只在非常简单且已验证的场景使用 `--json "{}"`。

推荐写法：

```powershell
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($p, '{"asset_path":"/Game/BP"}', $enc)
bh.cmd blueprint_get_asset_info --file $p --format full
```

## 9. 不要假设所有 Bridge 命令都有直接 CLI 暴露

本轮现象：

```text
Unsupported BlueprintHelper CLI command: blueprint_get_asset_info --file ...
```

原因：

`get_asset_info` 是 Bridge 内部命令，但当前 CLI 对外稳定入口不一定暴露对应的 `blueprint_get_asset_info` 直接命令。

稳定策略：

1. 普通 Agent 获取资产上下文应使用 `blueprinthelper_read_task_context`。
2. 只有 `tool-registry` 明确列出的 CLI 命令才作为 Agent 可用 surface。
3. 如果必须测试 Bridge 内部命令，使用专门 Debug/Bridge 脚本，不要把它当作普通 CLI surface。

## 10. Windows PowerShell 的 `Set-Content -Encoding utf8` 可能写入 BOM

错误写法：
```powershell
$taskSpec | ConvertTo-Json -Depth 40 | Set-Content -Path task.json -Encoding utf8
bh.cmd blueprinthelper_preview_task --file .\task.json --format full
```

本轮现象：
```text
Unexpected token '﻿', "﻿{ ... is not valid JSON
```

原因：
Windows PowerShell 5.1 的 `-Encoding utf8` 默认会写 UTF-8 BOM，当前 CLI JSON parser 不接受文件开头的 BOM。

稳定写法：
```powershell
$json = $taskSpec | ConvertTo-Json -Depth 40
$enc = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Resolve-Path '.\task.json'), $json, $enc)
```

## 11. `read_context` 的 `target_type` 不接受 `data_table`

错误写法：

```powershell
bh.cmd blueprinthelper_read_context --file read_datatable.json --format full
```

其中参数包含：

```json
{
  "read_type": "data_table_context",
  "target": {
    "asset_path": "/Game/DT_Test",
    "target_type": "data_table"
  }
}
```

现象：

```text
Invalid enum value ... received 'data_table'
```

稳定策略：

1. 当前 `ReadSpec.target.target_type` 枚举没有 `data_table`，不要填写该值。
2. DataTable 资产级目标先用 `target_type=asset`；具体行目标才使用 `target_type=data_table_row`。
3. 即使修正 target_type，当前 `blueprinthelper_read_context` 仍主要支持 `blueprint_logic`，DataTable/ObjectProperty 统一 read context 属于未完成能力，不要把它当作已可用读回入口。

## 12. 当前 CLI 未注册直接 DataTable/ObjectProperty 读命令

错误写法：

```powershell
bh.cmd blueprint_get_datatable_rows --file params.json --format full
bh.cmd blueprint_get_object_properties --file params.json --format full
```

现象：

```text
Unsupported BlueprintHelper CLI command: blueprint_get_datatable_rows --file ...
Unsupported BlueprintHelper CLI command: blueprint_get_object_properties --file ...
```

稳定策略：

1. 这些 Bridge 内部命令当前不属于 Agent-facing CLI registry，不能作为普通 CLI smoke 的读回入口。
2. 自动验证优先使用 `blueprinthelper_preview_task` / `blueprinthelper_execute_task` artifact 中的 runtime result、task_run_id、applied_count、changed_count 等证据。
3. 如果未来需要稳定读回，应补统一 `read_context` 能力或正式注册读命令，而不是在测试脚本中假设内部 Bridge 命令已暴露。

## 11. 开发验证时不要默认使用全局 `bh.cmd`

本轮现象：
工作区 TypeScript 编译器已经输出 `logic_spec`，但直接调用全局 `bh.cmd` 仍可能复现旧行为，原因是全局命令可能指向已安装插件缓存版本，而不是当前源码工作区。

稳定策略：
1. 验证未发布的工作区源码改动时，先构建工作区 CLI。
2. 使用工作区 CLI 入口执行测试，而不是全局 `bh.cmd`。

推荐写法：
```powershell
npm.cmd --prefix AgentFaceService\cli run build
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_preview_task --file .\task.json --format full
```
## 12. TaskSpec 编译/保存策略字段是 `validation`，不是 `validation_policy`

错误写法：
```json
{
  "validation_policy": {
    "should_compile": true,
    "should_save": false
  }
}
```

本轮现象：
TaskSpec 写入成功，但执行结果顶部显示 `validation.should_compile=false`，没有执行 post compile。后来改成 `validation.should_compile=true` 后，结果中出现 `post_operations.compile_blueprint_asset`。

稳定写法：
```json
{
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

原因：
当前 AgentFace TaskSpec schema 使用 `validation` 字段；`validation_policy` 会作为未知字段 passthrough，但不会参与 TaskPlan execution policy。
## 13. PowerShell 调用 .NET WriteAllText 时不要把 -replace 表达式直接塞进参数列表

现象：`[System.IO.File]::WriteAllText($path, $text -replace 'a','b', $encoding)` 会被 PowerShell 解析成 4 个参数，报“找不到 WriteAllText 的重载，参数计数为 4”。

建议：先用中间变量保存替换结果，再调用三参数重载：`$new = $text -replace 'a','b'; [System.IO.File]::WriteAllText($path, $new, $encoding)`。

## 14. 需要跨编辑器重启验证的资产必须设置 `validation.should_save=true`

现象：TaskSpec 执行成功后关闭并重新启动编辑器，再执行后续 TaskSpec 时出现 `target_blueprint_not_found`。

本轮原因：测试资产使用了 `validation.should_save=false`，资产只存在于当前编辑器内存/未保存包状态中；重启编辑器后后续 TaskSpec 找不到目标 Blueprint。

稳定写法：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

建议：

1. 单轮临时写入、同一编辑器会话内马上验证，可以使用 `should_save=false`。
2. 任何需要关闭/重启编辑器、复测持久化、或交给下一轮 TaskSpec 继续写入的资产，都应使用 `should_save=true`。

## 15. Windows PowerShell 5.1 不支持 `Set-Content -Encoding utf8NoBOM`

现象：
```text
Set-Content : 无法绑定参数“Encoding”。无法将值“utf8NoBOM”转换为类型“Microsoft.PowerShell.Commands.FileSystemCmdletProviderEncoding”。
```

原因：
Windows PowerShell 5.1 的 `Set-Content -Encoding` 枚举没有 `utf8NoBOM`，该写法只适用于较新的 PowerShell 版本。

稳定写法：
```powershell
$json = $object | ConvertTo-Json -Depth 40
$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'task.json'), $json, $encoding)
```

建议：
1. BlueprintHelper CLI 的复杂 JSON 仍优先走 `--file`。
2. 在 Windows PowerShell 5.1 下写 `--file` JSON 时使用 .NET `UTF8Encoding($false)`，不要使用 `utf8NoBOM` 枚举名。

## 16. `blueprinthelper_export_debug_bundle` 只接受 `debug_case_id`

错误写法：
```powershell
bh blueprinthelper_export_debug_bundle --json '{"asset_path":"/Game/Asset","reason":"manual"}'
```

现象：
```json
{"status":"cli_error"}
```

原因：
AgentFace schema 使用 `DebugCaseInputSchema`，该命令只接受 `debug_case_id: string`。`asset_path` 和 `reason` 不是合法字段。

正确写法：
```powershell
bh blueprinthelper_export_debug_bundle --json '{"debug_case_id":"<debug_case_id>"}'
# 或写入 UTF-8 no BOM 文件后：
bh blueprinthelper_export_debug_bundle --file .\debug_case.json
```

建议：
1. 先用 `blueprinthelper_list_debug_cases` 或 `blueprinthelper_get_debug_case` 获取有效 `debug_case_id`。
2. 如果只有 asset path，先触发/查询能产生 DebugCase 的流程，不要直接调用 export。

## 17. Windows PowerShell 下复杂 `--json` 仍可能污染 CLI command name

现象：
```text
Unsupported BlueprintHelper CLI command: blueprinthelper_list_debug_cases --json {\ limit\:5} --format full
```

原因：
Windows PowerShell 的引号和反斜杠规则可能让复杂 JSON 没有作为单独参数传入 CLI，导致 CLI 把后续参数拼进 command name。

稳定写法：
```powershell
$json = @'
{
  "limit": 5
}
'@
[System.IO.File]::WriteAllText((Join-Path (Get-Location) 'list_debug_cases_limit5.json'), $json, [System.Text.UTF8Encoding]::new($false))
bh blueprinthelper_list_debug_cases --file .\list_debug_cases_limit5.json --format full
```

建议：
复杂 JSON、带引号 JSON、或需要跨 shell 稳定复现的命令统一使用 UTF-8 no BOM `--file`。

## 15. UE 源码复制快照前先确认 Public / Private 实际路径

现象：A5 原生面板源码快照时，最初假设 `SReadOnlyHierarchyView.h` 位于 `UMGEditor/Private/Hierarchy`，PowerShell `Copy-Item` 前置校验返回 missing source file。

原因：UE 5.6 中 `SReadOnlyHierarchyView.cpp` 位于 `UMGEditor/Private/Hierarchy`，但 `SReadOnlyHierarchyView.h` 位于 `UMGEditor/Public/Hierarchy`。

稳定做法：复制 UE 原生源码前先用 `rg -n "ClassOrFileName" E:\UE_5.6\Engine\Source\Editor -g "*.h" -g "*.cpp"` 确认真实路径，再执行 `Copy-Item -LiteralPath <source> -Destination <snapshot>`。不要直接把未适配 `.cpp` 复制到插件 `Source` 编译目录。
## 2026-05-15 PowerShell rg 正则引号规避

现象：在 PowerShell 中直接写包含 `"`、`.`、`|`、括号的复杂 `rg` 正则时，PowerShell 可能先按自身转义/成员访问规则解析，报 `引用运算符后面缺少属性名称`，命令还没进入 `rg`。

稳定做法：PowerShell 里优先用单引号包住复杂 `rg` pattern，例如：

```powershell
rg -n 'Make.*StringArray|FJsonValueString|SetArrayField' 'D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\BlueprintHelperTaskRuntimeService.cpp'
```

分类：本地 shell 调用错误，不是 BlueprintHelper 插件 Bug。
## 2026-05-15 PowerShell ExecutionPolicy 拦截 bh.ps1

现象：在 PowerShell 中执行 `bh ...` 时，命令解析到 npm shim `bh.ps1`，如果当前系统执行策略禁止脚本，会报 `无法加载文件 ...\bh.ps1，因为在此系统上禁止运行脚本`。

稳定做法：本地不调整 ExecutionPolicy 时，直接调用 `bh.cmd ...`，例如：

```powershell
bh.cmd blueprint_get_runtime_profile --json "{}" --select status,summary
```

分类：PowerShell/npm shim 调用限制，不是 BlueprintHelper 插件 Bug。
## 2026-05-15 PowerShell 文档脚本内嵌 --json 大括号规避

现象：在 PowerShell 脚本的普通双引号字符串中直接写 ``--json "{}"``，`{}` 和转义引号可能被 PowerShell 解析为表达式片段，报“表达式或语句中包含意外的标记 `{`”。

稳定做法：文档替换/追加脚本中包含 JSON 示例时，使用 here-string 保存整段文本，或把命令示例拆成单引号字符串，不要在普通双引号字符串里嵌套 `"{}"`。

分类：PowerShell 脚本文本转义错误，不是 BlueprintHelper 插件 Bug。
## 2026-05-15 `task preview/execute` 需要裸 TaskSpec

现象：把旧文档里的 `{ "task_spec": { ... } }` 直接传给 `bh.cmd task preview --file ...`，会触发 Zod union 校验错误，提示根路径缺少 `schema/task_type/target/behavior`。

原因：当前 CLI 的 `task preview` / `task execute` 入参是裸 `BlueprintHelper.TaskSpec.v1`，不是带 `task_spec` 包装的对象。旧 runbook 中的包装示例不能直接作为 CLI 文件传入。

稳定做法：文件根对象直接写：

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "create_asset",
  "target": { "asset_path": "/Game/...", "target_type": "asset" },
  "behavior": {}
}
```

## 2026-05-15 PowerShell 写 TaskSpec 文件需要 UTF-8 no BOM

现象：使用 `Set-Content -Encoding utf8` 写出的 TaskSpec 在部分 PowerShell 环境下会带 UTF-8 BOM，CLI 读取 `--file` 时可能报 `Unexpected token '﻿' ... is not valid JSON`。

稳定写法：使用 .NET no BOM 编码写入文件。

```powershell
$json = $object | ConvertTo-Json -Depth 32
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
```

## 2026-05-15 本地 build CLI 的 TaskSpec 命令入口

现象：直接运行 `node ...\AgentFaceService\cli\build\cli\index.js blueprinthelper_execute_task_spec --file ...` 会返回 `Unsupported BlueprintHelper CLI command`。

原因：本地 build CLI 的 TaskSpec 入口是分组命令，不是旧工具名命令。

稳定做法：

```powershell
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task execute --file D:\Path\task.json --format full --fields status,summary,artifacts.full_result
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task preview --file D:\Path\task.json --format full
```

说明：Bridge 工具类命令仍可用 `<tool_name> --file params.json`，例如 `blueprinthelper_query_review_records`。

## 2026-05-15 PowerShell 场景避免复杂 `--json`

现象：PowerShell 中传入 `--json '{"..."}'` 容易因为引号/转义导致 CLI 解析错误或 `cli_error`。

稳定做法：不扩大 `--json` 转义容忍，复杂参数统一写入 UTF-8 no BOM JSON 文件，再使用 `--file`。

```powershell
$path = 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexTaskSpecs\params.json'
$json = @{ task_run_id='task_xxx'; pending_only=$true } | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText($path, $json, [System.Text.UTF8Encoding]::new($false))
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js blueprinthelper_query_review_records --file $path --format full
```
## 2026-05-15 Graph TaskSpec 语句使用短名

现象：`append_new_owned_graph` 中继续使用旧 `kind="call_function"` 会在 preview 阶段返回 `statement_kind_unsupported`。

稳定做法：GraphStatementFramework 主链路使用短名，例如 `kind="call"` + `target="PrintString"`。

```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": { "kind": "literal", "value_type": "string", "value": "hello" }
  }
}
```