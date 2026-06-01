# BlueprintHelper 全插件性能优化源码审查

日期：2026-06-01  
性质：只读源码审查；未修改运行时代码；未执行性能补丁。  
范围：v0.5.0/v0.5.1/v0.5.2 性能归档、当前 UE C++ Bridge/TaskRuntime/GraphWrite/ReadContext/Review 源码、AgentFaceService TS CLI/Bridge/TaskSpec 源码。

## 1. 总结论

当前最值得优先投入的性能方向不是试图在一个 UE Editor 进程内直接突破 GameThread 对 `UObject`、`UBlueprint`、`UEdGraph`、`UEdGraphNode`、`UWidgetTree`、`FProperty` 等编辑器对象读写限制。该限制不能安全绕过。

可行路线是把倒三角上方继续拉宽：CLI/TS/协议/纯计划/纯格式化/缓存/索引可以并发、预编译、复用；Bridge 层需要从“单连接读一条、GameThread 执行一条、等待回写一条”改造成有 backpressure 的请求调度器；UE 层只保留真正必须触碰 Editor 对象的短窗口快照与提交。

若目标是实质性“突破单 Editor GameThread 总吞吐”，安全边界不是多线程直接读写同一个 Editor 的 UObject，而是多 Editor 进程分片。该方案只适合读多写少或资产互斥严格的批处理场景，需要单独设计项目/内容目录/包保存/资产注册锁，不应作为当前 Bridge 的轻量补丁。

## 2. 已参考的 v0.5.0 优化基线

v0.5.0 的性能线已经完成了一批关键优化，当前审查以这些实现为基线，而不是重新提出已完成工作。

| 阶段 | 已完成优化 | 证据 |
| --- | --- | --- |
| P0 | TaskSpec 执行端到端 timing、preview token 复用、`dry_run_mode`、CallFunction resolution cache | `BlueprintHelper/Develop/v0.5.2/README.md:11` |
| P1 | TaskSpec compiler fast path；compile-only avg 从 `44.990ms` 到 `0.941ms` | `BlueprintHelper/Develop/v0.5.0/BlueprintHelper_v0.5.0_Final_ReadWrite_PerformanceReport_20260520_CN.md:125` |
| P2 | TaskRuntime 拆成 `PurePrepare -> MainThreadCommit -> PostIO`；Review IO 批处理 | `BlueprintHelper/Develop/v0.5.0/P2_TaskRuntimeReviewIO_ImplementationPlan_CN.md` |
| P3 | 读链路 GameThread 快照 + DTO formatter + request-local cache；读链路 median wall avg 到 `144.029ms` | `BlueprintHelper/Develop/v0.5.0/BlueprintHelper_v0.5.0_Final_ReadWrite_PerformanceReport_20260520_CN.md:79` |
| P4 | partial preview cache、CallFunction facts TTL cache、GraphWrite pure DTO plan cache | `BlueprintHelper/Develop/v0.5.2/README.md:11` |
| P5 | GraphWrite `GraphMutationPlan`、`GraphWriteContext`、独立 executor、pin lookup cache；`cluster_execute` 从 `275.529ms` 到 `33.771ms` | `BlueprintHelper/Develop/v0.5.0/BlueprintHelper_v0.5.0_Final_ReadWrite_PerformanceReport_20260520_CN.md:149` |
| P6 | compile/save `PostOperationPlanner`、target asset 去重、clean save skip、per-asset diagnostics | `BlueprintHelper/Develop/v0.5.2/README.md:17` |

最终归档数字显示，写链路 workflow avg 从 `2172.994ms` 到 `1472.806ms`，p50 从 `2209.757ms` 到 `1505.115ms`；读链路 11 Spec median wall avg 从 `1997.088ms` 到 `144.029ms`，提升 `92.788%`。但同一报告也记录了 `02_blueprint_logic_json.json` 的长尾样本：`bridge_send_receive_ms=962.643`，`ue_total_ms=0.448`，说明尾延迟可能主要卡在 Bridge/transport 等待，而不是 UE route 本身。

