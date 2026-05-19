# BlueprintHelper v0.5.0 TaskSpec 执行链路性能优化计划

日期：2026-05-19

## 版本目标

计划在 v0.5.0 优化从提交 `BlueprintHelper.TaskSpec.v1` 到 UE 真实执行开始之间的延迟。

本计划优先关注 `TaskSpec -> TaskPlan` 转换链路，但当前只读扫描和本地抽样结论显示：转换本身不是 2-5s 延迟的主要来源。当前默认 Python 子进程编译约 43-60ms，TS in-process 编译约 0.4-3ms；更大的延迟风险来自 execute 前强制 preview、UE dry-run 真实生成预览图、CallFunction 重复解析，以及 Review 快照和记录 IO。

## 当前链路判断

1. AgentFace `executeTask()` 当前会先执行 `previewTask()`，preview 成功后才发送 `execute_task_plan`。
2. 默认 `TaskSpec -> TaskPlan` 编译器为 Python 子进程，每次编译通过 stdin/stdout 传输 JSON。
3. UE 侧 preview 和 execute 都进入 `RunTaskPlan`，preview 不是纯 schema 校验。
4. GraphWrite dry-run 会运行语义构建、预览图生成和回滚，成本接近一次真实图写入前半段。
5. `dry_run_mode` 字段已经存在于 schema，但 runtime 当前没有使用该字段降低 preview 成本。
6. CallFunction 在 runtime 预解析和 GraphStatementBuilder 生成节点阶段存在重复解析风险，preview 与 execute 又会重复一次。
7. Review baseline snapshot、semantic snapshot、review record merge/write 属于真实 execute 侧成本，可能放大任务启动和收口耗时。

## v0.5.0 优化项

### P0-0：TaskSpec 到返回结果的端到端计时流程

目标：在执行任何性能优化前，先建立稳定的耗时证据链，覆盖从 TaskSpec 输入到最终 ToolResult 返回的主链路。

状态：已落地基础计时流程，并收敛到 develop 诊断模式下启用。

已覆盖：
- 仅当 CLI 附带 `--develop`，或 MCP/tool 输入显式传入 `develop: true` 时启动计时。
- AgentFace TaskSpec tool surface：develop 模式记录 `taskspec_parse`。
- AgentFace CLI `task preview/execute`：develop 模式记录 `taskspec_file_read_parse`。
- AgentFace TaskSpec runner：develop 模式记录 `taskspec_compile`、`bridge.preview_task_plan`、`bridge.execute_task_plan`、`result_wrap`。
- UE TaskRuntime：仅在 payload `include_timing=true` 时记录 `parse_task_plan`、`review_baseline_policy`、`review_baseline_capture`、`review_archive_session_write`、每个 step 的 `lowering`、`call_function_resolution`、`review_before_snapshot`、`cluster_execute`、`review_after_record_write`、`graph_layout_flush`、`post_operation.compile`、`post_operation.save`、`result_wrap`。
- ToolResult `data.timing` 只在 develop 模式返回 AgentFace 端计时。
- AgentFace `data.timing.nested` 只在 develop 模式挂载 UE preview/execute 的 `BlueprintHelper.TimingTrace.v1`。
- UE TaskRunJournal 只在 develop 模式同步记录 TaskRuntime timing，便于 `get_task_result` 后查。

验收：
- 普通 `blueprinthelper_preview_task` 和 `blueprinthelper_execute_task` 返回体不包含 `data.timing`，且不会启动 AgentFace/UE timing trace。
- 附带 CLI `--develop`，或 MCP/tool 输入显式传入 `develop: true` 时，返回体包含 `data.timing`。
- execute 返回体能区分 AgentFace 编译、Bridge preview、Bridge execute、结果包装耗时。
- nested UE timing 能继续细分 TaskRuntime 内部阶段。
- 计时字段只作为诊断数据，不改变 TaskPlan、Review evidence、资产写入语义。

### P0-1：execute 支持 preview 复用或跳过二次 preview

目标：避免同一个 TaskSpec 在 execute 前重复做完整 UE dry-run。

计划：
- preview 返回可复用的 `preview_id`、`task_plan_hash` 或等价 token。
- execute 接受 preview token，在 TaskSpec hash、目标资产状态、执行策略未变化时复用已编译 TaskPlan。
- 对没有 preview token 的调用保留现有安全路径，避免破坏当前 CLI/MCP 调用。
- 复用失败时返回明确诊断，不静默降级为错误执行。

