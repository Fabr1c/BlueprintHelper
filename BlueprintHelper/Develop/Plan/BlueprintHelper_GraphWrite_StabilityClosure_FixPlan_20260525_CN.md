# GraphWrite Stability Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 GraphWrite 进入稳定期前剩余的能力契约、Review evidence 证据链、legacy parsed-plan 残留、asset_action 真实 ActionDatabase 投影和最终 smoke 验收缺口。

**Architecture:** `GraphWriteCapabilityContract` 作为全局能力口径，不从散落测试或文档推导支持范围。Review evidence 走 producer-owned evidence -> post-IO -> ReviewStore -> DebugBundle/UI consumer 的同一证据链，禁止 runtime fallback 和伪成功。`asset_action` 只接受 ActionDatabase 投影证据，并在执行阶段复验同一 stable id，避免缓存复用把旧 spawner 当作当前资产上下文的真实能力。

**Tech Stack:** UE 5.6 C++ automation tests, BlueprintHelper GraphWrite ActionResolution/TaskRuntime/ReviewStore, AgentFaceService TypeScript TaskSpec schema, PowerShell verification commands.

---

## Scope

- 本计划修复刚刚讨论的 5 个稳定性闭环问题：
  - 全局 `GraphWriteCapabilityContract`。
  - Review evidence 上游到下游证据链可靠性。
  - legacy parsed-plan / `FParsedNode` / `parsed_node_plan_unsupported` 全量移除。
  - `asset_action` 的投影、执行复验、Review/debug evidence。
  - GraphWrite 稳定后运行四簇统一 generality preflight/smoke。
- 本计划不重新打开已经完成的 Merge/Patch ownership、Merge callable convergence、Event taxonomy 拆分结论。
- 执行本计划时不执行 `git add`、`git commit`、`git push`；每个任务结束只记录建议提交范围。

## File Structure

- Create: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - 唯一导出机器可读的 GraphWrite 能力契约、cluster/kind/operation/evidence/review gate。
- Create: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - 验证契约覆盖四簇、禁止 unsupported operation 被标记为 supported、约束 `asset_action` evidence gate。
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - 对外导出 `GRAPHWRITE_CAPABILITY_CONTRACT`，让 CLI、测试和计划生成使用同一口径。
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
  - 从说明文档改为 contract rollout 文档，记录 TypeScript contract 是 source of truth。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h`
  - 定义 `FBlueprintHelperAssetActionProjectionRequest`、`FBlueprintHelperAssetActionProjectedCandidate`、`FBlueprintHelperAssetActionProjectionResult`。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp`
  - 封装 ActionDatabase refresh、filter、stable id 计算、ambiguity/not-found 诊断。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
  - 从私有扫描逻辑切换为 projection service，并在 execute-time 复验 projected evidence。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
  - 增加 asset action evidence writer，保证 projection 和 resolver 使用同一 key 集合。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
  - 补齐 operation/asset/graph/action evidence metadata，失败、缺 asset、缺 graph 继续返回 `false`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.cpp`
  - 保持 producer evidence 进入 ReviewStore 的唯一链路，补诊断字段，不新增 fallback。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
  - 加强 producer-owned GraphWrite review evidence 单元测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`
  - 增加 evidence batch -> post-IO -> review record 字段完整性测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
  - 删除当前允许 `parsed_node_plan_unsupported` 的豁免，增加 private GraphWrite pipeline residue 扫描。
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanBuilderTests.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanExecutorTests.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanTests.cpp`
  - 删除只验证 legacy mutation-plan DTO 的测试。
- Delete after callers are removed:
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.cpp`

## Task 1: Add Global GraphWriteCapabilityContract

**Files:**
- Create: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Create: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`

- [ ] **Step 1: Write the failing contract test**

Add this test file:

```ts
import { strict as assert } from "node:assert";
import { GRAPHWRITE_CAPABILITY_CONTRACT } from "./graphwrite-capability-contract.js";

const clusters = new Map(
  GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => [cluster.id, cluster]),
);

assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.version, 1);
assert.equal(GRAPHWRITE_CAPABILITY_CONTRACT.status, "stable-candidate");

for (const id of ["function_action", "field", "event", "asset_action"]) {
  assert.ok(clusters.has(id), `missing GraphWrite cluster contract: ${id}`);
}

