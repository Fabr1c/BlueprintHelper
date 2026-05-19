# BlueprintHelper 四层架构内聚/耦合/复用性扫描报告

日期：2026-05-16

## 1. 扫描范围

本仓库未发现代码或文档中存在固定命名为“四层架构”的单独模块。本文按现有权威架构文档中的“四层协作模式”审查：

```text
Agent-facing TaskSpec
-> MCP/Python Task Compiler / task-core
-> UE Plugin Task Runtime
-> UE Capability Clusters
```

依据：

- `BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md` 明确写出四层协作模式：Agent 提交 TaskSpec，MCP/Python 负责编译和上下文，UE Task Runtime 执行 TaskPlan，工具簇降级为内部能力。
- `README.md` 当前主链路为 Agent -> CLI -> task-core -> compiler/read router -> Bridge -> UE Task Runtime -> capability clusters。
- GraphStatement 设计文档虽然是更细的 8 段链路，但它服务于四层中的 capability/graph mutation 层，因此也纳入本次扫描。

扫描覆盖：

- AgentFace/CLI/MCP：`AgentFaceService/cli`、`AgentFaceService/task-core/src/tool-surface`、`AgentFaceService/mcp`
- TaskSpec/TaskPlan 编译：`AgentFaceService/task-core/src/task`、`AgentFaceService/task-core/python/blueprinthelper_task`
- Bridge/TaskRuntime：`BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge`、`BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime`
- UE capability clusters / GraphStatement：`BlueprintHelper/Source BlueprintHelper/Public|Private/Systems`、`Shared`、`Runtime/TaskRuntime/TaskPlanAdapters`

本报告是静态结构扫描，没有运行编译、Automation 或真实编辑器验证。

## 2. 总体结论

当前实现“方向正确，但实现形态还没有完全达到高内聚、低耦合、强复用”。

结论等级：部分符合，建议评为 B-。

已经做得较好的部分：

1. Agent-facing 边界收敛明显：普通读写走 CLI/TaskSpec，MCP 被硬限制为 editor lifecycle。
2. TaskSpec -> TaskPlan -> TaskRuntime -> capability clusters 的职责方向基本成立。
3. C++ Route 文件、TaskPlanAdapter 文件、GraphStatement 的 IR/Registry/DAG/Composer 文件已经有模块化拆分。
4. GraphWrite 已禁止 legacy nodes/links 主路径，明确要求 `logic_spec/SemanticIR`，有利于复用语义层能力。

主要不足：

1. `BlueprintHelperTaskRuntimeService.cpp` 是当前最大架构债务，超过 5500 行，包含运行时编排、变量 adapter、cluster hub、review evidence、journal、compile/save 后置操作等多类职责。
2. `BlueprintHelperBridgeRouter.cpp` 和 `BlueprintHelperBridgeRoutePlanner.cpp` 仍是大中心路由器，每增加能力都需要修改中心映射。
3. Python 编译器和 TypeScript fallback 编译器存在大量重复 TaskSpec -> TaskPlan lowering 逻辑，存在语义漂移风险。
4. `Shared` 层反向 include `Systems`，`Systems` 又 include `Entry/BlueprintHelper.h`，严格分层已经被打穿。
5. GraphStatement 设计要求 NodeFragment 不直接写图，但实际 `FBlueprintHelperNodeFragment` 保存 `UEdGraphPin*` / `UK2Node*`，Builder 直接 spawn 节点，Composer 直接连接 pin，复用性受限。

## 3. 分层评价

| 层 | 内聚性 | 耦合度 | 复用性 | 结论 |
|---|---|---|---|---|
| Agent-facing / CLI / MCP | 高 | 中低 | 中高 | 主入口边界清晰，MCP lifecycle-only 硬门有效；遗留 MCP 注册文件仍很臃肿。 |
| task-core / Python Compiler | 中 | 中高 | 中 | 职责明确，但 TS/Python 双实现重复降低维护复用。 |
| Bridge / UE Task Runtime | 中低 | 高 | 中低 | 架构方向对，但中心文件和中心路由器承担过多职责。 |
| UE Capability Clusters / GraphStatement | 中 | 中 | 中 | 工具簇和 GraphStatement 文件拆分较好，但 GraphStatement 抽象层泄漏 UE 指针和落图动作。 |

## 4. 关键证据

### 4.1 Agent-facing 边界较清晰

证据：

