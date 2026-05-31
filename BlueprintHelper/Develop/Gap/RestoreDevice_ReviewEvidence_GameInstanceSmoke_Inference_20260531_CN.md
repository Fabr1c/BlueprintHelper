# RestoreDevice ReviewEvidence GameInstance Smoke 推断文档

日期：2026-05-31

## 2026-06-01 收束更新：根因与证据链已补全

本地重新安装 CLI 后，已用 fresh GameInstance smoke 把两层问题拆开并闭环验证：

1. Layer A 是前置 gate：TaskSpec preview 不能再把 UE step failure 误报成顶层通过；GraphWrite member variable `get` 的 `field_owner_class` alias / owner inference 已补齐；TaskRuntime dry-run 会临时叠加同一 TaskPlan 前序 `blueprint_variable` step 计划新增的 member variable，让后续 `graph_write` dry-run 能解析这些变量。
2. Layer B 是 Review 空链路：最终根因不是泛化的 `Ownership` 字段缺失，也不是 ReviewStore / PostIO 全局不可用，而是 GraphWrite evidence target identity 与 Review snapshot 匹配规则不一致。
3. 更精确地说，`append_result.block_refs` 里返回的是短 ref `CE_DumpGlobalStateForReview0`，但 GraphWrite 写到节点 metadata 的 `BlueprintHelperBlockId` 是完整 id `EventGraph_CE_DumpGlobalStateForReview0`。Review snapshot 通过 target key 中的 `block` anchor 匹配节点 metadata；短 ref target key 无法命中新建 graph block，因此 graph body visible change 被过滤掉。
4. 修复后 GraphWrite review evidence 使用 `graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0`，fresh smoke 查询返回 1 条 pending review record，并生成 `append_blueprint_graph EventGraph` 的 graph body visible change。

关键运行证据：

```text
.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.preview_after_full_block_id.json
  preview Passed=true, dry_run.can_execute=true, 3 steps ResultOk=true

.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.execute_after_full_block_id.json
  task_run_id=task_D7CCB6D24E9EA20A9CC7FA913FDA62EF
  status=executed, PostIoOk=true, diagnostics=[]
  graph_write ResultOk=true, Modified=true, BlockRefs=CE_DumpGlobalStateForReview0

.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.query_review_records_after_full_block_id.json
  Count=1, Status=pending, EvidenceCount=3
  OperationKinds=add_blueprint_member_variables,ensure_custom_event,append_blueprint_graph
  VisibleChangeCount=1
  VisibleChange=append_blueprint_graph EventGraph
```

注意：`review query` summary 会裁剪 / sanitize `atomic_targets`，所以 graph atomic target 的最终证据来自持久化 record JSON，而不是 query summary 字段：

```text
D:\UEProjects\Template\Saved\BlueprintHelper\Review\Records\review_archive_B385C4E54BDD81FC5E41FCA55AB308EF_Game_BlueprintHelperTemp_BP_BH_Evidence_GameInstance_Fresh_20260601_0150.json
  target_key=graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0
  target_kind=graph_block
  before_hash=crc32_6449522f
  after_hash=crc32_2523dcac
  before_has_block_not_found=false
  after_has_nodes=true
```

已补的回归验证：

```text
TemplateEditor build: Succeeded
BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence: passed
BlueprintHelper.TaskRuntime.PostIO: passed
BlueprintHelper.TaskRuntime.GraphWrite.DryRunUsesPlannedMemberVariableState: passed
BlueprintHelper.GraphWrite.ActionContext.FieldExpressionProjectsOwnerEvidence: passed
BlueprintHelper.TaskRuntime.PartialPreviewCache: passed
BlueprintHelper.TaskRuntime.GraphWritePlanCache: passed
BlueprintHelper.Review.UI.LoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBody: passed
AgentFaceService/task-core focused tests: 309 passed, 0 failed
```

因此，下文 2026-05-31 的“粗粒度 `graph_block:<GraphName>` target identity”判断应理解为第一阶段定位；最终收束后的精确根因是：GraphWrite review evidence 必须从执行结果拿到实际 block refs，并把短 ref 归一到与节点 metadata 一致的完整 block id。

