# BlueprintHelper Tool Cluster Onboarding Template

## Scope
新工具接入默认按静态工具簇接入，不做动态注册。一个新工具簇至少提供四个边界：

- `ClusterService`: 工具簇内部真实 UE Editor 资产操作，只在 GameThread 执行。
- `TaskRuntimeCluster`: 识别并执行该簇的 lowered step。
- `BridgeRoutes`: 识别并执行该簇的 Bridge command。
- `ReviewEvidenceBuilder`: 写工具必须产出 `WriteReviewEvidence.v1`；只读工具不产出 ReviewRecord。

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
- `DebugBundle`: 如该簇有诊断输出，只写稳定 ref，不把 Debug 系统并入工具簇。

## Verification Checklist
- Bridge route planner tests: command 只归属一个簇，unknown command 仍 unknown。
- Bridge route tests: `Is<ClusterName>Command` 只识别本簇命令。
- TaskRuntime cluster tests: capability / adapter operation 只由一个簇处理。
- Review regression: 写工具 evidence 仍生成。
- Commands:
  - `git diff --check`
  - `npm.cmd test` in `BlueprintHelper_MCP_Server`
  - UE build and Automation in a writable project `Intermediate` environment.

## Reserved Empty Clusters
`AnimationBlueprint` 与 `Material` 当前只保留空骨架和识别测试。没有真实 command / capability 前，不接入 `BridgeRouter` 或 `TaskRuntimeClusterHub` 的执行链。
