# AgentFace 工具返回字段冗余与 GUID 暴露检查报告

日期: 2026-05-17

## 2026-05-17 处理记录

- 已在 AgentFaceService task-core 结果层新增统一 Agent-facing sanitizer，默认递归移除 `*guid*` 字段。
- 已将 review 内部选择字段 `target_key` / `target_keys` / `atomic_targets` / `visual_group_key` 从 Agent-facing 默认返回中移除，避免 graph node GUID 通过 target key 间接泄露。
- 已把 `blueprinthelper_apply_review_action` 标记为 `audience=expert` 且 `requiresExpert=true`，不再属于普通 Agent 默认工具面。
- 已在 CLI 和剩余 MCP allowlist 输出适配层补安全网，stdout、structuredContent、CLI full_result artifact 都使用清洗后的 ToolResult。
- 已删除废弃 MCP 普通工具回归/导入/task/debug 测试；后续不允许恢复、补测或运行这些废弃工具测试。
- 保留 UE 插件内部 Review/Journal/GraphBounds/rollback 的 `NodeGuid`，因为这些是内部定位和回滚证据，不作为 Agent-facing 默认返回字段。

## 范围

本次检查覆盖当前 AgentFace 面向 Agent 的工具面和相关返回协议:

- AgentFace shared registry: `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts`
- CLI 调用入口: `AgentFaceService/cli/src/cli/run.ts`, `AgentFaceService/cli/src/cli/tool-command.ts`, `AgentFaceService/cli/src/cli/output.ts`
- MCP allowlist 注册入口: `AgentFaceService/mcp/src/mcp/tools/editor-lifecycle-tools.ts`
- Codex Agent 指南: `CodexPlugin/skills/blueprint-helper/SKILL.md`
- UE Bridge 侧返回 DTO: `BlueprintHelper/Source/BlueprintHelper/...`

本报告是静态代码和文档检查，没有启动编辑器做 live tool output smoke。

## 总结

1. 已修正普通 Agent 文档面和 CLI 实际可调用面不一致的问题: `blueprinthelper_apply_review_action` 已改为 `audience: expert` 且 `requiresExpert: true`。
2. 已处理 AgentFace 明确 UE GUID 暴露点: `blueprinthelper_read_context` 的 `logic_json` payload 即使 Bridge 返回 `node_guid`, Agent-facing ToolResult 也会默认清洗。
3. `blueprinthelper_query_review_records` 返回的是 summary artifact, 当前不返回 `atomic_targets`, `target_key`, `node_guid`。`blueprinthelper_apply_review_action` 的 `target_keys` 已从默认 Agent-facing 返回中移除。
4. `blueprinthelper_request_write_session` 当前会从 Bridge 结果中读取 `session_id` 并写入本地 BridgeClient, 但返回给 Agent 的 `write_session` 已脱敏, 不返回 `session_id`, `auth_session`, `auth_token` 或 `BLUEPRINTHELPER_BRIDGE_TOKEN`。
5. 最大的冗余不是单个业务字段, 而是通用 envelope 叠加: CLI 默认返回 `CliResult`, full artifact 再保存完整 `toolResult + extra`；剩余 MCP allowlist 也会输出 `ToolResultBase`。这套结构利于追踪和调试, 但普通 Agent 的日常路径需要继续依赖 `--fields` / `--omit` 或模板默认裁剪。

## 当前开放面

| 面 | 当前机制 | 结论 |
| --- | --- | --- |
| Codex 普通 Agent 指南 | `SKILL.md` 列出默认工具, 明确 `apply_review_action` 非 ordinary workflow | 文档意图是窄工具面 |
| CLI 直接工具名 | `run.ts` 对 `getBlueprintHelperTool(group)` 放行, `tool-command.ts` 检查 `requiresExpert` | `apply_review_action` 已要求 `--expert`, 不属于普通 CLI 工作流 |
| shared-registry MCP | 非普通 Agent 入口；`apply_review_action` 已是 expert | 不作为废弃普通 MCP 工具面的恢复路径 |
| MCP allowlist | `editor-lifecycle-tools.ts` 注册 open/close/developer exec | 只保留 editor open、editor close、developer-only exec command |
| legacy direct MCP tools | `register-tools.ts` 内仍有 old `server.registerTool` 代码 | 当前不得开放、不得补测、不得运行旧测试 |