验收：
- 已 preview 的任务进入 execute 时，不再重复调用同一次完整 `RunTaskPlan(true)`。
- 未 preview 的任务仍保持现有行为。
- 复用路径有 TaskSpec hash、目标资产 dirty/hash、execution policy 的一致性校验。

### P0-2：让 `dry_run_mode=quick|none` 真正生效

目标：把 dry-run 从单一重型路径拆成可控策略，降低真实执行前等待。

计划：
- `full` 保持当前行为，用于高风险写入。
- `quick` 只做 schema、lowering、SemanticIR、目标解析和 CallFunction 预解析，不生成预览图。
- `none` 用于可信内部链路或已复用 preview 的 execute，不做 UE preview。
- runtime 明确消费 `execution_policy.dry_run_mode`，并在返回结果中写明实际采用的 dry-run 策略。

验收：
- `quick` 不触发 GraphWrite 预览图生成和回滚。
- `none` 不触发 `RunTaskPlan(true)`。
- 默认策略保持向后兼容，不影响未显式配置的调用。

### P0-3：缓存并传递 CallFunction resolution 结果

目标：消除 preview/execute 以及 runtime/GraphStatementBuilder 之间的重复 CallFunction 解析。

计划：
- 在单个 TaskPlan 执行上下文内建立 request-level resolution cache。
- cache key 至少包含 query、search_mode、参数类型、target object type、blueprint context。
- runtime 预解析结果传递给 GraphStatementBuilder，GraphStatementBuilder 优先使用已解析结果。
- 对完全相同的 CallFunction statement 做去重解析。
- 对 editor-session 级候选 universe 可做短生命周期缓存，但必须有 Blueprint/action database 失效策略。

验收：
- 同一个 TaskPlan 中相同 CallFunction 查询只解析一次。
- preview 生成节点阶段不再重复解析 runtime 已解析的调用。
- execute 复用 preview 时可复用已确认的 resolution 结果，或显式校验后重用。

### P1-4：TaskSpec 编译优先走 in-process fast path

目标：减少 Python 子进程启动和 JSON 往返的固定开销。

计划：
- 对已覆盖且通过 parity 校验的 task type，允许使用 TS in-process fast path。
- Python compiler 保留为 canonical fallback，避免未覆盖 task type 或复杂 composite 行为漂移。
- 如果继续坚持 Python 作为唯一生产 compiler，则替代方案为 long-lived Python worker，避免每次 spawn。
- v0.5.0 实施前需要明确 compiler policy：TS fast path + parity gate，或 Python worker。

验收：
- 支持类型的编译固定开销从约 43-60ms 降到毫秒级，或 Python worker 去掉每次 spawn 成本。
- TS/Python 输出必须有契约测试覆盖，禁止出现同一 TaskSpec 生成不同 TaskPlan 的漂移。
- 生产入口策略要和架构边界测试同步更新。

### P1-5：减少 Python compile 无用输出

目标：降低大 TaskPlan 场景下的序列化和解析成本。

计划：
- 默认 compile 输出只返回 runner 必需的 `task_plan`。
- `bridge_payload`、`task_plan_summary` 改为 debug/diagnostic 模式按需输出。
- runner 侧继续使用统一 summary 生成方式，避免 Python/TS summary 双源。

验收：
- 大 TaskSpec 编译输出体积下降。
- 现有 CLI/MCP 正常执行不依赖被裁剪字段。
- debug 模式仍可拿到完整诊断信息。

### P2-6：Review 快照和记录写入异步化或批处理

目标：降低真实 execute 阶段 Review IO 对任务启动和收口耗时的影响。

计划：
- 对 baseline snapshot、semantic snapshot、review record merge/write 增加阶段耗时采样。
- 保持一致性要求的快照仍同步执行。
- 可延迟的记录整理、archive 写入、重复 target 快照改为批处理或后台队列。
- 对同一 TaskPlan 内相同 review target 做去重。

验收：
- Review IO 有明确耗时指标。
- 同一任务不会重复捕获等价 target 快照。
- 异步化不影响 reject/apply review action 的可恢复性。

### P2-7：UE TaskRuntime 三层执行模型