const assetAction = clusters.get("asset_action");
assert.ok(assetAction);
assert.ok(assetAction.operations.some((operation) => operation.id === "create.asset_action"));
assert.deepEqual(
  assetAction.evidence.requiredKeys,
  [
    "asset_action_stable_id",
    "asset_action_node_class",
    "asset_action_spawner_signature",
    "asset_action_owner_path",
  ],
);
assert.equal(assetAction.evidence.projectionSource, "UE ActionDatabase");
assert.equal(assetAction.executeRevalidation, "required");

for (const cluster of GRAPHWRITE_CAPABILITY_CONTRACT.clusters) {
  for (const operation of cluster.operations) {
    assert.notEqual(operation.supportStatus, "unknown", `${cluster.id}.${operation.id} has unknown support`);
    assert.ok(operation.reviewEvidence, `${cluster.id}.${operation.id} lacks review evidence policy`);
  }
}
```

- [ ] **Step 2: Run the test and verify it fails before implementation**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: build or node test fails because `graphwrite-capability-contract.ts` is not exported or does not exist.

- [ ] **Step 3: Add the contract module**

Create `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`:

```ts
export type GraphWriteSupportStatus = "supported" | "discussion-gated" | "retired";

export interface GraphWriteOperationContract {
  readonly id: string;
  readonly kind: string;
  readonly supportStatus: GraphWriteSupportStatus;
  readonly reviewEvidence: "graph_surface_atomic_target";
  readonly requiredEvidenceKeys?: readonly string[];
}

export interface GraphWriteClusterContract {
  readonly id: "function_action" | "field" | "event" | "asset_action";
  readonly responsibility: string;
  readonly operations: readonly GraphWriteOperationContract[];
  readonly evidence: {
    readonly projectionSource: "SemanticStatement" | "ActionContext" | "UE ActionDatabase";
    readonly requiredKeys: readonly string[];
  };
  readonly executeRevalidation: "required" | "not-required";
}

export interface GraphWriteCapabilityContract {
  readonly version: 1;
  readonly status: "stable-candidate";
  readonly clusters: readonly GraphWriteClusterContract[];
  readonly finalAcceptance: {
    readonly generalityPreflightAfterStable: true;
    readonly smokePlan: "BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md";
  };
}

export const GRAPHWRITE_CAPABILITY_CONTRACT: GraphWriteCapabilityContract = {
  version: 1,
  status: "stable-candidate",
  clusters: [
    {
      id: "function_action",
      responsibility: "GraphWrite owns function-like statements that resolve through ActionContext and shared action adapters.",
      operations: [
        { id: "call_function", kind: "function", supportStatus: "supported", reviewEvidence: "graph_surface_atomic_target" },
        { id: "macro_like", kind: "function", supportStatus: "supported", reviewEvidence: "graph_surface_atomic_target" },
      ],
      evidence: { projectionSource: "ActionContext", requiredKeys: [] },
      executeRevalidation: "not-required",
    },
    {
      id: "field",
      responsibility: "GraphWrite owns property path, linked typed pin, component_ref and field_access statements.",
      operations: [
        { id: "field_access", kind: "field", supportStatus: "supported", reviewEvidence: "graph_surface_atomic_target" },
        { id: "component_ref", kind: "field", supportStatus: "supported", reviewEvidence: "graph_surface_atomic_target" },
      ],
      evidence: { projectionSource: "SemanticStatement", requiredKeys: [] },
      executeRevalidation: "not-required",
    },
    {
      id: "event",
      responsibility: "GraphWrite owns only custom event statement creation/reference; override/native and delegate-bound events stay with their dedicated tools.",
      operations: [
        { id: "custom_event", kind: "event", supportStatus: "supported", reviewEvidence: "graph_surface_atomic_target" },
        { id: "override_native_event", kind: "event", supportStatus: "discussion-gated", reviewEvidence: "graph_surface_atomic_target" },
        { id: "delegate_component_bound_event", kind: "event", supportStatus: "discussion-gated", reviewEvidence: "graph_surface_atomic_target" },
      ],
      evidence: { projectionSource: "SemanticStatement", requiredKeys: [] },
      executeRevalidation: "not-required",
    },
    {
      id: "asset_action",
      responsibility: "GraphWrite may execute ActionDatabase-backed asset action spawners only when projected evidence selects exactly one current spawner.",
      operations: [
        {
          id: "create.asset_action",
          kind: "create",
          supportStatus: "supported",
          reviewEvidence: "graph_surface_atomic_target",
          requiredEvidenceKeys: [
            "asset_action_stable_id",
            "asset_action_node_class",
            "asset_action_spawner_signature",
            "asset_action_owner_path",
          ],
        },
      ],
      evidence: {
        projectionSource: "UE ActionDatabase",
        requiredKeys: [
          "asset_action_stable_id",
          "asset_action_node_class",
          "asset_action_spawner_signature",
          "asset_action_owner_path",
        ],
      },
      executeRevalidation: "required",
    },
  ],
  finalAcceptance: {
    generalityPreflightAfterStable: true,
    smokePlan: "BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md",
  },
};
```

- [ ] **Step 4: Export the contract from the schema barrel**

Add this line to `AgentFaceService/task-core/src/task/schema/task-schemas.ts`:

```ts
export { GRAPHWRITE_CAPABILITY_CONTRACT } from "./graphwrite-capability-contract.js";
export type {
  GraphWriteCapabilityContract,
  GraphWriteClusterContract,
  GraphWriteOperationContract,
  GraphWriteSupportStatus,
} from "./graphwrite-capability-contract.js";
```

- [ ] **Step 5: Run the TaskSpec tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: PASS. If the existing test runner executes all built `.test.js` files, confirm the new contract test appears in the output or fails when intentionally broken.

- [ ] **Step 6: Update the contract rollout document**

Append a `2026-05-25 Implementation Binding` section to `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md` with these bullets:

```markdown
## 2026-05-25 Implementation Binding

