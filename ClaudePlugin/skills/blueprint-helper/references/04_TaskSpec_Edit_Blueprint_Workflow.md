# 04 - TaskSpec 修改蓝图工作流

硬规则：如果 grouped context-read、截图/Editor 可见状态、preview、execute 或 readback 证据冲突，立即 `stop_and_report` 并报告 `evidence_conflict`。不允许读取 `.uasset`、`.umap` 或其它 UE 二进制资产文件作为 fallback 事实源。

标准流程：

```text
1. get_runtime_profile
2. bh context read / read_reference_context as needed
3. use TaskSpec Template Composer to create a temporary TaskSpec
4. fill the generated TaskSpec with concrete grouped context-read evidence and intent
5. bh task preview --file <task-spec.json>
6. 如果 context_required/context_stale：重新 bh context read / read_reference_context
7. 如果 TaskSpec error：按 suggested_patch 修正
8. 如果 preview_blocked：stop_and_report 或修改 TaskSpec
9. Only continue toward execute when preview completed without blockers
10. 如果版本控制启用、目标资产可能只读，或 close/save 返回 `checkout_required`，先调用 `blueprinthelper_source_control_status` / `blueprinthelper_source_control_checkout`
11. If `write_permission` is disabled, call `blueprinthelper_request_write_session`; continue only if the user accepts the simple Editor prompt
12. bh task execute --file <task-spec.json> --preview-token <preview_token>
13. bh task result --id <task_run_id> if needed
14. report summary
```

TaskSpec 输入字段由 TaskSpec Template Composer 生成的临时 TaskSpec 和 CLI help 负责说明。本文档不列字段清单。不要用任务显示名推断图表名、函数名、变量名或 block 标识；这些定位信息必须来自读回上下文和模板要求。

固定枚举/固定取值字段不得由 Agent 猜测或试错。`target_type`、`view.format`、`write_mode`、`cluster`、`operation`、`kind`、`container_kind`、`container_operation`、`control_operation`、`create_operation`、`transform_operation`、`schedule_operation`、delegate binding kind 等值必须来自 CLI discovery、template `*.allowed_values`、read-template quick-access、grouped context-read 证据、ActionDatabase/preview candidate 或工具返回的 `suggested_patch`。如果没有来源，停止并报告 `missing_capability` / `clarification_required` / `stop_and_report`。

使用 `bh task preview --file` / `bh task execute --file` 分组命令时，文件根对象必须是裸 `BlueprintHelper.TaskSpec.v1`。

## TaskSpec Template Composer

GraphWrite 写入前先用 CLI 四层索引收敛模板，不要扫描模板目录或手写完整 statement JSON：

```text
bh tools templates families --workflow preview_execute --format json
bh tools templates write-modes --family graph_write --format json
bh tools templates clusters --family graph_write --format json
bh tools templates operations --family graph_write --cluster generic_ops --write-mode graph.append --format json
bh tools templates quick-access --family graph_write --cluster generic_ops --operation let --write-mode graph.append --format json
bh tools templates quick-access --family graph_write --cluster generic_ops --operation expression --write-mode graph.append --format json
bh tools templates compose --family graph_write --write-mode graph.append --templates "generic_ops.let.default(generic_ops.expression.literal)" --out .tmp/taskspec-template-composer/graph_append.taskspec.json --format json
```

## GraphWrite Slot Expression

`quick-access.items[].template_id` 是 `compose --templates` 使用的 slot id。`quick-access.items[].slot_type` 说明这个 slot 能放在哪里：`statement` 可以作为顶层 root；`expression` 只能嵌套在某个 statement 的输入 slot 中。

`arg_slots` 的数组顺序就是括号内参数顺序。例如 statement 返回 `arg_slots:["value(ValueType)"]` 时，`generic_ops.let.default(generic_ops.expression.literal)` 表示把 literal expression 填入第 1 个输入位。动态输入返回多个同名 slot 时按位置使用；只填写第 3 个输入位时写 `generic_ops.call.direct(0,0,generic_ops.expression.get_symbol_or_variable)`，其中 `0` 是跳过符号，不是数字 literal。数字 0 必须通过 literal expression 在生成文件里填值。