## 结论摘要

本地 GameInstance smoke 复现说明，`ReviewEvent` 整条链路为空不是因为蓝图函数逻辑没有生成，也不是因为 PostIO / ReviewStore 全局不可用。

更准确的结论是：GraphWrite 在执行阶段已经生成了实际节点并完成编译，但 GraphWrite 交给 ReviewStore 的 review evidence target 没有稳定绑定到 snapshot 层可匹配的真实 graph block。第一阶段证据表现为 target 过粗，只指向 `graph_block:<GraphName>`；进一步修复并复测后，精确根因收束为短 `block_ref` 与节点 metadata 中完整 `BlueprintHelperBlockId` 不一致。ReviewStore 随后按目标快照构造 visible changes 时找不到有效 graph body 差异，经过 net-no-change 过滤后，非空 evidence batch 被压缩成 0 条 review records，触发：

```text
review_evidence_produced_zero_records: Review evidence batch produced zero review records from 3 evidence item(s).
```

另一个已证实但独立的问题是：TaskSpec 中 `kind:get` 读取 member variable 时，如果没有 `context_evidence.field_owner_class`，GraphWrite preview 会在 field variable resolver 阶段失败。这会阻止长 flow 进入执行阶段，但它不是“执行已生成节点后 Review 为空”的根因。

## 本地测试范围

测试资产：

```text
/Game/BlueprintHelperTemp/BP_BH_Evidence_GameInstance
```

测试输出目录：

```text
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531
```

关键任务：

```text
task_98B86F5E440245831282A993BF119A67
task_A19CE5AD42B7F6E756A4349344C676A9
task_213369054361057887A8CD9558EE3434
```

## 证据链

### 1. Bridge / PostIO / ReviewStore 不是全局不可用

初始 `bh.cmd bridge ping` 返回 `bridge_unavailable`，原因是本地 Editor/Bridge 未启动：

```text
ECONNREFUSED 127.0.0.1:54321
```

通过 BlueprintHelper MCP 启动 `D:\UEProjects\Template\Template.uproject` 后，runtime diagnostics 通过，Bridge 连接可用。

资产创建任务 `create_gameinstance_asset.execute.json` 成功：

```text
status = executed
post_io.ok = true
compile_succeeded = true
timing includes post_io.review_record_write
```

这证明 PostIO、ReviewStore、review record write、pending review notify 这条基础链路本身可运行。

### 2. 没有 owner evidence 时，长 flow 在 preview 阶段失败

原始长 flow `write_global_state_flow.preview.json` 中，CLI 顶层显示 `preview_passed`，但 UE step 结果里 `step_003 graph_write` 实际失败：

```text
SemanticIR generation completed with 6 unresolved items.
First unresolved:
Expression CE_DumpGlobalStateForReview_stmt_2_arg_InString
Field variable action requires explicit owner evidence:
semantic=field
field_operation=get
field_scope=variable
query=SessionId
target=SessionId
property_path=.
```

这说明 `dynamic_cast` / exec cast 不是这里的阻塞点。阻塞点是 GraphWrite 的 member variable `get` 需要明确 owner evidence。

本地补充 `context_evidence.field_owner_class` 后，同一长 flow preview 通过。

### 3. 补齐 owner evidence 后，蓝图节点确实生成，但 review records 为 0

补齐 owner evidence 的长 flow 执行文件：

```text
write_global_state_flow.with_owner_evidence.execute.json
```

执行结果：

```text
status = executed
modified = true
compile_succeeded = true
append_blueprint_graph applied
spawned_node_count = 23
layout_record_node_count = 22
block_refs = CE_DumpGlobalStateForReview0
fragment_debug.fragment_evidence.review_scopes.scope_id = event:CE_DumpGlobalStateForReview
```

同一次执行的 PostIO 结果：

```text
post_io.ok = false
review_evidence_produced_zero_records:
Review evidence batch produced zero review records from 3 evidence item(s).
```