- Source of truth: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`.
- Public export: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`.
- Contract test: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`.
- Stability rule: no GraphWrite operation can be called stable unless the contract marks it `supported`, has a review evidence policy, and is covered by either direct automation or the final generality preflight.
```

## Task 2: Complete Review Evidence Reliability Chain

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`

- [ ] **Step 1: Add failing producer evidence assertions**

Extend `BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence` in `BlueprintHelperTaskRuntimeClusterHubTests.cpp` with these checks against the GraphWrite case:

```cpp
TestEqual(TEXT("graph write evidence count"), Evidences.Num(), 1);
TestEqual(TEXT("graph write evidence asset path"), Evidences[0].AssetPath, FString(TEXT("/Game/Test/BP_GraphWrite")));
TestEqual(TEXT("graph write evidence operation"), Evidences[0].OperationKind, FString(TEXT("append_blueprint_graph")));
TestEqual(TEXT("graph write evidence step index"), Evidences[0].TaskStepIndex, 7);
TestEqual(TEXT("graph write target count"), Evidences[0].AtomicTargets.Num(), 1);
TestEqual(TEXT("graph write target kind"), Evidences[0].AtomicTargets[0].TargetKind, FString(TEXT("graph_block")));
TestEqual(TEXT("graph write target key"), Evidences[0].AtomicTargets[0].TargetKey, FString(TEXT("graph_block:Execute")));
TestEqual(TEXT("graph write target graph"), Evidences[0].AtomicTargets[0].GraphName, FString(TEXT("Execute")));
TestEqual(TEXT("graph write target operation"), Evidences[0].AtomicTargets[0].OperationKind, FString(TEXT("append_blueprint_graph")));
TestEqual(TEXT("graph write target step index"), Evidences[0].AtomicTargets[0].TaskStepIndex, 7);
```

- [ ] **Step 2: Add failing negative producer tests**

Add three cases in `BlueprintHelperTaskRuntimeClusterHubTests.cpp`:

```cpp
TestFalse(TEXT("failed graph write step emits no review evidence"),
  FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(FailedStep, FailedResult, FailedEvidences));

TestFalse(TEXT("graph write step without asset emits no review evidence"),
  FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(NoAssetStep, SuccessResult, NoAssetEvidences));

TestFalse(TEXT("graph write step without graph emits no review evidence"),
  FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(NoGraphStep, SuccessResult, NoGraphEvidences));
