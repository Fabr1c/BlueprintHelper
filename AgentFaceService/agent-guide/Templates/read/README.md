# Read Templates / 读取模板

## 中文

这些模板用于 UE asset、Blueprint logic、dependency 和 task-context 读取。它们不用于环境诊断或写入授权。

使用 `SEMANTIC_INDEX.md` 按 Agent 意图选择模板。

典型命令：

```powershell
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file .\read.json --fields status,summary,artifacts.full_result
```

图表大小未知时先用 `summary`，不要直接读取 whole-graph `logic_md`。需要稳定 `block_id`、`node_ref`、`pin_ref` 或 `link_ref` anchor 以编写 patch/merge TaskSpec 时，使用 `logic_json`。

## English

Use these templates for UE asset, Blueprint logic, dependency, and task-context reads. They are not for environment diagnostics or write authorization.

Use `SEMANTIC_INDEX.md` to choose a template by Agent intent.

Typical command:

```powershell
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file .\read.json --fields status,summary,artifacts.full_result
```

Use `summary` before whole-graph `logic_md` when graph size is unknown. Use `logic_json` when you need stable `block_id`, `node_ref`, `pin_ref`, or `link_ref` anchors for patch or merge TaskSpecs.