## 3. 当前倒三角瓶颈图

```text
TS / CLI / TaskSpec
  并发空间最大：schema parse、TaskSpec compile、preview plan、文件读取、JSON 结构化、只读聚合。

Bridge / Protocol / Request Scheduler
  当前瓶颈最明显：连接接收、请求读取、GameThread 入队、Future.Get 等待、回写响应形成队头阻塞。

TaskRuntime / GraphWrite / ReadContext / Review
  部分可拆：纯计划、缓存 key、DTO 格式化、Review 索引、PostIO。
  必须串行：真正 UObject/图/资产读写、编译、保存、Modify、MarkDirty、NotifyGraphChanged。

UE Editor GameThread
  不能安全绕过：只能缩短窗口、批量提交、资产级互斥、快照后后台计算。
```

## 4. P0：Bridge 服务端接入模型是最高杠杆点

### 4.1 当前问题

`FBlueprintHelperBridgeServer::Run` 在监听线程里 `Accept` 一个 socket 后直接进入 `HandleClient`，直到该连接退出才回到下一次 accept。

证据：

- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp:122` 调用 `Accept`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp:129` 同线程直接调用 `HandleClient(ClientSocket)`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp:144` 进入连接内循环。

连接内每条非 `ping` / `client_disconnect` 请求都会：

1. 读取完整请求 frame；
2. 解析 JSON；
3. `AsyncTask(ENamedThreads::GameThread, ...)`；
4. 在 Bridge 线程 `Future.Get()` 等待 GameThread 执行与序列化完成；
5. 回写响应；
6. 再读下一条请求。

证据：

- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp:241` 在 GameThread 调 `Router.HandleRequestWithPlan`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeServer.cpp:272` 在 Bridge 线程等待 `Future.Get()`。

这意味着：

- 多 CLI 多 socket 到达时，新连接可能被一个长连接阻塞在应用层 accept 后面。
- 单 CLI 单 socket 即使 TS 客户端有 pending map，也会被 UE 服务端队头阻塞。
- 长写请求会拖住短读请求，p95/p99 会显著放大。

### 4.2 建议方向

新增可复用的 Bridge 请求调度边界，而不是在单个 command handler 内补丁：

1. `BridgeConnectionWorker`：accept 线程只负责接收 socket，并把每个连接交给 worker 或任务池。
2. `BridgeRequestScheduler`：按 `request_id` 独立建请求上下文，支持异步完成后回写，不要求响应按请求顺序返回。
3. `BridgeGameThreadQueue`：只把需要 UE 对象访问的最短闭包投递到 GameThread，并记录 enqueue wait / execute / serialize / write timing。
4. `BridgeBackpressurePolicy`：按客户端、全局、命令类别限制 in-flight 数量；超限返回 busy/retry-after 或排队。
5. `BridgeAssetWriteGate`：写请求按资产路径/包路径互斥，读请求可在不触碰 UObject 时并发；同资产写仍串行。

验收指标：

- N 个并发 CLI read 请求，p50/p95 不应随 N 线性增长。
- 一个长写请求执行中，短 `ping` / 纯静态 route 不应被应用层 accept 阻塞。
- `bridge.game_thread_enqueue_wait`、`bridge.route_execute`、`bridge.response_write` 分项必须能区分。

## 5. P0：RoutePlanner 线程分类过粗

当前 route planner 把所有已知命令都标记为需要 GameThread：

- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp:125`：`Plan.bRequiresGameThread = Plan.bKnownCommand;`

同时 BridgeServer 当前实际执行也没有利用 `bRequiresGameThread` 做分流：除了 `ping` 与 `client_disconnect`，其余请求统一投递到 GameThread。

建议把命令分为至少四类：

