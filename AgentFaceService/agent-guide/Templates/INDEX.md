# Template Directory Notes

This file is not an Agent-facing selection index.

Use the CLI catalog to obtain the exact template path for a selected tool id:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Agents must not scan this directory to choose tools. This directory may contain
CLI invocation templates and TaskSpec semantic templates; read only the
concrete template paths returned by `bh tools templates <tool_id>`.

## 中文

本文件不是 Agent 选择工具或模板的索引。

Agent 必须先通过 CLI catalog 选择工具域、工具类型和工具 id，再只读取
`bh tools templates <tool_id>` 返回的具体模板路径。不要扫描 `Templates/`
目录来选择工具。
