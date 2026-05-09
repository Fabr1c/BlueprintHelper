# BlueprintHelper 工具簇入口重构计划

## Summary
- 将 `TaskRuntimeService` 和 `BridgeRouter` 从“直接接入所有具体工具 Service”改成“静态工具簇入口”。
- 新工具接入默认只新增一个工具簇 Service、一个 TaskRuntime 簇执行入口、一个 Bridge 簇路由入口，再在静态 Hub 中补一行。
- 不做动态注册。所有簇仍由模块启动时显式构造、显式传入、显式排序。
- Bridge 支持工作线程预判 route plan，但所有 UObject、Editor、Transaction、Review、DebugBundle 操作仍只在 GameThread 执行。

## Key Changes
- 新增 TaskRuntime 工具簇接口：
  - `TryLowerStep(TaskPlan, StepObject, bDryRun, OutLoweredStep, OutError)`
  - `CanExecuteStep(LoweredStep)`
  - `ExecuteStep(LoweredStep, Context)`
  - `BuildReviewEvidence(LoweredStep, StepResult, Context, OutEvidence)`
- 现有工具拆为静态簇：
  - `GraphWrite` 包含 append/replace/patch/merge graph。
  - `BlueprintVariables` 包含 member/local variables/defaults。
  - `AssetFactory`、`Component`、`ClassSettings`、`Signature`、`UMGWidget`、`DataTable`、`ObjectProperty`、`CleanupOwnership` 各自独立。
- `FBlueprintHelperTaskRuntimeService` 只持有 `FBlueprintHelperTaskRuntimeClusterHub`，不再直接持有所有具体工具 Service；compile/save post operation 仍属于 Runtime 编排层，不进入工具簇。
- 新增 Bridge route plan：
  - `FBlueprintHelperBridgeRoutePlan { Command, Cluster, bKnownCommand, bRequiresGameThread }`
  - `FBlueprintHelperBridgeRoutePlanner::BuildPlan(Command)` 必须无副作用、线程安全、只读 command。
  - BridgeServer 可在 socket/worker 线程先构建 `RoutePlan`，再把 `Request + RoutePlan` 投递到 GameThread。
- `FBlueprintHelperBridgeRouter` 保留唯一外部入口：
  - 先使用 route plan 定位簇。
  - 再由对应 `*BridgeRoutes` 执行命令。
  - payload validation 和响应格式保持现状。
- 每个工具簇拥有自己的 Bridge 路由类：
  - `GraphWriteBridgeRoutes`
  - `BlueprintVariablesBridgeRoutes`
  - `AssetFactoryBridgeRoutes`
  - 后续新增 `AnimationBlueprintBridgeRoutes`、`MaterialBridgeRoutes`
- Review、Debug、Transactions 不作为工具簇；它们是系统层。工具簇负责产出 Review evidence 和 DebugBundle ref，系统层负责持久化、查询、展示和回滚。

## Implementation Changes
- 第一阶段只做无行为变化拆分：
  - 建立 `TaskRuntimeClusterHub` 和 `BridgeRoutePlanner`。
  - 将当前 `TaskRuntimeService` 中的大型 step dispatch 改为 cluster hub dispatch。
  - 将当前 `BridgeRouter::HandleRequest` 中的 command if-chain 改为 route plan + cluster routes。
- 第二阶段按簇迁移现有逻辑：
  - 先迁 `GraphWrite`，因为它同时影响 Review evidence、TransactionJournal、rollback。
  - 再迁 `BlueprintVariables`、`Component`、`ClassSettings`、`Signature`。
  - 最后迁 `UMGWidget`、`DataTable`、`ObjectProperty`、`CleanupOwnership`。
- 第三阶段建立新工具接入模板：
  - 新增工具簇目录只需要提供 `ClusterService`、`TaskRuntimeCluster`、`BridgeRoutes`、`ReviewEvidenceBuilder`。
  - 静态 Hub 增加一行簇实例引用。
  - `BridgeRoutePlanner` 增加该簇 command 表。
- 第四阶段预留动画蓝图和材质簇：
  - 不实现具体工具逻辑。
  - 只添加空簇骨架和测试，验证新簇接入不需要修改大入口分发逻辑。

## Execution Status
- 2026-05-08：第一阶段已完成。
  - 新增 `FBlueprintHelperBridgeRoutePlanner`，Bridge socket/worker 线程可只读构建 `RoutePlan`。
  - `BridgeServer` 已先解析 request 并构建 `RoutePlan`，再投递 `Request + RoutePlan` 到 GameThread。
  - `BridgeRouter` 保留唯一外部入口，新增 `HandleRequestWithPlan`；已知命令优先走 RoutePlan 簇分发，第一阶段曾保留旧 if-chain 兼容回退，后续收口已移除。
  - 新增 `FBlueprintHelperTaskRuntimeClusterHub`，`TaskRuntimeService` 通过 ClusterHub 执行 lower/execute/review evidence；compile/save post operation 仍留在 Runtime 编排层。
  - 已新增 Automation 测试 `BlueprintHelper.Router.Cluster.*` 和 `BlueprintHelper.TaskRuntime.Cluster.*`。
  - 已修正 MCP regression fixture 路径到迁移后的 `Develop/TestFixtures/MCPRegression`。
