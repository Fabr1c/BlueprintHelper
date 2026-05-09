# BlueprintHelper Tool Cluster Onboarding Template

## Scope
新工具接入默认按静态工具簇接入，不做动态注册。一个新工具簇至少提供四个边界：

- `ClusterService`: 工具簇内部真实 UE Editor 资产操作，只在 GameThread 执行。
- `TaskRuntimeCluster`: 识别并执行该簇的 lowered step。
- `BridgeRoutes`: 识别并执行该簇的 Bridge command。
- `ReviewEvidenceBuilder`: 写工具必须产出 `WriteReviewEvidence.v1`；只读工具不产出 ReviewRecord。
- `DebugEvidenceBoundary`: 工具簇不私自写 Debug JSON；失败、blocked、partial failure、compile/save failure 统一通过 TaskRuntime / DebugEntry 进入 DebugCase，DebugBundle 只导出 summary 或 stable artifact candidate。

## Required Source Shape
- Service: `Source/BlueprintHelper/Public/Systems/ToolClusters/<ClusterName>/...`
- Bridge route header: `Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelper<ClusterName>BridgeRoutes.h`
- Bridge route implementation: `Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelper<ClusterName>BridgeRoutes.cpp`
- TaskRuntime cluster header: `Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/<ClusterName>/BlueprintHelper<ClusterName>TaskRuntimeCluster.h`
- TaskRuntime cluster implementation: 放在独立 cpp；如果必须调用 `BlueprintHelperTaskRuntimeService.cpp` 匿名命名空间 helper，可以临时留在该 cpp 内，并在后续 helper 下沉后迁出。

## Static Wiring Checklist
- `FBlueprintHelperBridgeRoutePlanner::BuildPlan`: 增加该簇 command 表。
- `FBlueprintHelperBridgeRouter`: 增加一个 route 成员，并在 `HandleRequestWithPlan` 中按 `RoutePlan.Cluster` 委托。
- `FBlueprintHelperTaskRuntimeClusterHub`: 增加一个 cluster 成员，并在 `ResolveClusterForLoweredStep` / `ExecuteStep` 中委托。
- `TaskPlanAdapter`: 负责从 TaskPlan IR lower 到该簇 payload，不把 adapter operation 暴露给 Agent-facing TaskSpec。
- `Review`: 所有 asset-mutating write 必须在生产者侧产出完整 `WriteReviewEvidence.v1`，ReviewStore 不猜测 anchor。
- `Debug`: 工具簇只提供脱敏 debug summary candidate，不把 Debug 系统并入工具簇，不写 bundle artifact 内容；只有 failure、blocker、partial、review needs_action 经 DebugEntry 创建 DebugCase，DebugBundle 只在开发者导出时生成。
- `Transaction`: 写工具的 `transaction_id` 必须能链接到 Review evidence 和 DebugCase transaction summary；Transaction Journal 仍是事实来源。

## Verification Checklist
- Bridge route planner tests: command 只归属一个簇，unknown command 仍 unknown。
- Bridge route tests: `Is<ClusterName>Command` 只识别本簇命令。
- TaskRuntime cluster tests: capability / adapter operation 只由一个簇处理。
- Review regression: 写工具 evidence 仍生成。
- Debug regression: 失败结果只暴露 `debug_case_ids[]` summary ref，不暴露 DebugBundle artifact、本地路径、raw payload 或 source content。
- Commands:
  - `git diff --check`
  - `npm.cmd test` in `BlueprintHelper_MCP_Server`
  - UE build and Automation in a writable project `Intermediate` environment.

## Reserved Empty Clusters
`AnimationBlueprint` 与 `Material` 当前只保留空骨架和识别测试。没有真实 command / capability 前，不接入 `BridgeRouter` 或 `TaskRuntimeClusterHub` 的执行链。
