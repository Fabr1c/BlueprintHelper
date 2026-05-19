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

## 读链路详细化

v0.5.0 的读链路计时目标不是把 UE 读操作并发化，而是先把 CLI 接收、AgentFace 编排、Bridge 往返、UE 主线程读取、AgentFace payload 规整和 CLI 返回分清。读链路仍以 GameThread 读取 UObject / Blueprint / UEdGraph / UWidgetTree / UProperty 为安全边界；后续可优化的是“GameThread 快照 + 后台格式化”，而不是让多个后台线程直接触碰 UE 对象。

### AgentFace read_context 主链路

1. CLI 接收命令并解析参数：`cli.parse_args`。
2. CLI 进入通用工具分支：`cli.invoke_tool` 包住整个 `read_context` 调用。
3. `executeReadContext()` 解析输入 schema：`read_context.parse_input`。
4. 根据 `read_type` / `view.format` 解析目标读格式：`read_context.resolve_format`。
5. 对非 logic 读构建 bridge request：`read_context.resolve_bridge_request`。
6. 构建 UE bridge payload，并在 develop 模式附加 `include_timing=true`：`read_context.build_bridge_payload`。
7. 发送 bridge 命令并等待 UE 返回：`read_context.bridge.<command>`。
8. develop 模式把 UE 回传的 timing 追加到 `data.timing.nested[]`，命名为 `ue.<command>`。
9. 解析 Bridge payload：`read_context.extract_bridge_payload`。
10. 移除诊断字段 `timing`，并按 read type 做 payload compact/filter：`read_context.post_process_payload`。
11. 包装 `ReadContextPack.v1` ToolResult：`read_context.result_wrap`。
12. CLI 返回前写入最终 `data.timing`：`cli.result_return`。

### read_type 到 UE 命令映射

| read_type | UE bridge command | UE 服务边界 | 主要成本来源 |
| --- | --- | --- | --- |
| `asset_context` | `get_asset_info` | AssetBrowse service | AssetRegistry / asset metadata 查询 |
| `blueprint_logic` + `logic_md` | `read_blueprint_logic_md` | LogicMdReadService | Blueprint/Graph 快照、逻辑分组、Markdown 格式化 |
| `blueprint_logic` + `logic_json` | `read_blueprint_logic_json` | LogicJsonReadService | Blueprint/Graph 快照、逻辑分组、JSON 规整 |
| `graph_context` | `read_blueprint_logic_json` | LogicJsonReadService | 单图目标解析、Graph 快照、JSON 规整 |
| `component_context` | `read_components` | Component service | Blueprint SCS / component 模板读取 |
| `variable_context` | `list_variables` | BlueprintStructure / Variable service | MyBlueprint 成员变量反射读取 |
| `variable_context:event_dispatcher` | `list_event_dispatchers` | BlueprintStructure service | Dispatcher 签名读取 |
| `widget_context` | `get_widget_tree` / `get_widget_properties` | UMGWidget service | UWidgetTree 遍历、Widget 属性读取 |
| `data_table_context` | `get_datatable_rows` | DataTable service | UDataTable 行读取和字段序列化 |
| `data_asset_context` / `object_property_context` | `get_object_properties` | PropertyReflection service | UObject / UProperty 反射读取 |

### UE 读链路计时边界

UE Bridge Router 统一消费 payload `include_timing=true`，只对读命令启用 `ue_bridge_router` timing。该 timing 不进入业务模型，也不改变读结果语义：

1. Bridge request 已完成 payload parse 后进入 router。
2. Router 完成 payload validation / authorization。
3. Router 根据 command/cluster 分发到对应 read route。
4. `route_execute` 覆盖当前 UE 读 route 的实际执行时间，包括具体 read service 内的 GameThread 对象读取和 DTO 构造。
5. Router 将 timing 附加到 ToolResult `data.timing`；raw payload 读命令则附加到顶层 `timing`。
6. AgentFace 抽取 timing 作为 nested diagnostic，并在最终 read payload 中移除该诊断字段，保证 agent-facing `payload` 不混入计时数据。

下一阶段如需进一步细化 UE read service 内部阶段，应沿用同一套 timing trace DTO，在 service 边界增加 `snapshot_read`、`dto_build`、`format_output` 等阶段，而不是在每个 UI/CLI 调用点写特判。