```

Construct `FailedStep`, `NoAssetStep`, and `NoGraphStep` from the existing local test helper so only one field changes per case.

- [ ] **Step 3: Add post-IO to ReviewStore chain test**

Add a test in `BlueprintHelperTaskRuntimePostIoBatchTests.cpp` named `BlueprintHelper.TaskRuntime.PostIO.GraphWriteReviewEvidenceBuildsReviewRecords`:

```cpp
FBlueprintHelperWriteReviewEvidence Evidence;
Evidence.EvidenceId = TEXT("task_step_run_1_7");
Evidence.AssetPath = TEXT("/Game/Test/BP_GraphWrite");
Evidence.OperationKind = TEXT("append_blueprint_graph");
Evidence.TaskStepIndex = 7;

FBlueprintHelperWriteAtomicTarget Target;
Target.TargetKind = TEXT("graph_block");
Target.TargetKey = TEXT("graph_block:Execute");
Target.GraphName = TEXT("Execute");
Target.OperationKind = TEXT("append_blueprint_graph");
Target.TaskStepIndex = 7;
Target.AtomicIndex = 0;
Evidence.AtomicTargets.Add(Target);

FBlueprintHelperReviewStoreService ReviewStore;
const TArray<FBlueprintHelperReviewRecord> Records =
  ReviewStore.BuildReviewRecordsFromEvidence({ Evidence });

TestEqual(TEXT("records count"), Records.Num(), 1);
TestEqual(TEXT("record asset"), Records[0].AssetPath, FString(TEXT("/Game/Test/BP_GraphWrite")));
TestEqual(TEXT("record graph"), Records[0].GraphName, FString(TEXT("Execute")));
TestEqual(TEXT("record target key"), Records[0].TargetKey, FString(TEXT("graph_block:Execute")));
TestEqual(TEXT("record operation"), Records[0].OperationKind, FString(TEXT("append_blueprint_graph")));
TestEqual(TEXT("record task step"), Records[0].TaskStepIndex, 7);
```

- [ ] **Step 4: Run the failing review tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.Cluster;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.PostIO;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected before implementation: at least one new assertion fails or a compile error reveals the missing ReviewStore include/type.

- [ ] **Step 5: Implement the minimal evidence chain fix**

In `FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence`, keep the existing guard semantics and ensure the produced evidence always has:

```cpp
Evidence.AssetPath = AssetPath;
Evidence.OperationKind = LoweredStep.AdapterOperation;
Evidence.TaskStepIndex = LoweredStep.StepIndex;

Target.TargetKind = TEXT("graph_block");
Target.TargetKey = FString::Printf(TEXT("graph_block:%s"), *GraphName);
Target.GraphName = GraphName;
Target.OperationKind = LoweredStep.AdapterOperation;
Target.TaskStepIndex = LoweredStep.StepIndex;
Target.AtomicIndex = 0;
```

In `BlueprintHelperTaskRuntimePostIoService.cpp`, do not add a runtime fallback. Only add diagnostics when `Batch.ReviewEvidences` is non-empty and `BuildReviewRecordsFromEvidence` returns no records:

```cpp
if (Batch.ReviewEvidences.Num() > 0 && ReviewRecords.Num() == 0)
{
  Result.Diagnostics.Add(TEXT("review_evidence_produced_no_review_records"));
}
```

- [ ] **Step 6: Re-run the review tests**

Run the same build and automation command from Step 4.

Expected: PASS for `BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence`, `BlueprintHelper.TaskRuntime.Cluster`, and `BlueprintHelper.TaskRuntime.PostIO`.

## Task 3: Remove Legacy Parsed-Plan Residue

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanBuilderTests.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanExecutorTests.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanTests.cpp`
- Delete after references are removed:
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.cpp`

- [ ] **Step 1: Tighten the legacy-mainline contract test**

In `BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`, remove the current allow-list that permits `parsed_node_plan_unsupported` in `BlueprintGraphMutationPlanExecutor.cpp`.

Add this forbidden-token scan:

```cpp
const TArray<FString> ForbiddenGraphWritePrivateTokens = {
  TEXT("parsed_node_plan_unsupported"),
  TEXT("FBlueprintGraphMutationPlan"),
  TEXT("FBlueprintGraphMutationNodePlan"),
  TEXT("FBlueprintGraphMutationLinkPlan"),
  TEXT("MakeNodePlanFromParsedNode"),
  TEXT("MakeLinkPlanFromParsedLink")
};