## 通用返回结构冗余

`ToolResultBase` 固定返回:

```json
{
  "ok": true,
  "schema": "BlueprintHelper.ToolResult.v1",
  "operation": "...",
  "trace_id": "...",
  "status": "completed",
  "modified": false,
  "target": {},
  "data": {}
}
```

这些字段里, `schema`, `operation`, `status`, `modified`, `trace_id` 是统一 envelope 字段。它们对日志、失败诊断和多工具串联有价值, 但对只看业务结果的 Agent 来说经常是冗余字段。

判断:

- 保留 outer envelope 是合理的, 不建议全局删除。
- 对无目标读工具, 例如 `read_agent_guide`, `diagnostics`, `request_write_session`, `open_editor`, `close_editor`, 当前 `{ target_type: "asset" }` 没有实际信息, 可以后续移除或只在需要 asset path 时返回。
- CLI 默认 summary 已经是降噪层, 但 `full_result` artifact 永远保存完整 `ToolResultBase`。这适合作为调试路径, 不适合让普通 Agent 每次读取完整 artifact。

## 逐工具检查

| 工具 | GUID 暴露 | 冗余/风险字段 | 建议 |
| --- | --- | --- | --- |
| `blueprint_get_runtime_profile` | 未发现 UE GUID 字段 | 负向稀疏返回, 冗余低 | 保持 |
| `blueprinthelper_diagnostics` | 未发现 UE GUID 字段 | outer `target` 无意义, `markdown` 是预期主体 | 可移除 placeholder target |
| `blueprinthelper_diagnostics_runtime` | 未发现 UE GUID 字段 | 与 static diagnostics 类似 | 可移除 placeholder target |
| `blueprinthelper_read_agent_guide` | 未发现 UE GUID 字段 | outer `target` 无意义, `markdown` 较大但符合工具用途 | 可移除 placeholder target |
| `blueprinthelper_read_context` | `logic_json` 会返回 `node_guid` | outer `target` 与请求重复；`data.payload` 可能带 asset path / graph；`logic_json` 是最大 GUID 暴露点 | 默认普通输出应隐藏 `node_guid`, 仅 `detail=debug` 或 expert 路径允许；普通锚点保留 `block_id`, `group_entry_node_path`, `node_ref`, `pin_ref`, `link_ref` |
| `blueprinthelper_read_task_context` | 未发现直接 UE GUID 字段 | outer `target.asset_path` 与 `data.target.asset_path` 重复；`runtime.profile` 可能较大 | outer target 可保留作 summary, 但默认 summary 不必展开 runtime full profile |
| `blueprinthelper_read_reference_context` | 当前 DTO 不含 `target_guid` / `node_guid`; `context_id` 是 opaque id | 当前 schema 仍是 `BlueprintHelper.ReferenceContextPack.v1`; outer target 重复请求 target | 按新设计改为 `ReferenceContextPack.v1`, 不返回 `target` / `query` 块, 不新增 `target_guid` |
| `blueprinthelper_preview_task` | TS 层无显式 GUID 字段；`issues` 是 passthrough, 需防 UE issue 扩展泄露 | `status=dry_run` 与 `passed/blocked` 信息重叠；`task_plan.target_assets` 与 outer target 重复 | 保留 `passed/blocked`; 对 issue 扩展加 no-GUID snapshot test |
| `blueprinthelper_execute_task` | 已经通过 Agent-facing sanitizer 清洗 `bridge_result` 中的 GUID 字段 | `data.task_run_id` 和 `data.task.task_run_id` 重复；`bridge_result` 可能过大 | 后续仍可把 raw bridge result 收到 debug artifact |
| `blueprinthelper_get_task_result` | 已经通过 Agent-facing sanitizer 清洗 journal 默认返回中的 GUID 字段 | outer `target` 无实际 asset path；调用者已持有 `task_run_id`, 但 journal 回填 `task_run_id` 有价值 | outer target 可后续移除 |
| `blueprinthelper_get_debug_case` | `debug_case_id`, `trace_ids`, `review_record_ids` 是 opaque ids, 不是 UE GUID；未见 `node_guid` | summary-only 设计正确 | 保持 summary-only, 不打开 raw DebugBundle |
| `blueprinthelper_list_debug_cases` | 同上 | `count/returned_count/limit` 有用 | 保持 |
| `blueprinthelper_export_debug_bundle` | `bundle_id` 由 GUID 生成但不是 UE node/asset GUID；manifest 只返回相对 artifact refs | 仍是 debug 入口, 但当前未返回本地绝对路径 | 保持 summary manifest |
| `blueprinthelper_query_review_records` | summary artifact 当前不返回 `node_guid` / `target_key` / `atomic_targets`；`visual_group_key` 也会被 sanitizer 从默认返回移除 | `data.query` 重复输入过滤条件；`change_id` 是内部 review identity | 可移除默认 `query`; 如需回显过滤条件放 `detail=debug` |
| `blueprinthelper_apply_review_action` | `target_keys` 可包含 graph node GUID 形态, 现已从默认返回清洗 | 已改为 expert-only | 普通 Agent 面不开放 |
| `blueprinthelper_request_write_session` | 返回不暴露 `session_id`; 无 auth token 暴露 | outer target 无意义；`asset_paths` 回显授权范围, 有用 | 保持脱敏; 可移除 placeholder target |
| `blueprint_open_editor` | 无 UE GUID | `editor_exe`, `uproj_path`, `launch_command`, `editor_args` 是本机路径/命令回显 | lifecycle 工具可保留 debug 信息, 但普通报告可裁剪 `launch_command` |
| `blueprint_close_editor` | 无 UE GUID | `bridge_response` 是 raw-ish 子结果, `pids` 是进程 ID | `bridge_response` 默认可压缩为 `method/code/elapsed_ms`; raw 放 debug |

