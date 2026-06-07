# 04 - TaskSpec 修改蓝图工作流

标准流程：

```text
1. get_runtime_profile
2. read_context / read_reference_context as needed
3. use TaskSpec Template Composer to create a temporary TaskSpec
4. fill the generated TaskSpec with concrete read_context evidence and intent
5. preview_task
6. 如果 context_required/context_stale：重新 read_context / read_reference_context
7. 如果 TaskSpec error：按 suggested_patch 修正
8. 如果 preview_blocked：stop_and_report 或修改 TaskSpec
9. Only continue toward execute when preview completed without blockers
10. 如果版本控制启用、目标资产可能只读，或 close/save 返回 `checkout_required`，先调用 `blueprinthelper_source_control_status` / `blueprinthelper_source_control_checkout`
11. If `write_permission` is disabled, call `blueprinthelper_request_write_session`; continue only if the user accepts the simple Editor prompt
12. execute_task
13. get_task_result if needed
14. report summary
```

TaskSpec 输入字段由 TaskSpec Template Composer 生成的临时 TaskSpec 和 CLI help 负责说明。本文档不列字段清单。不要用任务显示名推断图表名、函数名、变量名或 block 标识；这些定位信息必须来自读回上下文和模板要求。

TaskSpec 模板发现和生成只走 `bh tools templates` 四层索引，不使用旧 tool-id 模板发现入口，也不手扫 `AgentFaceService/agent-guide/Templates` 目录：

```powershell
bh tools templates families --workflow preview_execute --format json
bh tools templates write-modes --family graph_write --format json
bh tools templates clusters --family graph_write --format json
bh tools templates operations --family graph_write --cluster generic_ops --write-mode graph.append --format json
bh tools templates quick-access --family graph_write --cluster generic_ops --operation call --write-mode graph.append --format json
bh tools templates compose --family graph_write --write-mode graph.append --templates generic_ops.call.direct --out .tmp/taskspec-template-composer/graph_append.taskspec.json --format json
```

调用 `blueprinthelper_preview_task` / `blueprinthelper_execute_task` 工具名入口或 `task preview --file` / `task execute --file` 分组命令前，必须通过当前 CLI help 和 composer 输出确认 wrapper 与裸 TaskSpec 的差异。不要把 wrapper 传给分组命令，也不要额外包 `args`。

Patch/Merge 已有 BlueprintHelper-owned block 时，先用读工具获取结构化上下文。写入锚点必须来自当前模板要求的 grouped block 读回信息。不要把全图级数组位置、显示名或 GUID-first selector 当普通主线写锚点。

## Non-BlueprintHelper-Owned Graph Boundary

Non-BlueprintHelper-owned graph content is read-only in the normal GraphWrite flow. Normal Agents may use `blueprinthelper_read_context` or `blueprinthelper_read_reference_context` to inspect it, but must not build write anchors for it.

GraphWrite writes must preserve the current ownership policy selected by the template. If preview or compile returns an unsupported-scope diagnostic, stop and report that stable non-owned write anchors are not available yet. Do not switch to full-graph indexes, display labels, ad hoc JSONPath, or GUID-first selectors. GUID-first selectors remain expert/debug fallback only.

对已有 owned block 做分支插入时，Preview 是写入门禁。所需字段和值由当前模板负责说明；Preview blocked 时禁止 execute，也不要回退到底层工具。

execute_task 仍可能因 UE 当前状态、资产变化或 Editor 写入失败而失败；失败结果必须带非空 error code/message/stage，报告时使用该错误和 task result/journal，不展开底层 Bridge payload。

## Source Control / P4 Checkout

写入已有 UE 资产前，如果项目启用了 P4/Perforce 或其他 UE SourceControl Provider，必须在 preview 通过后、execute 前确认目标资产可编辑。按运行时 CLI 发现到的 source-control status / checkout 能力执行；本文档不维护静态 catalog 说明。

`checkout_required` 时调用 `blueprinthelper_source_control_checkout`。如果返回 `checked_out_by_other`、`source_control_conflicted`、`source_control_unavailable`、`checkout_failed` 或 `not_editable`，立即停止并把结果中的 `agent_message` / `recommended_action` 报给主 Agent；不要继续 execute，也不要在 close editor 时把保存失败当成已关闭成功。

GraphWrite body 内的函数调用形状由 TaskSpec Template Composer 返回的 GraphWrite quick-access 模板负责说明；本文档只保留策略规则，不复制具体 statement JSON。

## GraphWrite AutoSearch Candidate Flow

AutoSearch is a Preview resolution strategy for GraphWrite calls, not a new statement kind. Use the CLI-discovered GraphWrite template for exact fields when broad callable search or preview-recovery search is needed.

When GraphWrite Preview returns an AutoSearch candidate-required diagnostic, do not execute. Read the returned candidates, apply the current template's selection field, rerun Preview, and execute only after Preview passes.

Candidate ids are preview-scoped selection tokens. Treat them as opaque; do not persist them as UE node ids, capability ids, function stable ids, or raw ActionDatabase evidence. Public candidate data must not include raw spawner or stable-id internals.

GraphWrite `branch_fork` 成功 execute 后必须读回。读回应确认：

- 新插入的 Sequence 或等价分发节点连接在 anchor 之后。
- `inserted` call 节点可达。
- 原 original successor 仍从 Sequence 分支可达。
- 受影响执行流无孤立节点。

execute_task 成功后，普通报告只输出任务摘要、目标资产、主要变更、编译/保存/未完成项，不展开完整 TaskPlan、child transaction、Journal 路径或底层 Bridge JSON。

## 2026-05-07 调用参数检查点

在执行 smoke 或写入任务前，先用当前 per-command help 和 CLI discovery/index 确认精确参数形状。不要把模板返回的根字段再包进额外 `args`。如果客户端要求对象字段传 JSON string，字段名仍保持在根对象。
