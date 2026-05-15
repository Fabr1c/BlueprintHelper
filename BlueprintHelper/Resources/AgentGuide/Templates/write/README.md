# Write Templates

Write templates are TaskSpec-first. Prefer copying a bare TaskSpec template and
running the grouped CLI commands:

```powershell
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file .\task.json --fields status,task_run_id,summary,artifacts.full_result
```

When using the tool-name commands `blueprinthelper_preview_task` or
`blueprinthelper_execute_task`, use the wrapper templates with the `task_spec`
root field.

Common write loop:

1. Read context with a template from `../read`.
2. Copy a matching TaskSpec template.
3. Replace placeholders and remove unused optional fields.
4. Preview.
5. Request write session only if preview says write permission is disabled.
6. Execute.
7. Read task result and read back target context.