多个顶层 statement 用逗号分隔，例如 `"slotA(...),slotB(...)"`。PowerShell 下必须把整个 `--templates` 字符串加引号。不要把 expression slot 放在顶层，也不要绕过 quick-access 返回的 slot id 去手写内部 statement 结构。

Patch/Merge 已有 BlueprintHelper-owned block 时，先用 `bh context read` 读取 `logic_json`。写入锚点必须来自 grouped block：`block_id + group_entry_node_path + node_ref + pin_ref`，`insert_between` 额外需要 `link_ref`。不要把全图级 `nodes[0]`、显示名、GUID-first selector 当普通主线写锚点。

## Non-BlueprintHelper-Owned Graph Boundary

Non-BlueprintHelper-owned graph content is read-only in the normal GraphWrite flow. Normal Agents may use `bh context read` or `blueprinthelper_read_reference_context` to inspect it, but must not build write anchors for it.

GraphWrite TaskSpecs must preserve the ownership boundary selected by the current template. If preview or compile reports an unsupported ownership scope, stop and report that stable non-owned write anchors are not available yet. Do not switch to full-graph `nodes[index]`, display labels, ad hoc JSONPath, or GUID-first selectors. GUID-first selectors remain expert/debug fallback only.

`merge_owned_graph` 使用 `branch_fork + owned_block_call` 时，Preview 是写入门禁。TaskSpec 必须显式给出 `sequence_order`，且只使用 `original_successor` / `inserted_logic`；`inserted.block_id` 必须在 Preview 阶段解析为已有的 BlueprintHelper-owned CustomEvent block。Preview blocked 时禁止 execute，也不要回退到底层工具。

execute_task 仍可能因 UE 当前状态、资产变化或 Editor 写入失败而失败；失败结果必须带非空 error code/message/stage，报告时使用该错误和 task result/journal，不展开底层 Bridge payload。

## Source Control / P4 Checkout

写入已有 UE 资产前，如果项目启用了 P4/Perforce 或其他 UE SourceControl Provider，必须在 preview 通过后、execute 前确认目标资产可编辑。先按 CLI catalog 选择 `blueprinthelper_source_control_status` 或 `blueprinthelper_source_control_checkout`，再按该工具 help/manifest 的输入形状准备 schema root JSON；不要使用旧的单 tool_id 模板分发。

`checkout_required` 时调用 `blueprinthelper_source_control_checkout`。如果返回 `checked_out_by_other`、`source_control_conflicted`、`source_control_unavailable`、`checkout_failed` 或 `not_editable`，立即停止并把结果中的 `agent_message` / `recommended_action` 报给主 Agent；不要继续 execute，也不要在 close editor 时把保存失败当成已关闭成功。

GraphWrite body 内的函数调用形状由 TaskSpec Template Composer 返回的 GraphWrite quick-access 模板负责说明；本文档只保留策略规则，不复制具体 statement JSON。

GraphWrite `branch_fork` 成功 execute 后必须读回。读回应确认：

- 新插入的 Sequence 或等价分发节点连接在 anchor 之后。
- `inserted` call 节点可达。
- 原 original successor 仍从 Sequence 分支可达。
- 受影响执行流无孤立节点。

execute_task 成功后，普通报告只输出任务摘要、目标资产、主要变更、编译/保存/未完成项，不展开完整 TaskPlan、child transaction、Journal 路径或底层 Bridge JSON。

## 2026-05-07 调用参数检查点

在执行 smoke 或写入任务前，先用当前 per-command help 和 CLI discovery/index 确认精确参数形状。不要把模板返回的根字段再包进额外 `args`。如果客户端要求对象字段传 JSON string，字段名仍保持在根对象。