| 类别 | 执行线程 | 示例 | 约束 |
| --- | --- | --- | --- |
| TransportOnly | Bridge/worker | `ping`、断连、协议能力查询 | 不访问 UObject，不访问项目资产 |
| PureRuntime | Bridge/worker | schema/capability/static config、已缓存的纯规则、TaskSpec 静态编译 | 不访问 UObject；只读普通文件需受路径白名单保护 |
| SnapshotRead | GameThread snapshot + worker format | Blueprint/Graph/UMG/DataTable 读 | GT 只生成 DTO；格式化、压缩、响应序列化 off-GT |
| AssetWrite | GameThread commit + worker post-IO | GraphWrite/AssetFactory/Review accept/reject/compile/save | 资产级互斥；compile/save 去重；PostIO 脱离 GT |

这不会破坏 UE 线程安全边界，但可以减少无意义的 GameThread 排队。

## 6. P1：CLI/TS 侧需要 backpressure 和批量请求语义

TS `BridgeClient` 已经有 `pending Map`，按 `request_id` 匹配响应：

- `AgentFaceService/task-core/src/bridge/bridge-client.ts:82` 定义 `pending`。
- `AgentFaceService/task-core/src/bridge/bridge-client.ts:123` 进入 `sendRaw`。
- `AgentFaceService/task-core/src/bridge/bridge-client.ts:140` 写入 pending。
- `AgentFaceService/task-core/src/bridge/bridge-client.ts:292` 按 `request_id` 取回 pending。

CLI runtime 只有进程内连接复用：

- `AgentFaceService/cli/src/cli/run.ts:69` 定义 `runtimeBridgeCache`。
- `AgentFaceService/cli/src/cli/run.ts:458` 读取缓存 bridge。
- `AgentFaceService/cli/src/cli/run.ts:472` 写入缓存 bridge。
- `AgentFaceService/cli/src/cli/run.ts:476` 提供关闭 CLI-owned bridge。

当前缺口：

- 没有跨进程连接池，多 CLI 等于多 socket。
- 单连接 pending 无上限，服务端阻塞时会堆积 Promise、timeout 和内存压力。
- 没有明确区分 network timeout、Bridge busy、GameThread queue timeout、业务执行 timeout。
- 没有短窗口读请求聚合，多个 `ReadContext` 格式读取容易重复排队。

建议：

1. 在 `BridgeClient` 加可配置 in-flight semaphore，默认小上限，支持队列等待时间统计。
2. 增加错误分类：`bridge_busy`、`bridge_queue_timeout`、`bridge_transport_timeout`、`route_execution_timeout`。
3. 对只读 `ReadContext` 加短窗口 request coalescing：同 asset/target/schema 的请求共享一个 in-flight 结果。
4. 对批量 read/write 增加显式 batch route，不依赖用户并发多个 CLI 触发。

## 7. P1：ReadContext 跨请求共享与多视图读取

当前 ReadContext 已有 request-local cache：

- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:1127` 创建 `FBlueprintHelperLogicReadRequestSnapshotCache`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:1185` 另一条 read route 也创建 request-local cache。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.cpp:54` 标记部分 DTO 只能 request-local，除非有显式 invalidation。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperReadCachePolicy.cpp:76` 提供 `IsRequestLocalOnly`。

现有 request-local 策略是安全的，但对连续 CLI 请求没有复用。建议分两步：

1. 先做显式多视图 read route：一次 GameThread snapshot，同时输出 `logic_json`、`logic_md`、`logic_flow`、summary 等需要的视图，避免多个 CLI 往返。
2. 再做 Runtime 级短 TTL read cache，仅允许有清晰 invalidation 的数据进入跨请求缓存。缓存 key 至少包含 asset path、target type/name、graph/function scope、payload schema、serializer version、asset package dirty/version 指纹。

不建议把 UObject 指针、UEdGraph 指针或 pin 指针跨请求缓存。跨请求只能缓存纯 DTO、稳定序列化字节或 hash。

## 8. P1：TaskSpec 编译与计划缓存仍有上层空间