for (const FString& RelativePath : EnumerateGraphWritePrivateSourceFiles())
{
  FString Source;
  if (!FFileHelper::LoadFileToString(Source, *ToAbsoluteSourcePath(RelativePath)))
  {
    AddError(FString::Printf(TEXT("Could not read GraphWrite source: %s"), *RelativePath));
    continue;
  }

  for (const FString& Token : ForbiddenGraphWritePrivateTokens)
  {
    if (Source.Contains(Token))
    {
      AddError(FString::Printf(TEXT("legacy parsed mutation-plan token '%s' remains in %s"), *Token, *RelativePath));
    }
  }
}
```

- [ ] **Step 2: Verify the tightened test fails**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected: FAIL with remaining `BlueprintGraphMutationPlan*` or `parsed_node_plan_unsupported` tokens.

- [ ] **Step 3: Delete legacy mutation-plan tests**

Delete these files because they only preserve the retired parsed mutation-plan path:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanBuilderTests.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanExecutorTests.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanTests.cpp
```

- [ ] **Step 4: Remove legacy mutation-plan source files**

Delete these implementation files after confirming `rg "BlueprintGraphMutationPlan" BlueprintHelper/Source/BlueprintHelper` only reports the files in this task:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.cpp
```

- [ ] **Step 5: Run residue scans**

Run:

```powershell
rg -n "parsed_node_plan_unsupported|FBlueprintGraphMutationPlan|FBlueprintGraphMutationNodePlan|FBlueprintGraphMutationLinkPlan|MakeNodePlanFromParsedNode|MakeLinkPlanFromParsedLink" BlueprintHelper/Source/BlueprintHelper
rg -n "FParsedNode|FParsedPinType|FParsedLocalVariableDeclaration" BlueprintHelper/Source/BlueprintHelper/Public
```

Expected:
- First command: no source hits outside `BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`.
- Second command: no public header hits.

- [ ] **Step 6: Build and run legacy contract automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected: PASS.

## Task 4: Add asset_action Projection, Revalidation, and Evidence

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add failing projection service tests**

In `BlueprintHelperGenericCreateActionResolverTests.cpp`, add tests named:

```cpp
BlueprintHelper.GraphWrite.ActionResolution.AssetAction.ProjectionWritesStableEvidence
BlueprintHelper.GraphWrite.ActionResolution.AssetAction.ExecuteRejectsStaleProjectedStableId
BlueprintHelper.GraphWrite.ActionResolution.AssetAction.ExecuteRejectsAmbiguousProjection
```

The first test should:

```cpp
FBlueprintHelperAssetActionProjectionRequest ProjectionRequest;
ProjectionRequest.Blueprint = Blueprint;
ProjectionRequest.TargetGraph = Graph;
ProjectionRequest.Query = MenuName;

const FBlueprintHelperAssetActionProjectionResult Projection =
  FBlueprintHelperAssetActionProjectionService::Project(ProjectionRequest);

TestEqual(TEXT("one projected candidate"), Projection.Candidates.Num(), 1);
TestTrue(TEXT("candidate stable id"), Projection.Candidates[0].StableId.StartsWith(TEXT("action_database:")));
TestFalse(TEXT("candidate node class"), Projection.Candidates[0].NodeClassPath.IsEmpty());
TestFalse(TEXT("candidate signature"), Projection.Candidates[0].SpawnerSignature.IsEmpty());
```

The stale-id test should write a valid projection to `Request.ContextEvidence`, then replace `asset_action_stable_id` with `action_database:stale:none:none`, and assert:

```cpp
TestEqual(TEXT("stale projected evidence rejected"), Result.Status, EBlueprintHelperActionResolutionStatus::NotFound);
TestEqual(TEXT("stale projected evidence error"), Result.ErrorCode, FString(TEXT("asset_action_spawner_not_found")));
TestNull(TEXT("stale projected evidence has no spawner"), Result.SelectedSpawner.Get());
```

Also cover weak execute selectors:

- `ExecuteRejectsNodeClassOnlySelector`
- `ExecuteRejectsQueryOnlySelector`

Both should return `InvalidRequest / needs_more_semantic_context`; query/menu/category/node-class fields remain valid only for projection discovery, not for execute-time resolver success.

- [ ] **Step 2: Run the tests and verify failure**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create;BlueprintHelper.GraphWrite.ActionResolution.Contract" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected before implementation: compile failure for `FBlueprintHelperAssetActionProjectionService` or failing assertions because projection is embedded privately in the resolver.

- [ ] **Step 3: Add projection DTOs and writer**

Add `FBlueprintHelperProjectedSpawnerEvidence::WriteAssetActionEvidence`:

```cpp
static void WriteAssetActionEvidence(
  const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
  TMap<FString, FString>& OutContextEvidence);
