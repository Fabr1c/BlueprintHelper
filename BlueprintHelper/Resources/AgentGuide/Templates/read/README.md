# Read Templates

Use these templates for UE asset, Blueprint logic, dependency, and task-context
reads. They are not for environment diagnostics or write authorization.

Typical command:

```powershell
node .\AgentFaceService\cli\build\cli\index.js blueprinthelper_read_context --file .\read.json --fields status,summary,artifacts.full_result
```

Use `summary` before whole-graph `logic_md` when graph size is unknown. Use
`logic_json` when you need stable `block_id`, `node_ref`, `pin_ref`, or
`link_ref` anchors for patch or merge TaskSpecs.

