# Read Templates / 读取模板

## 中文

这些模板用于 UE asset、Blueprint logic、dependency 和 task-context 读取。它们不用于环境诊断或写入授权。

不要扫描本目录来按 Agent 意图选择模板。先使用 CLI catalog：

```powershell
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

然后只读取 `bh tools templates <tool_id>` 返回的具体模板路径。

典型命令：

```powershell
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file .\read.json --fields status,summary,artifacts.full_result
```

简单 function/event/custom event 读取先用 `logic_flow`。入口较大或分支较多时使用 `logic_md`。图表大小未知、读取全图、需要稳定 `block_id`、`node_ref`、`pin_ref` 或 `link_ref` anchor 以编写 patch/merge TaskSpec 时，使用 `logic_json`。ReadSpec 不再支持 `view.format=summary`；非 logic 模板直接省略 `view.format`。

## English

Use these templates for UE asset, Blueprint logic, dependency, and task-context
reads. They are not for environment diagnostics or write authorization.

Do not scan this directory to choose a template by Agent intent. First use the
CLI catalog:

```powershell
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Then read only the concrete template paths returned by
`bh tools templates <tool_id>`.

Typical command:

```powershell
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file .\read.json --fields status,summary,artifacts.full_result
```

Use `logic_flow` first for simple function/event/custom event reads. Use
`logic_md` when the entry is larger or has enough branches that a separated
Entry / Execution / Data view is easier to scan. Use `logic_json` for full graph
reads, unknown size, block anchors, patch/merge, or debug. ReadSpec no longer
supports `view.format=summary`; non-logic templates omit `view.format`.

`logic_flow` templates are the fast first read for simple entries. They are not
anchor templates. If the result degrades to `logic_json`, use the returned
structured payload directly.