- `README.md:36-37`：CLI-first、TaskSpec-first 是当前主入口。
- `README.md:48-54`：主链路经过 CLI、task-core、compiler/read router、Bridge、UE Task Runtime、capability clusters。
- `README.md:60`：Editor lifecycle 走全局 BlueprintHelper MCP。
- `AgentFaceService/mcp/src/mcp/tools/register-tools.ts:720-722`：先注册 lifecycle tools，随后 `isEditorLifecycleOnlyMcpSurface()` 为 true 时直接返回。
- `AgentFaceService/mcp/src/mcp/tools/register-tools.ts:2891-2892`：`isEditorLifecycleOnlyMcpSurface()` 固定返回 true。
- `AgentFaceService/mcp/src/mcp/tools/editor-lifecycle-tools.ts:31`：工具描述明确普通读写必须使用 CLI，不走 MCP。

判断：

- 这部分满足低耦合目标。普通 Agent 不直接面对几十个 UE 原子命令，避免了能力暴露面膨胀。
- 但 `AgentFaceService/mcp/src/mcp/tools/register-tools.ts` 仍保留约 2800+ 行 legacy 注册代码，虽然运行时被硬门截断，但维护者仍需要理解一个大量死路径/冻结路径混合文件。

### 4.2 TaskSpec/TaskPlan 编译层职责明确，但双实现重复

证据：

- `AgentFaceService/task-core/src/task/compiler/task-python-orchestrator.ts:44`：task-core 默认可调用 Python compiler。
- `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py:151`：Python compiler 以 `compile_task_spec` 分发 task_type。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:29-45`：TypeScript fallback compiler 也按 task_type 分发。
- `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py:175` 与 `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:75`：两边都实现 `create_blueprint_feature` 编译。
- `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py:994` 与 `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:916-969`：两边都实现 TaskPlan 到 append bridge payload 的 lowering。

判断：

- 编译层职责是高内聚的：它确实围绕 TaskSpec 校验、语义转换、TaskPlan 输出工作。
- 但复用性不足：Python 与 TS 重复实现同类 strategy、op、payload lowering。即使有测试，也会增加字段新增时的双改成本和漂移风险。

### 4.3 Bridge 层已经拆出 Routes，但中心路由仍偏重

证据：

- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/*BridgeRoutes.cpp` 已按 capability 拆出路由文件。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.cpp:20-184`：中心 planner 以大量 command string 判断 cluster。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:3-58`：router 直接 include Debug、Shared、GraphWrite、Runtime、Review、Transactions 等多类服务。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:609-760`：HandleRequest 仍在中心 router 中分发大量 cluster。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:1600-1645`：TaskRuntime preview/execute/journal 也由 router 直接调 runtime service。

判断：

- Route 文件提升了局部内聚，但中心 Router/Planner 仍是高耦合修改点。
- 当前模式适合小中规模工具面；当 capability 继续增加时，建议转为 route registry 或自注册表，减少中心文件改动。

### 4.4 TaskRuntime 是当前最大低内聚风险

证据：

- `BlueprintHelperTaskRuntimeService.cpp` 约 5539 行，是当前扫描中最大的核心实现文件。
- `BlueprintHelperTaskRuntimeService.cpp:5-37`：runtime service include 大量 capability service、adapter、review/debug/post-operation 相关依赖。
- `BlueprintHelperTaskRuntimeService.cpp:51-67`：BlueprintVariable TaskPlanAdapter 常量定义在 TaskRuntimeService.cpp 中。
- `BlueprintHelperTaskRuntimeService.cpp:69` 与 `:307`：BlueprintVariable adapter helper 和 `TryBuildPayloadFromTaskPlanStep` 也实现于 TaskRuntimeService.cpp，而其他 adapter 多数有独立 `.cpp`。
- `BlueprintHelperTaskRuntimeService.cpp:4072-4394`：除 GraphWrite 外，多数 TaskRuntimeCluster 方法实现也集中在 TaskRuntimeService.cpp。
- `BlueprintHelperTaskRuntimeService.cpp:4453-4492`：ClusterHub 用固定 if 链解析 cluster。
- `BlueprintHelperTaskRuntimeService.cpp:4723-4866`：TryLowerTaskPlanStep 中集中处理多个 capability adapter。
- `BlueprintHelperTaskRuntimeService.cpp:4998-5031` 与 `:5446-5447`：preview/execute/run、compile/save policy、step dependency 和 post operation 也集中在同一文件。

判断：

- 职责边界在概念上成立，但实现上 TaskRuntimeService 同时承担 orchestration、lowering、cluster dispatch、review evidence、journal、post compile/save、部分 adapter 和部分 cluster 实现。
- 这不符合高内聚。它会导致新增 capability 或调整 review/journal 行为时高概率触碰同一大文件，复用性和变更局部性都偏弱。