```

Implementation:

```cpp
OutContextEvidence.Add(TEXT("asset_action_stable_id"), Evidence.StableId);
OutContextEvidence.Add(TEXT("asset_action_node_class"), Evidence.NodeClassPath);
OutContextEvidence.Add(TEXT("asset_action_spawner_signature"), Evidence.SpawnerSignature);
OutContextEvidence.Add(TEXT("asset_action_owner_path"), Evidence.OwnerPath);
OutContextEvidence.Add(TEXT("asset_action_query"), Evidence.Query);
OutContextEvidence.Add(TEXT("asset_action_menu_name"), Evidence.MenuName);
OutContextEvidence.Add(TEXT("asset_action_category"), Evidence.Category);
```

- [ ] **Step 4: Extract ActionDatabase scanning into projection service**

Create `FBlueprintHelperAssetActionProjectionService::Project` with these result rules:

```cpp
if (!Request.Blueprint || !Request.TargetGraph)
{
  Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
  Result.ErrorCode = TEXT("invalid_asset_action_projection_context");
  return Result;
}

FBlueprintActionDatabase::Get().RefreshAll();

// Build the same FBlueprintActionContext and FBlueprintActionFilter used by the resolver.
// For every filtered UBlueprintNodeSpawner, compute:
// StableId = FBlueprintHelperProjectedSpawnerEvidence::MakeAssetActionStableId(ActionOwner, Spawner, NodeClass)
// NodeClassPath, SpawnerSignature, OwnerPath, Query, MenuName, Category.
```

The service must not create synthetic `UBlueprintNodeSpawner` instances. It returns only candidates observed from `FBlueprintActionDatabase`.

- [ ] **Step 5: Make resolver consume the projection service**

In `FBlueprintHelperGenericAssetActionResolver::Resolve`:

```cpp
const FBlueprintHelperProjectedAssetActionEvidence Evidence =
  FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(Request);

if (!Evidence.HasProjectedIdentity())
{
  return MakeInvalidResult(TEXT("asset_action create requires projected ActionDatabase spawner identity evidence."));
}

FBlueprintHelperAssetActionProjectionRequest ProjectionRequest;
ProjectionRequest.Blueprint = Context.Blueprint;
ProjectionRequest.TargetGraph = Context.Graph;
ProjectionRequest.RequiredEvidence = Evidence;

const FBlueprintHelperAssetActionProjectionResult Projection =
  FBlueprintHelperAssetActionProjectionService::Project(ProjectionRequest);

if (Projection.Candidates.Num() == 0)
{
  return MakeNotFoundResult(Evidence, TEXT("asset_action projected evidence did not match any current ActionDatabase spawner."));
}

if (Projection.Candidates.Num() > 1)
{
  return MakeAmbiguousResult(Evidence, Projection.Candidates.Num());
}

return MakeResolvedResult(Projection.Candidates[0]);
```

The revalidation check is the matching operation in `Project`: execute-time resolver success requires the full projected identity tuple (`asset_action_stable_id`, `asset_action_node_class`, `asset_action_spawner_signature`, `asset_action_owner_path`) and the current ActionDatabase candidate must match it. A stale stable id must fail instead of falling back to menu-name-only matching.

- [ ] **Step 6: Strengthen source contract tests**

In `BlueprintHelperActionResolutionContractTests.cpp`, assert:

```cpp
TestTrue(
  TEXT("asset_action resolver uses projection service"),
  GenericAssetResolverSource.Contains(TEXT("FBlueprintHelperAssetActionProjectionService::Project")));

TestFalse(
  TEXT("asset_action resolver does not directly refresh ActionDatabase after extraction"),
  GenericAssetResolverSource.Contains(TEXT("FBlueprintActionDatabase::Get().RefreshAll()")));