随后查询：

```text
write_global_state_flow.with_owner_evidence.query_review_records.json
records = []
count = 0
```

因此，本地已复现“真实蓝图内已经生成函数逻辑 node，但 review 查询整条为空”的症状。

### 4. 控制组证明 source review summary 存在，但 graph/signature 没有变成 visible changes

控制组 `write_review_evidence_only_flow.execute.json` 是一个更短的 literal flow，执行成功：

```text
append_blueprint_graph applied
spawned_node_count = 11
layout_record_node_count = 10
compile_succeeded = true
post_io.ok = true
```

查询结果 `write_review_evidence_only_flow.query_review_records.json` 返回 1 条 record：

```text
source_review_summary.evidence_count = 3
source_review_summary.operation_kinds =
  add_blueprint_member_variables
  ensure_custom_event
  append_blueprint_graph
visible_change_count = 4
```

但 `visible_changes` 只有变量：

```text
variable SessionId
variable CurrentScore
variable bHasSave
variable ScoresByPlayer
```

没有 custom event、graph body、graph node 的 visible change。

这证明 GraphWrite graph/signature evidence 至少进入了 source summary，但没有成功通过 ReviewStore 的 target snapshot / visible change 构造阶段。

## 代码路径推断

### PostIO 非空 evidence 变成 0 records 的入口

`BlueprintHelperTaskRuntimePostIoService.cpp` 中，PostIO 会把 task runtime 收集到的 `Batch.ReviewEvidences` 传给 ReviewStore：

```text
Batch.ReviewEvidences.Num() > 0
ReviewStore.BuildReviewRecordsFromEvidence(Batch.ReviewEvidences)
ReviewRecords.Num() == 0
=> review_evidence_produced_zero_records
```

本次长 flow 明确是“3 个 evidence item 输入，0 条 record 输出”，所以问题发生在 evidence 到 review record / visible changes 的转换阶段。

### GraphWrite evidence target 当前过粗

`BlueprintHelperGraphWriteTaskRuntimeCluster.cpp` 的 `BuildReviewEvidence` 当前对 graph write 使用：

```text
TargetKind = graph_block
TargetKey = graph_block:<GraphName>
Target.Ownership = graph_write
Target.AnchorJson = SerializePayloadForAnchor(LoweredStep.Payload)
```

关键问题是它没有消费执行结果里的：

```text
append_result.block_refs = CE_DumpGlobalStateForReview0
fragment_debug.fragment_evidence.review_scopes.scope_id = event:CE_DumpGlobalStateForReview
```

也就是说，执行阶段知道真实创建的 block ref / fragment scope，但 review evidence 仍然只指向 `graph_block:<GraphName>`。

### ReviewStore 会移除没有可见差异的 record

`BlueprintHelperReviewStoreService.cpp` 的构建逻辑会：

```text
AddEvidenceAtomicTargets(Evidence, Record)
NormalizeReviewTargetSemanticSnapshots(...)
RemoveNetNoChangeVisibleChanges(...)
if VisibleChanges.Num() == 0 skip record
```

`BlueprintHelperReviewBaselineSnapshotService.cpp` 对 graph block snapshot 的解析会从 target key 里提取 block id，再匹配节点上的 block id。若 target key 是 `graph_block:EventGraph`，而实际写入产生的是 `CE_DumpGlobalStateForReview0`，快照层很可能解析不到本次新增 block，形成 `block_not_found` 或 before/after 无有效差异。

随后 `RemoveNetNoChangeVisibleChanges` 会把这种没有净变化的 atomic target / visible change 移除。若变量也都是 no-op 或已存在，整条 record 就会被过滤成 0。

## 根因判断

### 已证实

1. 长 flow 原始 TaskSpec 缺少 `field_owner_class` 时，GraphWrite preview 会因 member variable `get` owner evidence 缺失失败。
2. 补齐 owner evidence 后，长 flow 能执行、能生成实际蓝图节点、能编译成功。
3. 同一次执行 PostIO 收到了 3 个 review evidence item，但 ReviewStore 构建出 0 条 review record。
4. 控制组证明 graph/signature evidence 能进入 source summary，但不会形成对应 visible changes。