### 当前读链路抽样基线

测试输入目录：

`D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519`

产物目录：

`D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\timing_20260519_203352`

| Spec | 状态 | data.timing total_ms | wall_ms | 备注 |
| --- | --- | ---: | ---: | --- |
| `01_asset_context.json` | completed | 1668.343 | 1762.055 | 资产元信息读取 |
| `02_blueprint_logic_json.json` | completed | 1997.325 | 2091.087 | 全 Blueprint logic JSON |
| `03_blueprint_logic_md.json` | completed | 1811.723 | 1903.964 | 全 Blueprint logic Markdown |
| `04_eventgraph_logic_json.json` | completed | 1904.062 | 1998.434 | EventGraph logic JSON |
| `05_eventgraph_logic_md.json` | completed | 1905.955 | 1997.196 | EventGraph logic Markdown |
| `06_eventgraph_context_json.json` | completed | 1904.545 | 1998.536 | graph_context JSON |
| `07_components_context.json` | completed | 1898.746 | 1995.797 | components context |
| `08_variables_context.json` | completed | 1904.481 | 1997.009 | variables context |
| `09_event_dispatchers_context.json` | completed | 1906.919 | 1999.886 | event dispatchers context |
| `10_object_properties_context.json` | completed | 1903.216 | 1997.916 | object properties context |

读链路本次样本：count=10，avg=1880.532ms，min=1668.343ms，max=1997.325ms，p50=1904.062ms。旧样本只包含 CLI 总阶段；v0.5.0 继续测试时需要使用新的 read_context 分段计时和 UE nested timing 判断耗时集中在 AgentFace、Bridge round-trip、还是 UE route 执行。

实现后抽样验证：

| Spec | 验证结果 |
| --- | --- |
| `01_asset_context.json` | 已返回 AgentFace 分段：`read_context.parse_input`、`read_context.resolve_bridge_request`、`read_context.bridge.get_asset_info`、`read_context.extract_bridge_payload`、`read_context.post_process_payload`、`read_context.result_wrap`。 |
| `02_blueprint_logic_json.json` | 已返回 AgentFace 分段：`read_context.parse_input`、`read_context.resolve_format`、`read_context.build_bridge_payload`、`read_context.bridge.read_blueprint_logic_json`、`read_context.extract_bridge_payload`、`read_context.post_process_payload`、`read_context.result_wrap`。 |

当前已启动 Editor 未返回 UE nested read timing，判断为运行中 Editor 仍使用重编译前已加载的插件模块；需要 Editor 重载/重启后重新跑 read Spec，以验证 `ue_bridge_router.route_execute` nested timing。

## 读工具优化计划

读工具优化进入 v0.5.0 计划，但优化方向必须保持通用性和线程安全边界：UE 对象读取仍在 GameThread 完成，后台线程只处理已经脱离 UObject 的纯 DTO。当前不把“多个后台线程直接并发读 Blueprint / UEdGraph / UWidgetTree / UProperty”列为优化方案。

### R0：补齐读工具端到端计时

目标：让所有读工具和 TaskSpec 写链路使用同一套 develop timing 观察方式，先证明耗时集中点再改结构。

计划：
- 保持 CLI `--develop` 作为唯一 CLI 诊断开关，普通调用不启动 timing、不返回 `data.timing`。
- `read_context` 已记录 AgentFace 编排阶段：parse、format resolve、route resolve、payload build、Bridge round-trip、payload extract、post-process、result wrap。
- UE Bridge read router 使用 `include_timing=true` 返回 `ue_bridge_router.route_execute`，用于拆分 Bridge 往返与 UE route 内部耗时。
- Editor 重载/重启后重新跑读 Spec，补齐 `data.timing.nested[].source=ue_bridge_router` 的实测数据。

验收：
- 所有 read_context develop 结果都包含 AgentFace 分段 timing。
- 当前 ReadSpecs 至少覆盖 `asset_context`、`blueprint_logic`、`graph_context`、`component_context`、`variable_context`、`event_dispatchers_context`、`object_properties_context`。
- UE nested timing 在重载新 DLL 后能显示 `route_execute`。

### R1：GameThread 快照，后台格式化

目标：降低长读工具的主线程占用，尤其是 `blueprint_logic` 的 Markdown/JSON 输出和后续大 payload 规整。

