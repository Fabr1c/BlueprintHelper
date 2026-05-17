# Write Templates / 写入模板

## 中文

写入模板都是 TaskSpec-first。优先复制 bare TaskSpec 模板，并使用分组 CLI 命令：

```powershell
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file .\task.json --fields status,task_run_id,summary,artifacts.full_result
```

使用 `SEMANTIC_INDEX.md` 按 Agent 意图选择模板。

当使用 `blueprinthelper_preview_task` 或 `blueprinthelper_execute_task` 工具名命令时，使用带 `task_spec` 根字段的 wrapper 模板。

常见写入循环：

1. 用 `../read` 中的模板读取上下文。
2. 复制匹配的 TaskSpec 模板。
3. 替换占位符并删除未使用的可选字段。
4. Preview。
5. 只有 preview 提示 write permission disabled 时才 request write session。
6. Execute。
7. 读取 task result，并回读目标上下文。

## English

Write templates are TaskSpec-first. Prefer copying a bare TaskSpec template and using grouped CLI commands:

```powershell
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file .\task.json --fields status,task_run_id,summary,artifacts.full_result
```

Use `SEMANTIC_INDEX.md` to choose a template by Agent intent.

When using the tool-name commands `blueprinthelper_preview_task` or `blueprinthelper_execute_task`, use wrapper templates with the `task_spec` root field.

Common write loop:

1. Read context with a template from `../read`.
2. Copy a matching TaskSpec template.
3. Replace placeholders and remove unused optional fields.
4. Preview.
5. Request write session only if preview says write permission is disabled.
6. Execute.
7. Read task result and read back target context.