- 2026-05-08：第二阶段开始，优先迁移 `GraphWrite`。
  - 目标：抽出 `GraphWriteBridgeRoutes` 与 `GraphWriteTaskRuntimeCluster`，让 GraphWrite 不再停留在 Router/Hub 的内联分支中。
  - 边界：保持对外 command、payload validation、响应 JSON、TaskPlan wire shape 不变。
  - 当前进度：`GraphWriteBridgeRoutes` 与 `GraphWriteTaskRuntimeCluster` 已完成代码迁移；新增 GraphWrite 簇识别 Automation 测试。
  - 验证状态：MCP 测试已通过；UE build 仍被 `G:/UnrealPractise/MrStone/Intermediate/Build/SourceFileCache.bin` 权限问题阻塞，C++ 编译与 Automation 运行待该环境问题解除后确认。
- 2026-05-08：第二阶段继续迁移第二批簇。
  - 已抽出 `BlueprintVariablesBridgeRoutes`、`ComponentBridgeRoutes`、`ClassSettingsBridgeRoutes`，`BridgeRouter` 的 RoutePlan 优先分发会委托到对应簇 routes；当时旧 handler 仅保留为兼容入口并转发，后续收口已删除。
  - 已抽出 `BlueprintVariablesTaskRuntimeCluster`、`ComponentTaskRuntimeCluster`、`ClassSettingsTaskRuntimeCluster`、`SignatureTaskRuntimeCluster`，`TaskRuntimeClusterHub` 的 resolve/execute 会委托到对应簇 cluster。
  - 已新增第二批簇识别 Automation 测试：`BlueprintHelper.Router.Cluster.SecondBatchRoutesRecognizeOnlyOwnedCommands` 与 `BlueprintHelper.TaskRuntime.Cluster.SecondBatchClustersRecognizeOnlyOwnedSteps`。
  - 本轮仍不改变 MCP/Bridge command 名称，不改变 TaskSpec/TaskPlan wire shape。
  - 本轮验证：`git diff --check` 通过；`npm.cmd test` 通过。当前 Codex 会话内 UE build 仍在写入 `G:/UnrealPractise/MrStone/Intermediate/Build/SourceFileCache.bin` 时被权限阻塞，未进入 C++ 编译；用户侧已反馈编译通过，后续仍需在可写 Intermediate 环境下跑 Automation。

  - 2026-05-08：第二阶段 final batch 已完成。
  - 已接入 `UMGWidgetBridgeRoutes`，并新增 `DataTableBridgeRoutes`、`ObjectPropertyBridgeRoutes`、`CleanupOwnershipBridgeRoutes`；`BridgeRouter` 的 RoutePlan 优先分发已覆盖 UMGWidget、DataTable、ObjectProperty、CleanupOwnership；当时旧 handler 仅保留为兼容转发入口，后续收口已删除。
  - 已新增 `UMGWidgetTaskRuntimeCluster`、`DataTableTaskRuntimeCluster`、`ObjectPropertyTaskRuntimeCluster`、`CleanupOwnershipTaskRuntimeCluster`；`TaskRuntimeClusterHub` 不再直接持有 final batch 具体 service 引用，而是通过对应静态簇执行 resolve/execute。
  - 已新增 final batch 簇识别 Automation 测试：`BlueprintHelper.Router.Cluster.FinalBatchRoutesRecognizeOnlyOwnedCommands` 与 `BlueprintHelper.TaskRuntime.Cluster.FinalBatchClustersRecognizeOnlyOwnedSteps`。
  - 本轮验证：`git diff --check` 通过；`npm.cmd test` in `BlueprintHelper_MCP_Server` 通过，Node/Python 共 138/44 项测试通过；UE build 在当前 Codex 沙箱仍被项目级 `G:/UnrealPractise/MrStone/Intermediate/Build/SourceFileCache.bin` 写入权限阻塞，未进入 C++ 编译阶段，需要在可写 Intermediate 的本机/IDE 环境复跑。
- 2026-05-08：第三、四阶段已完成骨架闭环。
  - 新增工具簇接入模板文档：`Develop/Plan/BlueprintHelper_ToolCluster_Onboarding_Template_20260508.md`，固定 Service / BridgeRoutes / TaskRuntimeCluster / ReviewEvidenceBuilder 四个接入边界。
  - 新增保留空簇骨架：`AnimationBlueprintBridgeRoutes`、`MaterialBridgeRoutes`、`AnimationBlueprintTaskRuntimeCluster`、`MaterialTaskRuntimeCluster`。
  - 空簇当前不声明任何 command 或 capability，不接入 `BridgeRouter` / `TaskRuntimeClusterHub` 执行链；后续真实工具落地时再补 `BridgeRoutePlanner` command 表和 Hub 成员。
  - 已新增保留空簇 Automation 识别测试：`BlueprintHelper.Router.Cluster.ReservedRoutesDoNotClaimCommands` 与 `BlueprintHelper.TaskRuntime.Cluster.ReservedClustersDoNotClaimSteps`。
  - 收口验证：`git diff --check` 通过；`npm.cmd test` in `BlueprintHelper_MCP_Server` 通过；UE build 仍在项目级 `Intermediate/Build/SourceFileCache.bin` 写入阶段被当前 Codex 沙箱权限阻塞。