计划：
- 在 UE read service 边界拆出纯数据 Snapshot DTO，例如 `BlueprintLogicSnapshot`、`ComponentSnapshot`、`WidgetTreeSnapshot`、`ObjectPropertySnapshot`。
- GameThread 只负责解析目标资产、读取 UObject/Blueprint/Graph/WidgetTree/Property 并填充 Snapshot DTO。
- 后台阶段只接收 Snapshot DTO，不持有 `UObject*`、`UEdGraph*`、`UWidget*`、`FProperty*` 或任何 Editor 对象指针。
- Markdown、JSON、compact/filter、统计信息生成等格式化逻辑迁移到纯 DTO formatter。
- 对 `blueprint_logic_md` / `blueprint_logic_json` 优先落地，因为它们最容易把对象读取和输出格式化混在同一条重路径里。

验收：
- 后台 formatter 可以在无 UObject 访问的单元测试里运行。
- UE read timing 至少能区分 `snapshot_read` 和 `format_output`。
- `snapshot_read` 仍在 GameThread，`format_output` 不调用 UE Editor API。
- 输出 schema 与现有 `LogicMd.v1`、`LogicJson.v1`、`ReadContextPack.v1` 保持一致。

### R2：读 DTO / Formatter 复用边界

目标：避免每个 read command 各自维护一套输出解释，提升读工具复用性。

计划：
- 按领域建立复用 formatter：Logic、Component、Variable、Widget、DataTable、ObjectProperty。
- `read_context` 只负责 route 和通用 post-process，不内联领域业务解释。
- UE service 返回领域 DTO，AgentFace 层只做 agent-facing compact/filter，不重建业务语义。
- DebugBundle、Review evidence、UI overlay 如果需要读模型，应复用同一类 Snapshot/DTO，而不是各自重新解释 Blueprint 状态。

验收：
- 新增 read type 时优先扩展 read route builder、UE service、DTO formatter，而不是在 CLI 或 UI 入口写特判。
- 同一类读输出的字段命名、计数、target 描述由同一 formatter 生成。
- AgentFace compact/filter 不改变 UE DTO 的业务含义。

### R3：同资产多格式复用，暂不做泛化 batch read 主线

目标：只在有实际重复成本证据时复用同一资产快照，不把“批量读”作为替代 full blueprint 读的主优化。

计划：
- 对同一请求链路内的 `blueprint_logic_md` 与 `blueprint_logic_json`，允许复用同一个 `BlueprintLogicSnapshot`。
- 若未来出现同一资产连续多次读不同视图的场景，可以增加 request-level snapshot cache，生命周期仅限一次 CLI/tool 调用。
- 不做长期 Blueprint 读缓存，除非有明确的资产变更失效策略。
- 不把“按 asset 分组的 batch read”列为 v0.5.0 主优化，因为已有 full blueprint read 能覆盖大量一次性上下文读取需求。

验收：
- 单次请求内复用快照不改变任何输出字段。
- cache key 至少包含 asset path、target graph/function/event/block、read detail、format 相关选项。
- 请求结束后释放快照，避免跨用户编辑状态污染。

### R4：纯数据缓存仅限非 UE 核心对象状态

目标：缓存只用于稳定的纯数据，不缓存可能被用户编辑即时改变的 Blueprint / UObject 状态。

计划：
- 允许缓存 read capability matrix、CLI schema metadata、runtime profile 中不触碰 UE 核心对象的纯数据。
- 不缓存 Blueprint 图、WidgetTree、DataTable rows、DataAsset properties 等用户可编辑资产内容。
- 如需缓存资产内容，必须先定义明确的 package dirty、asset save、editor change event 或 request-local invalidation 策略。

验收：
- 缓存数据不依赖 UObject 生命周期。
- 用户在 Editor 内修改资产后，普通读工具不会返回旧 Blueprint / Widget / Property 状态。
- runtime profile 类读取若只涉及插件能力信息，可以并发或缓存；涉及 UE 反射/资产状态时仍走 GameThread 快照。

### R5：读链路指标与回归门槛

目标：用统一指标判断读工具优化是否有效，避免只凭单次 wall time 判断。

计划：
- 基线指标使用 `data.timing.total_ms`、`read_context.bridge.<command>`、nested `ue_bridge_router.route_execute`。
- 长读工具额外记录 `snapshot_read`、`format_output`、`payload_size_bytes`。
- 读优化完成后重跑同一组 ReadSpecs，并与写链路对照组分开统计。