v0.5.0 已证明 TaskSpec compiler fast path 收益极高，但当前上层仍可继续做跨会话缓存：

- key：`TaskSpec canonical hash + schema version + plugin version + compiler version + feature flag digest`
- value：`TaskPlan`、diagnostics、normalized execution_policy、capability lowering result
- 层级：进程内 LRU + 可选磁盘缓存

要求：

- 缓存只覆盖纯编译输出，不包含 UObject、编辑器状态或 Preview 执行结果。
- 缓存命中必须保留 parity gate：同输入 deterministic；schema/compiler 版本变化自动失效。
- preview 与 execute 之间应优先复用已验证 preview token / plan，不重复 compile/preview。

## 9. P1：写链路按资产级批处理和后处理去重

当前 TaskRuntime 已经持有多级缓存和 post-operation planner：

- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:5368` 初始化 PartialPreview cache。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:5369` 初始化 CallFunction resolution cache。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:5370` 初始化 GraphWrite plan cache。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:6699` 调用 `PostOperationPlanner`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationPlanner.cpp:42` 构造 post operation plan。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationPlanner.cpp:105` 读取 unique target assets。

下一步空间：

1. 对同一 TaskPlan 内同资产写 step 建依赖 DAG，只允许无依赖、不同资产、无共享输出符号的 step 并发做纯准备。
2. 对同资产 commit 串行，但合并 compile/save/dirty/notify，避免 step 级重复后处理。
3. 对跨 CLI 同资产写入必须通过资产级 gate 排队，不能让多个 CLI 同时改同一资产。
4. 对不同资产写入可以探索批次调度，但最终 UE Editor 的资产提交仍在 GameThread 上逐项应用。

## 10. P2：Review 查询索引化与分页

Review 路径在记录增多后容易被文件枚举、逐文件反序列化、全量查询拖慢。当前已有 pending load coordinator 和 validity sweep coordinator，可以沿同类 service/coordinator 边界继续演进，不应放到 UI widget。

建议：

1. 建轻量索引：`asset -> record ids`、`action_state -> ids`、`timestamp order`、`change_id -> parent/children`。
2. `QueryReviewRecords` 支持分页与过滤条件命中索引，避免 UI 打开时全量加载。
3. validity sweep 保持 GameThread 验证，但候选集构建、去重、排序、分页在 worker 或普通文件 IO 层完成。
4. DebugBundle、Review evidence、UI overlay、AcceptReject 状态必须消费同一 Review 数据模型，不能各自生成不同解释。

相关安全边界：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewTargetValidityResolver.cpp:232` 明确检查 `IsInGameThread()`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.cpp:233` 在 sweep 中调用 `ValidateOnGameThread`。

## 11. P2：JSON/序列化/Hash 重复开销

当前重复开销主要来自：

- Bridge 请求/响应 JSON frame parse/serialize；
- TaskRuntime cache key 生成中的稳定序列化、Clone、Hash；
- ReadContext snapshot -> formatter -> payload -> response 的多级 JSON 对象构建；
- Review evidence / DebugBundle / result store 的重复落盘格式化。

建议：

1. 对稳定序列化结果缓存 `bytes + hash`，key 构造不重复 Clone/Serialize/Deserialize。
2. 对响应使用 lazy serialization，直到 transport write 前再生成最终 JSON 字节。
3. 大 payload 输出支持分页或按需 include 字段，默认 CLI 不输出重 debug/timing 数据。
4. timing 中加入 payload size、serialize ms、parse ms、hash ms、clone count 近似指标。

## 12. P3：多 Editor 分片是唯一可讨论的“突破单 GameThread”方案

单个 Editor 进程内不能安全地让后台线程直接读写 UObject/Blueprint/Graph。若必须提升总吞吐，只能考虑多 Editor 进程：