- 2026-05-08：编译闭环补充。
  - 环境权限修复后，UE build 已进入 C++ 编译阶段，并暴露 TaskPlanAdapters 在 unity build 下的匿名 namespace helper 同名问题。
  - 已将 AssetFactory、Component、ClassSettings、Signature、UMGWidget、DataTable、ObjectProperty、CleanupOwnership adapter 内部 helper 改为按文件/工具簇前缀命名，保持错误码、payload、wire shape、执行逻辑不变。
  - 已复跑 TaskPlanAdapters helper 名称扫描，确认当前 adapter 内部 helper 不再出现跨文件同名定义。
  - 当前 Codex 环境下直接启用 UBA 仍会因为 `C:/ProgramData/Epic/UnrealBuildAccelerator/sessions` 写入权限失败；验证使用 `-NoUBA`。
  - UE build 已通过：`F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex -NoUBA -MaxParallelActions=1`。
  - 复跑同一 UE build 命令返回 `Target is up to date` 与 `Result: Succeeded`。
- 2026-05-08：BridgeRouter 工具簇入口命名收口。
  - 已移除 `BridgeRouter::HandleRequestWithPlan` 后半段旧 command if-chain 兼容回退；已知命令必须由 `RoutePlan` 对应簇路径处理。
  - 已删除已迁移工具簇在 `BridgeRouter` 中的旧私有转发 handler，包括 GraphWrite、BlueprintVariables、AssetFactory、Component、ClassSettings、UMGWidget、DataTable、ObjectProperty、CleanupOwnership。
  - 新增 `AssetFactoryBridgeRoutes`，使 `create_asset` 与其它工具簇命令一样通过静态簇 routes 执行。
  - 已将 Routes 与 TaskRuntime 中会进入同一 unity translation unit 的匿名 namespace helper 改为按簇/层前缀命名，避免 `ReadStringField`、`ReadStringArrayField`、`ReadClassDefaultSettings` 等 helper 同名冲突。
  - 当前边界：Debug、Transactions、TaskRuntime、AssetBrowser、SharedServices、BlueprintStructure、EditorCommand 仍是系统层/遗留系统入口，不作为工具簇迁入本轮。
  - 用户侧 UE build 已通过；本地轻量检查 `git diff --check` 通过，且 `BridgeRouter` 中已无已迁移工具簇的私有 `HandleXxx` 转发入口。
  - 本地补充验证：直接编译包含 Router、Routes、TaskRuntime 的 `Module.BlueprintHelper.1.cpp` unity 单元通过，用于确认最后的 helper 前缀收口没有新增 C++ 编译错误。

## Test Plan
- TaskRuntime tests：
  - 每个现有 capability 能被正确 lower 到对应工具簇。
  - 每个 adapter operation 只由一个簇处理。
  - unknown capability 返回现有错误形态。
  - dry-run 行为和 compile/save post operation 保持不变。
- BridgeRouter tests：
  - 每个现有 command 被 `BridgeRoutePlanner` 分类到正确簇。
  - unknown command 返回现有 `UnknownCommand`。
  - route plan 可在 worker thread 构建，不触碰 UObject。
  - GameThread 执行路径响应 JSON 与迁移前一致。
- Review/Debug regression：
  - GraphWrite、Component、Variables、CleanupOwnership 的 Review evidence 仍生成。
  - Debug/diagnostics/runtime profile 命令仍走 Debug 系统，不进入工具簇。
- Verification：
  - `git diff --check`
  - `npm.cmd test` in `BlueprintHelper_MCP_Server`
  - UE build: `F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex`
  - 当前 Codex 环境验证用 UE build: `F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex -NoUBA -MaxParallelActions=1`
  - UE Automation: existing `BlueprintHelper.*` plus new `BlueprintHelper.Router.Cluster` and `BlueprintHelper.TaskRuntime.Cluster`

## Assumptions
- 先提交当前物理路径迁移基线，再执行本重构，避免 rename 和逻辑拆分混在同一个 diff。
- 不改 MCP/Bridge 对外 command 名称，不改 TaskSpec/TaskPlan wire shape。
- 不引入运行时动态注册、插件扫描、反射式注册。
- 多线程只用于 route plan 预判；所有真实 UE Editor 操作继续 GameThread。
- 本计划只重构入口和接入边界，不顺手补完 Review rollback 或新增动画蓝图/材质工具能力。