TestTrue(
  TEXT("projection service is the only ActionDatabase refresh owner for asset_action"),
  ProjectionServiceSource.Contains(TEXT("FBlueprintActionDatabase::Get().RefreshAll()")));
```

- [ ] **Step 7: Run asset_action tests and source scans**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create;BlueprintHelper.GraphWrite.ActionResolution.Contract" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
rg -n "UBlueprintNodeSpawner::Create" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp
```

Expected:
- Automation tests PASS.
- `rg` has no `UBlueprintNodeSpawner::Create` hit in the `asset_action` resolver/projection files. Broader ActionResolution may still contain existing direct-spawn providers for non-asset_action singleton/generic operations.

## Task 5: Mark asset_action in Review/Debug Evidence

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`

- [ ] **Step 1: Add failing evidence metadata test**

Extend the GraphWrite producer evidence test with an `asset_action` payload containing:

```json
{
  "asset": "/Game/Test/BP_GraphWrite",
  "graph": "Execute",
  "kind": "create",
  "create_operation": "asset_action",
  "context_evidence": {
    "asset_action_stable_id": "action_database:/Script/UMG.WidgetBlueprint:/Script/BlueprintGraph.K2Node_CreateWidget:CreateWidget",
    "asset_action_node_class": "/Script/BlueprintGraph.K2Node_CreateWidget",
    "asset_action_spawner_signature": "CreateWidget",
    "asset_action_owner_path": "/Script/UMG.WidgetBlueprint"
  }
}
```

Assert:

```cpp
TestEqual(TEXT("asset action target kind"), Evidence.AtomicTargets[0].TargetKind, FString(TEXT("graph_block")));
TestTrue(TEXT("anchor keeps asset action stable id"),
  Evidence.AtomicTargets[0].AnchorJson.Contains(TEXT("asset_action_stable_id")));
TestEqual(TEXT("asset action operation kind"), Evidence.OperationKind, FString(TEXT("append_blueprint_graph")));
```

- [ ] **Step 2: Keep GraphWrite target surface stable**

Do not create a second Review target kind for asset actions in this batch. The atomic target remains the graph block because the user-visible mutation is a graph surface change. Asset action details live in `AnchorJson` and DebugBundle diagnostics.

- [ ] **Step 3: Run producer evidence tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected: PASS after Task 2 evidence serialization is stable.

## Task 6: Final Stable-After Preflight and Smoke

**Files:**
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`