### 4.5 Header 依赖有循环/反向耦合迹象

证据：

- `BlueprintHelperTaskRuntimeClusterHub.h:6-16` include 所有 cluster header，并 include `BlueprintHelperTaskRuntimeService.h`。
- 各 cluster header 又需要 `FBlueprintHelperTaskRuntimeLoweredStep`，因此形成 cluster hub、cluster、service types 之间的高耦合。
- `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Services/*.cpp` 多处 include `Systems/...`，例如 `BlueprintHelperAgentImportSemanticExecutor.cpp:4-7`、`BlueprintHelperImportService.cpp:4-7`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/...` 又多处 include `Entry/BlueprintHelper.h`，例如 `BlueprintHelperComponentService.cpp:6`、`BlueprintHelperRuntimeProfileService.cpp:4`、`BlueprintHelperLogicJsonReadService.cpp:11`。

判断：

- 如果把 `Shared` 视为公共底层，`Shared -> Systems` 是反向依赖。
- 如果把 `Entry` 视为入口层，`Systems -> Entry` 也是反向依赖，实际变成了 module singleton/service locator 依赖。
- 这些依赖不会马上破坏功能，但会降低低耦合目标，尤其会让复用 Shared 服务或单测 Systems 服务变困难。

### 4.6 GraphStatement 抽象方向正确，但 NodeFragment/Composer 已经混入 UE 落图细节

设计依据：

- `BlueprintHelper_GraphStatementFramework_Design_20260513_CN.md:54-61`：设计明确 AgentFace、BlueprintLogicSpec、SemanticIR、PatternRegistry、NodeFragment、GraphComposer、StrategyAdapter、UE Graph Mutator 的职责。
- 其中 `:58-59` 明确 NodeFragment/Graph Composer 不应直接插入目标图或直接调用 UE mutation。
- `:598` 明确 UE Graph Mutator 是唯一直接改图层。

实现证据：

- `BlueprintHelperGraphStatementBuilder.h:4` 直接 include `BlueprintGraphWriteFacade`。
- `BlueprintHelperGraphStatementBuilder.h:17`、`:30`、`:33-34`：NodeFragment 结构保存 `UEdGraphPin*`、`UK2Node*`、exec pin 指针。
- `BlueprintHelperGraphStatementBuilder.cpp:583`、`:633`：Builder 直接构建 call/set fragment。
- `BlueprintHelperGraphStatementBuilder.cpp:611`、`:655`：Builder 通过 resolver/facade spawn UE 节点。
- `BlueprintGraphGenerationPipeline.cpp:790-831`：pipeline 解析 SemanticIR、构建 FragmentDag 后直接开启 `FScopedTransaction` 并进入生成。
- `BlueprintGraphGenerationPipeline.cpp:876-888`：pipeline 构建 statement 并调用 linker/composer 连接 data edges。
- `BlueprintGraphGenerationPipeline.cpp:802`、`:1009`、`:1032`：主路径已禁止 legacy nodes/links，要求 `logic_spec/SemanticIR`。

判断：

- GraphStatement 的“语义输入”和“禁止 legacy nodes/links”方向很好，避免 AgentFace 泄漏 pin/node 细节。
- 但 NodeFragment 当前不是纯中间表示，而是“已绑定 UE 节点/Pin 的片段”。这让 Graph Composer、Builder、Mutator 的边界变薄，复用到 dry-run、preview、非 UE 图模型、布局求解、单元测试时都会受限。

## 5. 复用性评价

复用性强的部分：

- TaskSpec/TaskPlan 是跨 CLI、MCP adapter、Bridge、UE Runtime 的统一协议。
- task-core tool registry 和 `createTaskSpecRunner` 允许 CLI/MCP 共享默认任务工具。
- GraphWrite 的 SemanticIR、FragmentDag、FragmentEvidence 已经能作为 review/debug/evidence 的通用材料。
- 多数 TaskPlanAdapter 已经独立文件化，后续扩展 capability 有可复制模板。

复用性弱的部分：

- Python/TS 编译器重复，不能单一复用。
- Runtime lowering、cluster resolution、review evidence 和 post-op 执行没有注册表化，新增能力复用路径仍依赖改大中心文件。
- GraphStatement 的 NodeFragment 持有 UE 指针，无法作为纯数据 contract 被 compiler、review、layout、dry-run、测试复用。
- Shared 服务和 Systems 服务互相 include，复用边界不稳定。

## 6. 建议

### P0：先拆最大耦合点

1. 拆分 `BlueprintHelperTaskRuntimeService.cpp`
   - `TaskRuntimeOrchestrator.cpp`：只保留 preview/execute/run、dependency scheduling。
   - `TaskRuntimeLoweringRegistry.cpp`：按 capability 注册 adapter，替代 `TryLowerTaskPlanStep` 的大 if 链。
   - `TaskRuntimeClusterHub.cpp`：保留 cluster dispatch，但 cluster 实现移回各自 `.cpp`。
   - `TaskRuntimeJournalBuilder.cpp` / `TaskRuntimeReviewEvidenceBuilder.cpp`：从 runtime 主流程中分离 journal/review 构建。
   - `BlueprintVariableTaskPlanAdapter.cpp`：补齐缺失的独立 adapter 实现文件。

2. 抽出 runtime 公共类型
   - 新建 `BlueprintHelperTaskRuntimeTypes.h`，放 `FBlueprintHelperTaskRuntimeLoweredStep`、step record、post operation record。
   - Cluster header 不再 include `BlueprintHelperTaskRuntimeService.h`。

3. 收敛 Bridge Router/Planner
   - 把 command -> cluster 映射改成静态表或 route registry。
   - 各 `*BridgeRoutes` 暴露 `Commands()` 或 `RegisterRoutes()`，中心 router 不再枚举所有 command string。

4. 建立 include 分层约束
   - `Shared` 禁止 include `Systems`。
   - `Systems` 禁止 include `Entry/BlueprintHelper.h`，改用构造注入或小接口。
   - `Entry/Bridge` 类型如 `FBlueprintHelperBridgeResponse` 可下沉到 `Shared/Protocol`，避免 Systems 直接依赖 Entry。

### P1：提升编译层和 GraphStatement 复用

1. 统一 TaskSpec 编译来源
   - 优先让 Python compiler 成为唯一生产 compiler。
   - TS fallback 如果保留，只保留 schema/summary/测试辅助，不再独立维护完整 lowering。
   - 或把 strategy/op lowering 描述成 declarative capability descriptor，由 Python/TS 共同消费。

2. 拆分 GraphStatement 的纯 IR 与 UE 绑定层
   - `NodeFragment` 保持纯数据：fragment id、ports、edge、metadata、layout hints。
   - 新增 `BoundNodeFragment` 或 `SpawnedNodeFragment` 保存 `UK2Node*` / `UEdGraphPin*`。
   - GraphComposer 对纯 fragment 输出 mutation plan，UE Graph Mutator 负责实际 spawn/connect。
   - PatternRegistry 进一步承接 pattern -> fragment template，不让 Builder 继续内聚过多 node-handler 细节。

3. 清理 MCP legacy 注册文件
   - 既然 `isEditorLifecycleOnlyMcpSurface()` 固定 true，应将后续 legacy tool 注册移动到 archived/compat 文件，或构建期排除。
   - 保留 `editor-lifecycle-tools.ts` 与最小 adapter 即可。

### P2：增加架构守护测试

1. 加 include-boundary 脚本：
   - fail on `Shared/** #include "Systems/"`
   - fail on `Systems/** #include "Entry/"`
   - fail on `Runtime/TaskRuntime/Clusters/** #include "BlueprintHelperTaskRuntimeService.h"`

2. 加 capability registration 合同测试：
   - 每个 TaskPlan adapter 必须有独立 `.cpp` 或明确豁免。
   - 每个 route command 必须由 route registry 声明一次，避免 BridgeRoutePlanner 和 handler 双写。

3. 如果保留 TS/Python 双 compiler：
   - 对同一 fixture 输出 TaskPlan 做 golden parity 测试。
   - 新增 task_type 时测试必须覆盖两边或显式声明 TS 不支持。

## 7. 最终判断

当前四层架构满足“方向上的低耦合”：Agent 不直接调用底层 UE 原子工具，TaskSpec/TaskPlan 边界清楚，MCP 生命周期边界也已经硬化。

但当前实现还未完全满足“实现上的高内聚、低耦合、强复用”：

- 高内聚不足集中在 TaskRuntimeService、BridgeRouter、双 compiler、GraphStatement Builder/Composer。
- 低耦合不足集中在 Shared/System/Entry 反向 include 和 runtime cluster/header 相互依赖。
- 强复用不足集中在 Python/TS lowering 重复和 NodeFragment 持有 UE 指针。

建议优先拆 `BlueprintHelperTaskRuntimeService.cpp` 与 runtime 公共类型。这是收益最高的第一步，因为它会同时改善内聚、降低 cluster/header 依赖，并为 capability 扩展建立更可复用的注册式路径。