验收：
- `blueprint_logic_json/md` 至少能显示主线程快照和后台格式化的耗时占比。
- 普通读工具没有 develop timing 字段泄漏。
- 优化前后同一 ReadSpec 输出 payload 保持结构兼容。

## 写链路详细对照组

写链路作为 v0.5.0 优化的对照组保留完整 TaskSpec execute 形态，原因是它覆盖了当前最重的路径：TaskSpec 文件读取、TaskSpec->TaskPlan 编译、UE preview/dry-run、UE execute、Review evidence、资产 mutation、compile/save/post operation 和结果包装。

### 当前写链路主流程

1. CLI 接收 `task execute --input <TaskSpec> --develop`。
2. `cli.parse_args` 解析参数。
3. `taskspec_file_read_parse` 读取并解析 TaskSpec 文件。
4. `taskspec_compile` 将 `BlueprintHelper.TaskSpec.v1` 编译为 TaskPlan。
5. `bridge.preview_task_plan` 发送 preview payload，develop 模式附加 `include_timing=true`。
6. UE `preview_task_plan` 进入 TaskRuntime `RunTaskPlan(true)`，执行 task plan parse、review baseline policy、step lowering、CallFunction resolution、dry-run cluster execution、review snapshot 和 result wrap。
7. preview 成功后，AgentFace 发送 `bridge.execute_task_plan`。
8. UE `execute_task_plan` 进入 TaskRuntime `RunTaskPlan(false)`，执行真实 mutation、review evidence/record、graph layout flush、compile/save 等后处理。
9. AgentFace `result_wrap` 汇总 preview/execute 结果。
10. CLI `cli.result_return` 写入最终 `data.timing`。

### 当前写链路抽样基线

测试输入目录：

`D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.3\ArchivedReference\RetiredReviewDebugDocs_20260518\PlanArtifacts\ReviewPanel_UI_Test_TaskSpecs_20260518`

产物目录：

`D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\timing_20260519_203352`

| Spec | 状态 | data.timing total_ms | wall_ms | 备注 |
| --- | --- | ---: | ---: | --- |
| `01_create_blueprint_actor.json` | executed | 778.544 | 877.910 | 创建 Blueprint Actor |
| `02_edit_blueprint_components.json` | executed | 1828.563 | 1929.450 | 组件写入 |
| `03_edit_blueprint_variables.json` | executed | 2240.277 | 2336.795 | 变量写入 |
| `04_edit_blueprint_signatures.json` | executed | 2177.983 | 2278.201 | 函数/事件签名写入 |
| `04b_write_function_body.json` | executed | 3246.974 | 3341.578 | 函数图写入 |
| `05_append_graph_review_body.json` | failed | 936.306 | 1032.360 | preview 被 review baseline dirty policy 阻塞 |
| `06_create_structure_row.json` | executed | 2835.556 | 2929.841 | Struct 行创建 |
| `07_create_data_table.json` | executed | 1682.314 | 1776.274 | DataTable 创建 |
| `08_edit_data_table_rows.json` | executed | 2248.840 | 2341.396 | DataTable 行写入 |
| `09_create_data_asset_class.json` | executed | 2396.901 | 2490.762 | DataAsset class 创建 |
| `10_edit_data_asset_class_variables.json` | executed | 1702.325 | 1797.226 | DataAsset class 变量写入 |
| `11_create_data_asset_instance.json` | executed | 2604.367 | 2702.276 | DataAsset instance 创建 |
| `12_edit_data_asset_properties.json` | executed | 2197.007 | 2302.228 | DataAsset 属性写入 |
| `13_create_widget_blueprint.json` | executed | 2531.668 | 2624.600 | Widget Blueprint 创建 |
| `14a_edit_widget_tree_root.json` | executed | 1839.756 | 1937.416 | Widget root 写入 |
| `14b_edit_widget_tree_child.json` | executed | 2209.757 | 2307.205 | Widget child 写入 |
| `14c_edit_widget_tree_property.json` | executed | 2247.066 | 2341.360 | Widget property 写入 |

写链路成功样本：count=16，avg=2172.994ms，min=778.544ms，max=3246.974ms，p50=2209.757ms。包含失败样本：count=17，avg=2100.247ms，min=778.544ms，max=3246.974ms，p50=2209.757ms。