## GUID 暴露细节

### 1. `read_context logic_json` 明确暴露 `node_guid`

C++ 逻辑 JSON 导出会写入:

```cpp
NodeObj->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
```

AgentFace 的 `blueprinthelper_read_context` 对 `logic_json` 会把 Bridge payload 放到 `data.payload`。因此普通 Agent 只要请求 `view.format=logic_json`, 就能看到 UE 节点 GUID。

这和当前 AgentGuide 的语义有冲突: 文档要求 patch/merge anchor 来自 grouped block 的 `block_id + group_entry_node_path + node_ref + pin_ref`, 同时要求不要把 GUID-first selector 当普通主线写锚点。当前输出虽然不要求 Agent 传 `target_guid`, 但仍把可被误用的 `node_guid` 放在普通读结果里。

建议:

- 默认 `logic_json` 删除或重命名隐藏 raw `node_guid`。
- 如果某些内部修复仍需要 GUID, 放到 `detail=debug` 或 expert-only output。
- 普通 Agent 的稳定锚点继续使用 `node_ref`, `pin_ref`, `link_ref`, `block_id`, `group_entry_node_path`。

### 2. Review summary 默认不暴露 `node_guid`, 但内部 apply action 有风险

ReviewRecord 完整持久化结构里, atomic target 会序列化 `node_guid`。但 `query_review_records` 当前调用 `BuildReviewRecordSummaryArtifact`, 只返回 record/change summary, 不返回 `atomic_targets`。

风险在 `apply_review_action`:

- 输入字段含 `target_keys`。
- 返回字段会回显 `target_keys`。
- graph node target key 在若干路径中可能包含 node GUID, 例如 `...:node:<guid>`。
- 该工具已从 registry default 面收回到 expert。