### 高置信推断

真实 Review 空链路的主因是 GraphWrite review evidence 的 target identity 与实际生成结果不一致：

```text
当前 evidence target:
graph_block:<GraphName>

执行实际结果:
block_refs = CE_DumpGlobalStateForReview0
fragment review scope = event:CE_DumpGlobalStateForReview
```

因此 ReviewStore 不能把 evidence 映射到实际新增 graph body / event block，最终 visible changes 为空。

这不是简单的 `Target.Ownership` 字段没有设置。GraphWrite evidence 已经有 `Ownership = graph_write`；问题更接近“ownership/anchor 指向的对象身份不够具体或不匹配执行结果”。

## 伴随问题

### CLI preview 顶层状态不可信

原始 preview 中 UE step 已失败，但 CLI 顶层仍显示：

```text
status = preview_passed
passed = true
```

这会误导 agent 继续执行或错误归因。应单独修复 TaskSpec preview output contract：任一 UE step failed 时，顶层 preview 不应标记为 passed。

### field owner evidence 需要编译器或模板层明确处理

GraphWrite 当前要求 member variable field get 具备 owner evidence。可接受的修复方向有两种：

1. TaskSpec/compiler 在目标蓝图上下文明确时自动补齐 `field_owner_class`。
2. 模板/agent contract 明确要求所有 member variable `kind:get` 携带 `context_evidence.field_owner_class`，并在 preview 前给出可读错误。

无论采用哪种方式，它都是“让长 flow 能进入执行”的前置问题，不是“执行后 Review 空”的主根因。

## 修复入口建议

1. 调整 GraphWrite runtime review evidence 构造。
   - 入口：`BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
   - `BuildReviewEvidence` 应消费 `StepResult` 中的 `block_refs` / `fragment_debug.fragment_evidence.review_scopes`
   - graph body target 应绑定真实 block ref 或 fragment scope，而不是只绑定 `GraphName`

2. 调整 ReviewStore graph body snapshot 对 GraphWrite fragment evidence 的支持。
   - 入口：`BlueprintHelperReviewStoreService.cpp`
   - 对 append graph 的 review evidence 应能生成 graph/event visible change
   - `BuildReviewRecordsFromFragmentEvidence` 已存在，但当前 PostIO 走的是 `BuildReviewRecordsFromEvidence`

3. 增加回归测试。
   - 执行一个包含 `ensure_custom_event` + `append_blueprint_graph` 的 GameInstance TaskSpec
   - 断言 query review records 至少 1 条
   - 断言 visible changes 包含 graph/event change，不只是 source summary operation kind
   - 断言不再出现 `review_evidence_produced_zero_records`

4. 修复 TaskSpec preview 顶层状态。
   - UE step failed 时，CLI top-level `status/passed` 必须失败

5. 明确 member variable get 的 owner evidence 合约。
   - 自动推断或强制模板输出 `context_evidence.field_owner_class`
   - 保留清晰 preview error，避免 silent fallback

## 验证门槛

修复后，以下检查必须同时满足：

```text
write_global_state_flow.with_owner_evidence.preview.json
=> preview passed, no unresolved field owner errors

write_global_state_flow.with_owner_evidence.execute.json
=> compile_succeeded = true
=> post_io.ok = true
=> no review_evidence_produced_zero_records

write_global_state_flow.with_owner_evidence.query_review_records.json
=> count >= 1
=> visible_changes includes graph/event/body change
=> source_review_summary.operation_kinds includes append_blueprint_graph
```

并且原始缺 owner evidence 的 TaskSpec 不应再出现“UE step failed 但 CLI 顶层 preview_passed”的不一致状态。

## 附录：本次证据文件

```text
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\create_gameinstance_asset.execute.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_review_evidence_only_flow.execute.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_review_evidence_only_flow.query_review_records.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.preview.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.preview.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.execute.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.query_review_records.json
```