代表性最慢成功样本 `04b_write_function_body.json`：

| 阶段 | duration_ms |
| --- | ---: |
| `cli.parse_args` | 0.277 |
| `taskspec_file_read_parse` | 2.483 |
| `taskspec_compile` | 48.703 |
| `bridge.preview_task_plan` | 2063.882 |
| `bridge.execute_task_plan` | 1129.964 |
| `result_wrap` | 0.535 |
| `cli.result_return` | 0.005 |
| nested `ue.preview_task_plan` total | 456.449 |
| nested `ue.execute_task_plan` total | 791.983 |

对照结论：AgentFace 编译不是主耗时；当前写链路耗时主要落在 Bridge preview/execute round-trip 和 UE TaskRuntime 执行。v0.5.0 的优化应优先验证 P0-1 preview 复用/跳过、P0-2 dry_run_mode、P0-3 CallFunction resolution cache、P2-6 Review IO，以及 P2-7 三层 TaskRuntime 是否实际降低写链路 p50 和最慢样本。

## v0.5.0 优化项

### P0-0：TaskSpec 到返回结果的端到端计时流程

目标：在执行任何性能优化前，先建立稳定的耗时证据链，覆盖从 TaskSpec 输入到最终 ToolResult 返回的主链路。

状态：已落地基础计时流程，并收敛到 develop 诊断模式下启用。

已覆盖：
- 仅当 CLI 附带 `--develop`，或 MCP/tool 输入显式传入 `develop: true` 时启动计时。
- CLI 侧计时是通用能力：从 CLI 接收命令并完成参数解析开始，覆盖所有 CLI 工具分支，到 ToolResult 返回前统一写入 `data.timing`。
- AgentFace TaskSpec tool surface：develop 模式记录 `taskspec_parse`。
- AgentFace CLI：develop 模式对所有 CLI 命令记录 `cli.parse_args`、命令执行阶段和 `cli.result_return`；TaskSpec 文件入口额外记录 `taskspec_file_read_parse`。
- AgentFace read_context：develop 模式记录 `read_context.parse_input`、`read_context.resolve_format`、`read_context.resolve_bridge_request`、`read_context.build_bridge_payload`、`read_context.bridge.<command>`、`read_context.extract_bridge_payload`、`read_context.post_process_payload`、`read_context.result_wrap`。
- AgentFace TaskSpec runner：develop 模式记录 `taskspec_compile`、`bridge.preview_task_plan`、`bridge.execute_task_plan`、`result_wrap`。
- UE TaskRuntime：仅在 payload `include_timing=true` 时记录 `parse_task_plan`、`review_baseline_policy`、`review_baseline_capture`、`review_archive_session_write`、每个 step 的 `lowering`、`call_function_resolution`、`review_before_snapshot`、`cluster_execute`、`review_after_record_write`、`graph_layout_flush`、`post_operation.compile`、`post_operation.save`、`result_wrap`。
- UE Bridge read router：仅在 read payload `include_timing=true` 时记录 `ue_bridge_router` 的 `route_execute`，用于区分 AgentFace/Bridge round-trip 和 UE read route 成本。
- ToolResult `data.timing` 只在 develop 模式返回 AgentFace 端计时。
- AgentFace `data.timing.nested` 只在 develop 模式挂载 UE preview/execute 的 `BlueprintHelper.TimingTrace.v1`。
- UE TaskRunJournal 只在 develop 模式同步记录 TaskRuntime timing，便于 `get_task_result` 后查。

验收：
- 普通 CLI 和 MCP/tool 调用返回体不包含 `data.timing`，且不会启动 AgentFace/UE timing trace。
- 附带 CLI `--develop` 时，所有 CLI 工具返回体的 ToolResult 均包含 `data.timing`。
- MCP/tool 输入显式传入 `develop: true` 时，TaskSpec preview/execute 返回体包含 `data.timing`。
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
- 普通路径保持无计时采集、无 `data.timing` 返回；CLI 诊断路径通过 `--develop` 对所有 CLI 工具显式开启，TaskSpec MCP/tool 诊断路径通过 `develop: true` 显式开启。
- 后续实现必须保持高内聚、低耦合，避免把性能分支堆进单个 service 或 UI 入口。