因此 `apply_review_action` 已按文档口径收回到 internal/expert 面。

### 3. ReferenceContext 当前没有 `target_guid`, 但 schema/目标回显还没完全收敛

当前 `ReferenceContextPack` DTO:

- schema: `BlueprintHelper.ReferenceContextPack.v1`
- 包含 `context_id`
- 不包含 `target_guid`
- 不包含 `node_guid`
- 不包含完整 `target` / `query` 块

但 Bridge 外层 `ToolResultBase.target` 会回显目标字段, 且 schema 带 `BlueprintHelper.` 前缀。按照 2026-05-17 新设计, 内层业务 schema 应改成 `ReferenceContextPack.v1`, 默认不返回 `target` / `query`, 也不新增 GUID 输入/输出。

### 4. 普通 ID 与 UE GUID 的区分

以下字段是工具链 opaque id, 不应按 UE GUID 暴露问题处理:

- `trace_id`
- `task_run_id`
- `preview_id`
- `debug_case_id`
- `bundle_id`
- `review_record_id`
- `archive_session_id`
- `context_id`

它们可能由时间戳或 `FGuid::NewGuid()` 派生, 但语义上不是 UE asset/node/member GUID, Agent 无法用它们直接定位 UE graph 节点。

## 冗余字段分级

| 级别 | 字段/模式 | 判断 |
| --- | --- | --- |
| 必须保留 | `ok`, `error`, `debug_case_ids` on failure, root `task_run_id`, `preview_id`, `summary`, `issues` | Agent 决策和恢复需要 |
| 可保留但默认裁剪 | `trace_id`, `operation`, `status`, `modified`, outer `target` | 追踪有用, 日常输出冗余 |
| 应从普通业务 payload 移除 | `data.query` 回显, nested `task.task_run_id`, placeholder `{target_type:"asset"}` | 调用者已知道或没有信息量 |
| 已加 gate 或 redaction | `node_guid`, review `target_keys` 内嵌 GUID, raw `bridge_result`, raw `bridge_response` | 普通 Agent 默认输出已走 sanitizer |
| 已正确脱敏 | write session `session_id`, auth token/session, DebugBundle 本地绝对路径/raw payload | 当前符合安全预期 |

## 建议落地项

1. 已修正 `blueprinthelper_apply_review_action` 元数据: `audience=expert`, `requiresExpert=true`。
2. 已给 Agent-facing 结果层增加输出整形测试: 默认结果不得出现 `node_guid`, `target_guid`, `guid`, `target_key`, `target_keys`, `atomic_targets`, `visual_group_key`。
3. 已覆盖 `read_context logic_json` 的 no-GUID registry 输出测试。
4. 已覆盖 `apply_review_action` expert-only 与返回清洗测试。
5. 实现 ReferenceContext 新合同: 内层 schema 去 `BlueprintHelper.` 前缀, 默认不返回 `target` / `query`, 不支持 `target_guid`, 不返回具体 GUID。
6. 清理无信息量 outer target: `read_agent_guide`, `diagnostics`, `request_write_session`, `get_task_result`, lifecycle 工具可以不填 placeholder target。

## 当前不需要立即改的内容

- `trace_id` 等 envelope 字段不建议从 `ToolResultBase` 删除, 因为它们服务跨工具追踪。
- `debug_case_id`, `review_record_id`, `task_run_id` 这类 opaque id 不等同于 UE GUID, 可以继续返回。
- `export_debug_bundle` 当前只返回 summary manifest 和相对 ref, 没看到本地绝对路径或 raw artifact 内容泄露。

## 已定稿的产品口径

`read_context logic_json` 不允许普通 Agent 默认看到 `node_guid` 或其他 UE GUID 字段。

普通 Agent 默认不看 raw UE GUID。需要可写锚点时, 使用 BlueprintHelper 自己的稳定、语义化锚点字段；GUID 只留在 UE 插件内部和明确的开发/debug 证据路径中，不作为 Agent-facing 默认返回字段。
