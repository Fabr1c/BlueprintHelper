# GraphWrite AssetAction Review Policy GraphBlock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 固化已讨论结论：`asset_action` 不实现 `asset_action_atomic_target`，public contract 收窄为统一 `graph_surface_atomic_target`，runtime Review evidence 继续产出 graph-level `graph_block`。

**Architecture:** Review v2 的 GraphWrite evidence 以 graph surface 为唯一粒度，`asset_action` 的 ActionDatabase / spawner identity 属于 ActionResolution 和 Debug diagnostics，不成为 Review 原子 target。DebugBundle / Review UI / AcceptReject 继续消费同一套 ReviewStore graph_block 模型，避免 `asset_action` 独立解释一套 Review 粒度。

**Tech Stack:** TypeScript TaskSpec capability contract, UE 5.6 C++ TaskRuntime Review evidence, Unreal Automation tests, ripgrep static contract scans.

---

## Decision Record

| Topic | Decision |
|---|---|
| Public Review policy | Only `graph_surface_atomic_target` is public for GraphWrite operations. |
| `asset_action` Review target | Do not add `asset_action_atomic_target`. `create.asset_action` uses the same graph_block evidence as other GraphWrite graph mutations. |
| Review / DebugBundle granularity | DebugBundle follows ReviewStore graph_block granularity. Action-level spawner evidence can appear in ActionResolution diagnostics or fixture reports, not as Review atomic targets. |
| Future action-level debugging | If action-level Review/DebugBundle is needed later, create a separate enhancement with a new Review data-model decision. Do not pre-add hidden enum values or unused target kinds. |

## Current Implementation Facts To Preserve

| Area | Required state |
|---|---|
| TS contract | `GraphWriteReviewEvidencePolicy` is exactly `'graph_surface_atomic_target'`. |
| `create.asset_action` contract | `reviewEvidence` is `graph_surface_atomic_target`. |
| Runtime evidence | `FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence` emits one atomic target with `TargetKind=graph_block` and `TargetKey=graph_block:<graph_name>`. |
| Failure behavior | Failed GraphWrite step, missing asset, or missing graph produces no Review evidence. |
| Static ban | `asset_action_atomic_target` must not appear in public GraphWrite contract, runtime evidence, tests except in explicit forbidden-token scans. |

## File Structure

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Keep review policy union narrowed to `graph_surface_atomic_target`.
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - Assert `create.asset_action` uses `graph_surface_atomic_target` and no operation uses an action-level policy.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
  - Assert GraphWrite evidence target stays graph_block even when the operation comes from an asset_action GraphWrite step.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
  - Mark `asset_action Review policy` as DECIDED / graph_block and link this plan.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
  - Ensure final matrix uses graph_block Review checks for all GraphWrite operations including `create.asset_action`.
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - Keep design record aligned with graph_block Review policy.

## Task 1: Lock Public Contract To GraphBlock Review Policy

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`

- [ ] **Step 1: Add failing contract test for forbidden action-level policy**

Add this test to `graphwrite-capability-contract.test.ts`:

```ts
it('does not expose asset_action-specific Review target policy', () => {
  const serialized = JSON.stringify(GRAPHWRITE_CAPABILITY_CONTRACT);

  assert.equal(serialized.includes('asset_action_atomic_target'), false);
  for (const cluster of GRAPHWRITE_CAPABILITY_CONTRACT.clusters) {
    for (const operation of cluster.operations) {
      assert.equal(
        operation.reviewEvidence,
        'graph_surface_atomic_target',
        `${cluster.id}.${operation.id} must use graph-level GraphWrite Review evidence`,
      );
    }
  }
});
```

- [ ] **Step 2: Run Node contract test**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- graphwrite-capability-contract
```

Expected before implementation if the contract has drifted: failure mentioning `asset_action_atomic_target` or a non-graph Review policy. Expected after implementation: the test passes.

- [ ] **Step 3: Keep the TypeScript review policy type narrowed**

In `graphwrite-capability-contract.ts`, the type must remain:

```ts
export type GraphWriteReviewEvidencePolicy = 'graph_surface_atomic_target';
```

Do not expand it to:

```ts
export type GraphWriteReviewEvidencePolicy =
  | 'graph_surface_atomic_target'
  | 'asset_action_atomic_target';
```

- [ ] **Step 4: Ensure asset_action operation is graph-level**

In the `asset_action` cluster, keep:

```ts
{
  id: 'create.asset_action',
  kind: 'create',
  supportStatus: 'supported',
  reviewEvidence: 'graph_surface_atomic_target',
  requiredEvidenceKeys: ASSET_ACTION_REQUIRED_EVIDENCE_KEYS,
}
```

- [ ] **Step 5: Re-run full task-core verification**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: build succeeds and all Node tests pass.

## Task 2: Lock Runtime Review Evidence To graph_block

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Verify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`

- [ ] **Step 1: Add asset_action-shaped GraphWrite evidence test**

Add a focused test near the existing GraphWrite Review evidence tests in `BlueprintHelperTaskRuntimeClusterHubTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeGraphWriteAssetActionUsesGraphBlockReviewEvidenceTest,
	"BlueprintHelper.TaskRuntime.ClusterHub.GraphWriteAssetActionUsesGraphBlockReviewEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeGraphWriteAssetActionUsesGraphBlockReviewEvidenceTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/BP/BP_AssetActionReviewPolicy"));
	Payload->SetStringField(TEXT("graph_name"), TEXT("EventGraph"));
	Payload->SetStringField(TEXT("create_operation"), TEXT("asset_action"));
	TSharedRef<FJsonObject> ContextEvidence = MakeShared<FJsonObject>();
	ContextEvidence->SetStringField(TEXT("asset_action_stable_id"), TEXT("action_database:/Script/Test.Asset:/Script/Test.Node:sig"));
	ContextEvidence->SetStringField(TEXT("asset_action_node_class"), TEXT("/Script/Test.Node"));
	ContextEvidence->SetStringField(TEXT("asset_action_spawner_signature"), TEXT("sig"));
	ContextEvidence->SetStringField(TEXT("asset_action_owner_path"), TEXT("/Script/Test.Asset"));
	Payload->SetObjectField(TEXT("context_evidence"), ContextEvidence);

	FBlueprintHelperTaskRuntimeLoweredStep Step;
	Step.Capability = TEXT("graph_write");
	Step.AdapterOperation = TEXT("append_blueprint_graph");
	Step.Payload = Payload;

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Operation = TEXT("append_blueprint_graph");

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		Step,
		StepResult,
		TEXT("archive_asset_action_review_policy"),
		TEXT("task_asset_action_review_policy"),
		7,
		Evidence);

	TestTrue(TEXT("asset_action-shaped GraphWrite step builds graph-level Review evidence"), bBuilt);
	TestEqual(TEXT("one atomic graph target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() == 1)
	{
		const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
		TestEqual(TEXT("target kind remains graph_block"), Target.TargetKind, FString(TEXT("graph_block")));
		TestEqual(TEXT("target key remains graph_block graph name"), Target.TargetKey, FString(TEXT("graph_block:EventGraph")));
		TestEqual(TEXT("target graph name"), Target.GraphName, FString(TEXT("EventGraph")));
		TestEqual(TEXT("target ownership"), Target.Ownership, FString(TEXT("graph_write")));
		TestFalse(TEXT("no asset_action target kind"), Target.TargetKind.Equals(TEXT("asset_action_atomic_target"), ESearchCase::IgnoreCase));
	}
	return true;
}
```

If the local helper names differ in this test file, keep the payload shape and assertions identical; only adapt construction to the file's existing helper style.

- [ ] **Step 2: Verify runtime code stays operation-agnostic**

`FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence` must keep this shape:

```cpp
const FString TargetKey = FString::Printf(TEXT("graph_block:%s"), *GraphName);

FBlueprintHelperReviewAtomicTarget Target;
Target.AssetPath = AssetPath;
Target.Surface = EBlueprintHelperReviewSurface::Graph;
Target.GraphName = GraphName;
Target.TargetKind = TEXT("graph_block");
Target.TargetKey = TargetKey;
Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
Target.Ownership = TEXT("graph_write");
```

Do not add:

```cpp
if (PayloadCreateOperation == TEXT("asset_action"))
{
	Target.TargetKind = TEXT("asset_action_atomic_target");
}
```

- [ ] **Step 3: Run focused TaskRuntime tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.ClusterHub;Quit"
```

Expected: GraphWrite evidence tests pass, including the asset_action-shaped payload case.

## Task 3: Prevent DebugBundle / ReviewStore Granularity Drift

**Files:**
- Modify if needed: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`
- Verify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Debug/BlueprintHelperDebugCaseStoreService.cpp`
- Verify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review`

- [ ] **Step 1: Add PostIO preservation assertion if missing**

In `BlueprintHelperTaskRuntimePostIoBatchTests.cpp`, ensure the existing graph_block preservation test asserts:

```cpp
TestEqual(TEXT("post io target kind stays graph_block"),
	Record.SourceReviewSummary.AtomicTargets[0].TargetKind,
	FString(TEXT("graph_block")));
TestEqual(TEXT("post io target key stays graph_block graph name"),
	Record.SourceReviewSummary.AtomicTargets[0].TargetKey,
	FString(TEXT("graph_block:EventGraph")));
```

If the file stores targets in a different local variable, keep the same expected values.

- [ ] **Step 2: Add static scan for forbidden DebugBundle target**

Add this verification command to the final task script or release checklist:

```powershell
rg -n "asset_action_atomic_target" AgentFaceService/task-core/src BlueprintHelper/Source BlueprintHelper/Develop
```

Expected after implementation: no production-code hits. A hit is allowed only in this plan or in an explicit forbidden-token test assertion.

- [ ] **Step 3: Run PostIO tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.PostIO;Quit"
```

Expected: ReviewStore/PostIO records preserve graph_block target data.

## Task 4: Documentation Sync

**Files:**
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [ ] **Step 1: Sync capability contract status**

In `BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`, keep this status:

```markdown
`asset_action` Review policy 已收窄为 graph-level `graph_block` / `graph_surface_atomic_target`；不实现 `asset_action_atomic_target`。
```

- [ ] **Step 2: Sync final generality preflight policy**

In `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`, ensure `asset_action` final preflight checks action success/readback separately from Review target granularity:

```markdown
`create.asset_action` 的 ActionDatabase projection/readback 可以是 operation-specific；Review evidence 验收仍统一检查 `TargetKind=graph_block`、`TargetKey=graph_block:<graph_name>`。
```

- [ ] **Step 3: Sync design decision**

In `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`, keep the design row:

```markdown
| `asset_action` Review evidence policy | DECIDED / graph_block | public contract 收窄为 `graph_surface_atomic_target`；runtime `BuildReviewEvidence` 继续产出 graph-level `graph_block` target。 | Review/DebugBundle 粒度保持图级，不追踪 asset-action atomic target；若未来需要 action 级 DebugBundle，再作为增强项立项。 |
```

## Task 5: Final Verification

**Files:**
- Verify all files above.

- [ ] **Step 1: Run TypeScript verification**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: build succeeds and Node tests pass.

- [ ] **Step 2: Run UE focused verification**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.ClusterHub;BlueprintHelper.TaskRuntime.PostIO;Quit"
```

Expected: focused Review evidence tests pass.

- [ ] **Step 3: Run static no-action-target scan**

Run:

```powershell
rg -n "asset_action_atomic_target" AgentFaceService/task-core/src BlueprintHelper/Source
```

Expected: no output and exit code 1.

- [ ] **Step 4: Run whitespace check**

Run:

```powershell
git diff --check -- AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
```

Expected: no whitespace errors. LF/CRLF warnings are acceptable if no error lines are reported.

## Non-Goals

- Do not create `asset_action_atomic_target`.
- Do not add action-level Review target kinds for `asset_action`.
- Do not make DebugBundle maintain a separate asset_action interpretation outside ReviewStore.
- Do not use Review policy to prove `asset_action` ActionDatabase projection correctness; that remains covered by ActionResolution and positive TaskSpec/readback fixtures.

## Suggested Manual Commit Message After Execution

```text
变更需求：
1. 收窄 GraphWrite asset_action Review policy 为 graph_block 级 evidence

新增内容：
1. 防止 asset_action_atomic_target 回归的 contract 和 Review evidence 测试
```