| 模式 | 可行性 | 风险 |
| --- | --- | --- |
| 多 Editor 只读分片 | 中 | 资产 registry 冷启动、缓存一致性、内存占用 |
| 多 Editor 不同项目/不同 content root 写入 | 中低 | 包保存冲突较低，但部署复杂 |
| 多 Editor 同一项目不同资产写入 | 高风险 | AssetRegistry、source control、package dirty/save、redirector、文件锁 |
| 多 Editor 同一资产写入 | 不建议 | 冲突不可接受 |

如果要做，应作为独立架构项目，先实现 `EditorShardManager`、`AssetShardLock`、`PackageSaveLease`、`ShardHealthProbe`、`ReadOnlyShardPolicy`，而不是在现有 BridgeServer 中临时多开连接。

## 13. 不建议做的事

1. 后台线程直接访问 `UObject`、`UBlueprint`、`UEdGraph`、`UEdGraphPin`、`UWidgetTree`、`FProperty`。
2. 跨请求缓存 UObject 指针、Graph 指针、Pin 指针。
3. 用 UI timer、延迟一帧、retry loop 隐藏性能问题。
4. 让多个 CLI 对同一资产同时写入并假设 UE 会自动解决冲突。
5. 为单个 command 写临时 fast path，绕过统一 route planner / scheduler / service 边界。

## 14. 推荐实施顺序

### 第一批：观测与并发骨架

1. 增加 Bridge queue/in-flight/accept wait/response write/payload size 指标。
2. CLI `BridgeClient` 增加 in-flight 上限和 backpressure。
3. UE Bridge accept 与 connection worker 拆分。
4. Bridge request scheduler 支持按 `request_id` 异步完成。

### 第二批：线程分类与纯计算迁移

1. RoutePlanner 引入命令线程类别，不再让所有 known command 默认 GameThread。
2. TaskSpec compile cache 持久化。
3. TaskRuntime pure prepare、cache key、依赖 DAG 构建前移到 worker。
4. ReadContext snapshot 后格式化/压缩/响应序列化 off-GT。

### 第三批：读写批处理

1. 显式 multi-view ReadContext route。
2. 写链路按资产级 gate、compile/save 去重。
3. Review 索引和分页。
4. GraphWrite/GraphLayout 增量 snapshot 与批量 apply。

### 第四批：独立探索

1. 多 Editor 只读分片 PoC。
2. 多 Editor 不同资产写入 PoC。
3. 与现有 Bridge/TaskRuntime 隔离，先做实验性 CLI，不进入默认路径。

## 15. 建议的基准测试矩阵

| 场景 | 目的 | 指标 |
| --- | --- | --- |
| N 个并发 `bh bridge ping` / 静态 route | 检查 accept/connection 阻塞 | p50/p95、accept wait、response write |
| N 个并发 `ReadContext logic_flow` 同目标 | 检查 coalescing / snapshot 复用 | UE snapshot 次数、bridge wait、wall p95 |
| N 个并发 `ReadContext` 不同目标 | 检查 GT snapshot 窗口 | enqueue wait、snapshot ms、format ms |
| 1 个长写 + 多个短读 | 检查队头阻塞缓解 | 短读 p95 是否被长写拖住 |
| 同资产多 step 写入 | 检查 compile/save 去重 | compile count、save count、post_io ms |
| 不同资产并发写入 | 检查资产 gate 粒度 | throughput、冲突数、失败恢复 |

## 16. 本轮审查状态

已完成：

- 分发 5 个只读 worker；其中 1 个因模型容量失败，已用替代 worker 完成 v0.5.0 归档审查。
- 本地复核 BridgeServer、RoutePlanner、TS BridgeClient、CLI runtime cache、TaskRuntime caches、ReadContext request-local cache、Review GT validity、v0.5.0 性能报告。
- 输出本性能缺口审查文档。

未执行：

- 未运行 UE Editor。
- 未做真实并发 benchmark。
- 未修改代码。
- 未执行自动化测试或 E2E；本轮是只读审查和文档输出，不是功能实现。