目标：把当前 `RunTaskPlan` 中混合在一个同步流程里的准备、UE 主线程写入、结果 IO 拆成三层，降低耦合并为后续安全并发留出边界。

三层模型：
- `PurePrepare`：只处理纯数据，不触碰 UObject。负责 TaskPlan 结构读取、step id / depends_on / target_assets / execution_policy 规范化、step 顺序或 DAG 计划构建、JSON 到 lowered payload 的纯转换、CallFunction query 收集和去重 key 构建。
- `MainThreadCommit`：唯一允许触碰 UObject / Blueprint / UEdGraph / transaction 的层。负责 baseline policy 的写入前屏障、写入前快照、Blueprint/Graph 解析、CallFunction 对 UE 上下文的解析、cluster 执行、GraphWrite dry-run 或真实写入、before/after target snapshot 采集、graph layout flush、compile/save。
- `PostIO`：负责不改变 UE 对象状态的结果收口。包括 ReviewRecord 批量 merge/write、TaskRunJournal 持久化或内存登记、DebugEntry best-effort 写入、runtime facts 附加、archive session 元数据 flush。

边界要求：
- baseline snapshot 是写入前屏障，不能作为普通 PostIO 延后到 mutation 之后。
- `PurePrepare` 不得调用 `ResolveBlueprint`、查找 `UEdGraph`、遍历 node/pin、访问 `FBlueprintActionDatabase`、`Modify()`、`MarkPackageDirty()`、compile/save。
- `MainThreadCommit` 保持按 asset lock 和 step dependency 串行提交；v0.5.0 不把 UObject 写入并发化。
- 层间只传 DTO，例如 `PreparedTaskRun`、`PreparedStep`、`CommitResult`、`PostIoBatch`，避免重新形成大 service 隐式共享状态。

验收：
- `RunTaskPlan` 可以从编排角度清晰映射到 `PurePrepare -> MainThreadCommit -> PostIO`。
- 只有 `MainThreadCommit` 层触碰 UObject 和 Editor API。
- Review record 写入从逐 step 即时写入改为可批处理的 PostIO 批次，且不破坏 reject/apply review action 的可恢复性。
- 分阶段耗时至少能区分 pure prepare、main-thread commit、post IO。

## 优先级排序

1. P0-0 TaskSpec 到返回结果的端到端计时流程。
2. P0-1 preview 复用或跳过二次 preview。
3. P0-2 `dry_run_mode` 策略落地。
4. P0-3 CallFunction resolution 缓存和结果传递。
5. P1-4 TaskSpec 编译 fast path 或 Python worker。
6. P1-5 Python compile 输出裁剪。
7. P2-6 Review IO 批处理和异步化。
8. P2-7 UE TaskRuntime 三层执行模型。

## 度量要求

v0.5.0 实施前需要补齐分阶段耗时记录：

- AgentFace schema parse。
- `TaskSpec -> TaskPlan` compile。
- bridge preview round-trip。
- UE TaskPlan parse/lowering。
- CallFunction resolution。
- GraphWrite dry-run generation。
- Review baseline/semantic snapshot。
- execute graph generation。
- compile/save post operation。
- review record/archive write。
- UE `PurePrepare` / `MainThreadCommit` / `PostIO` 三层耗时。

## 风险和前置决策

1. TS fast path 与当前“生产入口默认 Python compiler”的既有约束存在策略冲突，必须先决定是引入 TS fast path，还是改为 long-lived Python worker。
2. `dry_run_mode=none` 只能用于可信链路或已有 preview 复用的链路，不能成为默认安全策略。
3. CallFunction editor-session 级缓存必须有失效条件，否则可能在 Blueprint 或 ActionDatabase 更新后使用旧候选。
4. Review IO 异步化不得破坏 review reject/apply 的可恢复性。
5. UE 三层拆分必须保持 baseline snapshot 在 mutation 前完成，不能为了异步化改变 Review 证据语义。

## 当前状态

- 状态：P0-0 develop 诊断计时流程已开始实现，P0-1 之后仍为 v0.5.0 优化计划。
- 普通路径保持无计时采集、无 `data.timing` 返回；诊断路径通过 `--develop` 或 `develop: true` 显式开启。
- 后续实现必须保持高内聚、低耦合，避免把性能分支堆进单个 service 或 UI 入口。