- [ ] **Step 1: Confirm all stability gates are green before preflight**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.Cluster;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.PostIO;Quit" -Unattended -NullRHI -TestExit="Automation Test Queue Empty"
```

Expected: PASS. If any command fails, do not start generality preflight; fix the failing gate first.

- [ ] **Step 2: Run the four-cluster smoke plan**

Execute the plan in `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md` after Task 1-5 pass.

Minimum coverage required by the smoke report:

```text
function_action: supported function/macro-like statements through shared ActionContext/adapter path
field: property path, linked typed pin, component_ref, field_access
event: custom_event only in GraphWrite; override/native/delegate-bound entries routed to their owning tools
asset_action: create.asset_action with ActionDatabase-projected stable evidence and execute-time revalidation
```

- [ ] **Step 3: Update plan status documents from runtime evidence**

In `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`, add one status row per cluster:

```markdown
| Cluster | Smoke status | Evidence |
| --- | --- | --- |
| function_action | PASS | Automation/test report path or CLI output id |
| field | PASS | Automation/test report path or CLI output id |
| event | PASS | Automation/test report path or CLI output id |
| asset_action | PASS | Automation/test report path or CLI output id |
```

In `BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`, change the rollout status from draft/stable-candidate to stable only if all four rows are PASS.

- [ ] **Step 4: Run final hygiene checks**

Run:

```powershell
rg -n "parsed_node_plan_unsupported|FBlueprintGraphMutationPlan|FBlueprintGraphMutationNodePlan|FBlueprintGraphMutationLinkPlan" BlueprintHelper/Source/BlueprintHelper
rg -n "FParsedNode|FParsedPinType|FParsedLocalVariableDeclaration" BlueprintHelper/Source/BlueprintHelper/Public
rg -n "UBlueprintNodeSpawner::Create" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp
git diff --check
```

Expected:
- First command has no hits outside the contract test that forbids these strings.
- Second command has no hits.
- Third command has no hits in the `asset_action` resolver/projection files.
- `git diff --check` reports no whitespace errors. Existing line-ending warnings can be recorded separately when no changed line has trailing whitespace.

## 2026-05-25 Execution Status

| Task | Status | Evidence |
|---|---|---|
| Task 1 CapabilityContract | DONE | `npm.cmd --prefix AgentFaceService/task-core run build`; `npm.cmd --prefix AgentFaceService/task-core run test:node`。 |
| Task 2 Review evidence chain | DONE | `Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence`; `Automation RunTests BlueprintHelper.TaskRuntime.PostIO`。 |
| Task 3 Legacy parsed-plan removal | DONE | `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline`; residue scan only hits contract-test forbidden tokens. |
| Task 4 asset_action projection/revalidation | DONE | `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.AssetAction`; `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner`; weak selector regression tests reject query-only/node-class-only execute requests. |
| Task 5 asset_action Review anchor evidence | DONE | Producer evidence test asserts `AnchorJson` preserves `asset_action_stable_id` and `create_operation`。 |
| Task 6 focused smoke status sync | CORE GRAPHWRITE SUITE PASS / FINAL PREFLIGHT PENDING | Full `Automation RunTests BlueprintHelper.GraphWrite` passes after focused gates; the referenced 45-operation generality preflight is still not implemented and not run. |

## 2026-05-25 Full Suite Debug Notes

- `call_function` resolver no longer creates synthetic function node spawners in supplemental scan; function candidates require real ActionDatabase spawner evidence.
- Function ActionDatabase scan refreshes current actions before lookup and canonicalizes Blueprint skeleton-owned functions to generated function stable ids while keeping the real ActionDatabase spawner evidence.
- GraphWrite append test fixtures now provide Signature-owned custom event evidence instead of bypassing the event taxonomy gate with hand-written entries.
- CallFunction stress expectations now treat ActionDatabase-backed Blueprint-authored function candidates as real evidence; ambiguity still blocks resolution when constraints are not unique.
- `asset_action` execute resolution now requires the full projected identity tuple (`asset_action_stable_id`, `asset_action_node_class`, `asset_action_spawner_signature`, `asset_action_owner_path`); weak query/menu/category/node-class selectors are limited to projection discovery.

## Completion Criteria

- `GRAPHWRITE_CAPABILITY_CONTRACT` exists, is exported, and is covered by TaskSpec node tests.
- GraphWrite successful steps produce cluster-owned graph surface atomic targets; failed steps and missing asset/graph payloads produce no Review evidence.
- Review evidence fields survive producer -> post-IO -> ReviewStore record construction with asset path, graph name, operation kind, target key, and task step index intact.
- No active source or public header contains legacy parsed mutation-plan residue.
- `asset_action` cannot succeed without current ActionDatabase projected identity evidence and cannot reuse stale cached stable ids.
- Core `BlueprintHelper.GraphWrite` automation suite passes after the stability gates; final 45-operation / 450-variant generality preflight remains the separate final capability gate.

## Manual Commit Guidance

Do not run these commands inside the agent session. After reviewing the diff, the user can stage only the files changed by this plan:

```powershell
git status --short
git add AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts `
  AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts `
  AgentFaceService/task-core/src/task/schema/task-schemas.ts `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanBuilder.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanBuilderTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanExecutorTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintGraphMutationPlanTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_StabilityClosure_FixPlan_20260525_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md
git commit -m "修复内容：GraphWrite 稳定性闭环"
```

Commit message body:

```text
新增内容：
1. 增加 GraphWriteCapabilityContract 作为全局能力契约。
2. 增加 asset_action ActionDatabase 投影服务和复验证据链。

修复内容：
1. 补全 GraphWrite Review evidence 从 producer 到 ReviewStore 的可靠性验证。
2. 移除 legacy parsed mutation-plan 残留和对应测试。
3. 修正 FunctionAction spawner evidence 复用边界，禁止 supplemental scan 伪造函数 spawner。
4. 收紧 asset_action execute-time 复验，拒绝 query-only/node-class-only 弱选择器。
5. 将四簇 generality preflight 固定为稳定后最终验收。
```
