# RestoreDevice ReviewEvidence Convergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 RestoreDevice / GameInstance 长 flow 中的两层问题：preview 前置假通过和 GraphWrite 执行成功后 ReviewRecord 为空。

**Architecture:** 先修 TaskSpec runner 的 preview 输出契约，保证 UE step failed 不会被顶层 `passed=true` 掩盖；再修 GraphWrite runtime evidence 的 target identity，让 ReviewStore 消费真实 `block_refs` / graph fragment scope；最后收束 member variable `get` 的 owner evidence 合约。Review 数据模型仍是唯一源头，不在 UI / ReviewPanel 层补显示。

**Tech Stack:** TypeScript `node:test` for AgentFaceService task-core, UE 5.6 C++ automation tests, BlueprintHelper TaskRuntime / ReviewStore / GraphWrite runtime clusters, PowerShell on Windows.

---

## 2026-06-01 Execution Status

当前计划已按两层问题收束。原始 checklist 保留为实施计划正文；本节记录实际执行结果和最终根因修正。

| Gate | Status | Evidence |
| --- | --- | --- |
| TaskSpec preview false-positive | Done | `task-spec-runner` now treats failed UE preview steps as blocked top-level preview; focused task-core command exited 0. |
| field owner evidence | Done | Compiler preserves explicit `field_owner_class`; GraphWrite action context projects nested alias into canonical `field.owner_class`; focused task-core command exited 0. |
| planned variable dry-run state | Done | TaskRuntime overlays same-plan `blueprint_variable` dry-run member variables before `graph_write`; partial preview / graph write plan cache keys include planned-state hash. |
| GraphWrite Review target identity | Done | `append_result.block_refs` is consumed, and short refs are normalized to full node metadata block ids before building `graph:<GraphName>:block:<FullBlockId>`. |
| Review visible change regression | Done | `BlueprintHelper.Review.UI.LoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBody` passed and locks `graph_block` loading as graph body visible change. |
| Runtime smoke | Done | Fresh GameInstance smoke produced 1 pending review record and 1 graph body visible change for `append_blueprint_graph EventGraph`. |

Final root cause for Layer B is more precise than the initial coarse-target hypothesis:

```text
append_result.block_refs = CE_DumpGlobalStateForReview0
node metadata BlueprintHelperBlockId = EventGraph_CE_DumpGlobalStateForReview0

Before fix:
  target_key = graph:EventGraph:block:CE_DumpGlobalStateForReview0
  snapshot cannot match node metadata block id
  graph body visible change is filtered away

After fix:
  target_key = graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0
  snapshot matches the generated graph block
  visible_changes includes append_blueprint_graph EventGraph
```

Executed verification:

```text
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload
  Result: Succeeded

UnrealEditor-Cmd Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.TaskRuntime.PostIO
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.TaskRuntime.GraphWrite.DryRunUsesPlannedMemberVariableState
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.GraphWrite.ActionContext.FieldExpressionProjectsOwnerEvidence
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.TaskRuntime.PartialPreviewCache
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.TaskRuntime.GraphWritePlanCache
  exit 0

UnrealEditor-Cmd Automation RunTests BlueprintHelper.Review.UI.LoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBody
  exit 0

npm.cmd --prefix AgentFaceService/task-core run test:node -- task/service/task-spec-runner.test.ts
  309 passed, 0 failed

npm.cmd --prefix AgentFaceService/task-core run test:node -- task/compiler/task-compiler.field.test.ts
  309 passed, 0 failed
```

Runtime evidence files:

```text
.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.preview_after_full_block_id.json
.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.execute_after_full_block_id.json
.tmp\review_evidence_gameinstance_smoke_20260531\Fresh_20260601_0150.query_review_records_after_full_block_id.json
D:\UEProjects\Template\Saved\BlueprintHelper\Review\Records\review_archive_B385C4E54BDD81FC5E41FCA55AB308EF_Game_BlueprintHelperTemp_BP_BH_Evidence_GameInstance_Fresh_20260601_0150.json
```

## Context

关联诊断文档：

```text
BlueprintHelper/Develop/Gap/RestoreDevice_ReviewEvidence_GameInstanceSmoke_Inference_20260531_CN.md
```

本计划把问题拆成两个独立 gate：

```text
Layer A: TaskSpec / GraphWrite preview 前置 gate
Layer B: GraphWrite execute 后 Review evidence -> ReviewRecord gate
```

不要把 Layer A 的 `field_owner_class` 缺失解释成 Layer B 的 Review 空链路根因。`with_owner_evidence` fixture 已证明：owner evidence 补齐后，蓝图节点能生成并编译，但 PostIO 仍然可能从 3 个 evidence item 得到 0 条 ReviewRecord。

## File Structure

### TypeScript preview contract

Modify:

```text
AgentFaceService/task-core/src/task/service/task-spec-runner.ts
AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts
```

Responsibility:

```text
task-spec-runner.ts:
  Normalize Bridge preview responses.
  Convert failed UE preview steps into top-level preview failure.

task-spec-runner.test.ts:
  Lock the false-positive regression where dry_run.can_execute=true but a UE step result failed.
```

### GraphWrite Review target identity

Modify:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp
```

Responsibility:

```text
BlueprintHelperGraphWriteTaskRuntimeCluster.cpp:
  Build Review evidence from actual StepResult output when available.
  Prefer append_result.block_refs over graph_block:<GraphName>.

BlueprintHelperTaskRuntimeClusterHubTests.cpp:
  Lock GraphWrite BuildReviewEvidence target keys against actual block_refs.

BlueprintHelperTaskRuntimePostIoBatchTests.cpp:
  Lock PostIO / ReviewStore behavior for GraphWrite evidence carrying real block refs.
```

### Owner evidence contract

Modify:

```text
AgentFaceService/task-core/src/task/compiler/task-compiler.ts
AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts
```

Responsibility:

```text
task-compiler.ts:
  Add a conservative default field owner class only for same-target-blueprint member variable gets.

task-compiler.field.test.ts:
  Lock generated-class owner inference and preserve explicit context_evidence precedence.
```

### Verification artifacts

Read only:

```text
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.preview.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.execute.json
D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.query_review_records.json
```

The `.tmp` files are evidence inputs for manual verification. Do not stage them.

## Task 1: Fix Preview False-Positive Contract

**Files:**

- Modify: `AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts`
- Modify: `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`

- [ ] **Step 1: Add the failing TypeScript test**

Append this test to `AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts`:

```ts
test('preview task fails when a UE preview step failed even if dry_run says can_execute', async () => {
  const failedStepPreviewBridgeResponse: BridgeResponse = {
    success: true,
    request_id: 'preview_failed_step_request',
    result: {
      ok: true,
      schema: TOOL_RESULT_SCHEMA,
      operation: 'preview_task_plan',
      trace_id: 'trace_preview_failed_step',
      status: 'dry_run',
      modified: false,
      data: {
        preview_token: 'fedcba9876543210fedcba9876543210',
        dry_run: {
          can_execute: true,
          result: 'passed',
          errors: [],
          conflicts: [],
        },
        steps: [
          {
            step_id: 'step_003',
            capability: 'graph_write',
            result: {
              ok: false,
              status: 'failed',
              error: {
                code: 'missing_required_evidence',
                message: 'Field variable action requires explicit owner evidence: semantic=field field_operation=get.',
                field: 'task_plan.steps[2]',
              },
            },
          },
        ],
      },
    },
  };

  const bridge: TaskRunnerBridge = {
    async sendCommand() {
      return failedStepPreviewBridgeResponse;
    },
  };
  const runner = createTaskSpecRunner({
    bridge,
    taskCompiler: async () => createCompiledTaskPlan({
      taskPlan: graphWriteAppendExpectedTaskPlanFixture,
      strategyId: 'canonical_ts',
    }),
  });

  const result = await runner.previewTask(graphWriteAppendTaskSpecFixture);

  assert.equal(result.passed, false);
  assert.equal(result.toolResult.data?.['passed'], false);
  assert.equal(result.toolResult.data?.['blocked'], true);
  assert.equal(result.issues.length, 1);
  assert.equal(result.issues[0]?.code, 'missing_required_evidence');
  assert.equal(result.issues[0]?.path, 'task_plan.steps[2]');
  assert.match(result.issues[0]?.message ?? '', /explicit owner evidence/);
});
```

- [ ] **Step 2: Run the focused TypeScript test and verify it fails**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task/service/task-spec-runner.test.ts
```

Expected before implementation:

```text
FAIL preview task fails when a UE preview step failed even if dry_run says can_execute
actual result.passed = true
```

- [ ] **Step 3: Add failed-step issue extraction**

In `AgentFaceService/task-core/src/task/service/task-spec-runner.ts`, replace `extractDryRun` with this version and add the helper functions immediately below it:

```ts
function extractDryRun(resp: BridgeResponse): { canExecute: boolean; issues: TaskIssue[] } {
  const result = asRecord(resp.result);
  const data = asRecord(result?.['data']);
  const dryRun = asRecord(data?.['dry_run']) ?? asRecord(result?.['dry_run']);
  const canExecute = dryRun?.['can_execute'];
  const failedStepIssues = collectFailedPreviewStepIssues(result, data, dryRun);
  const blockedByStatus =
    result?.['status'] === 'failed' ||
    dryRun?.['result'] === 'blocked' ||
    canExecute === false ||
    failedStepIssues.length > 0;
  const dryRunIssues = collectIssues(dryRun);
  const issues = [
    ...dryRunIssues,
    ...failedStepIssues,
  ];
  return {
    canExecute: typeof canExecute === 'boolean' ? canExecute && !blockedByStatus : !blockedByStatus,
    issues: issues.length > 0 || !blockedByStatus
      ? issues
      : collectBlockedPreviewIssues(result, dryRun),
  };
}

function collectFailedPreviewStepIssues(
  result: Record<string, unknown> | undefined,
  data: Record<string, unknown> | undefined,
  dryRun: Record<string, unknown> | undefined,
): TaskIssue[] {
  const steps = [
    ...arrayOfRecords(data?.['steps']),
    ...arrayOfRecords(dryRun?.['steps']),
    ...arrayOfRecords(result?.['steps']),
  ];

  return steps.flatMap((step, index) => {
    const stepResult = asRecord(step['result']);
    const error = asRecord(stepResult?.['error']) ?? asRecord(step['error']);
    const failed =
      step['status'] === 'failed' ||
      stepResult?.['status'] === 'failed' ||
      stepResult?.['ok'] === false;
    if (!failed) {
      return [];
    }

    const stepId = readNonEmptyString(step['step_id']) ?? `steps[${index}]`;
    const code =
      readString(error?.['code']) ??
      readString(stepResult?.['error_code']) ??
      'preview_step_failed';
    const path =
      readString(error?.['field']) ??
      readString(error?.['path']) ??
      `preview.steps.${stepId}`;
    const message =
      readNonEmptyString(error?.['message']) ??
      readNonEmptyString(stepResult?.['message']) ??
      readNonEmptyString(step['message']) ??
      `Preview step ${stepId} failed.`;

    return [{ code, path, message }];
  });
}
```

- [ ] **Step 4: Run the focused TypeScript test and verify it passes**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task/service/task-spec-runner.test.ts
```

Expected:

```text
PASS task/service/task-spec-runner.test.ts
```

- [ ] **Step 5: Run the full task-core test suite**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core test
```

Expected:

```text
build succeeds
node tests pass
```

- [ ] **Step 6: Record commit scope without committing**

Do not run `git add` or `git commit` from Codex. Record this manual commit scope for the user:

```powershell
git add -- AgentFaceService/task-core/src/task/service/task-spec-runner.ts AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts
git commit -m "fix: fail TaskSpec preview when UE step fails"
```

## Task 2: Bind GraphWrite Review Evidence To Actual Block Refs

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`

- [ ] **Step 1: Add a C++ helper for applied graph result with block_refs**

In `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`, add this helper inside `FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils` after `MakeClusterEvidenceAppliedResult`:

```cpp
static FBlueprintHelperToolResultBase MakeGraphWriteAppliedResultWithBlockRefs(
	const FString& Operation,
	const TArray<FString>& BlockRefs)
{
	FBlueprintHelperToolResultBase Result = MakeClusterEvidenceAppliedResult(Operation);
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> AppendResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BlockRefValues;
	for (const FString& BlockRef : BlockRefs)
	{
		BlockRefValues.Add(MakeShared<FJsonValueString>(BlockRef));
	}
	AppendResult->SetArrayField(TEXT("block_refs"), BlockRefValues);
	Data->SetObjectField(TEXT("append_result"), AppendResult);
	Result.Data = Data;
	return Result;
}
```

- [ ] **Step 2: Change the GraphWrite evidence test expectation**

In the GraphWrite section of `FBlueprintHelperTaskRuntimeCluster_BuildsProducerOwnedReviewEvidence::RunTest`, replace the current `BuildReviewEvidence` call and target count/key assertions with:

```cpp
TestTrue(TEXT("graph write cluster owns Review evidence production"),
	FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		GraphWriteStep,
		FBlueprintHelperTaskRuntimeClusterHubTestsLocalUtils::MakeGraphWriteAppliedResultWithBlockRefs(
			GraphWriteStep.AdapterOperation,
			{TEXT("CE_DumpGlobalStateForReview0")}),
		TEXT("archive_cluster_evidence"),
		TEXT("task_cluster_evidence"),
		3,
		GraphWriteEvidence));
```

Replace the target count/key assertions with:

```cpp
TestEqual(TEXT("graph write emits one graph block target from result refs"), GraphWriteEvidence.AtomicTargets.Num(), 1);
if (GraphWriteEvidence.AtomicTargets.Num() != 1)
{
	return false;
}
const FBlueprintHelperReviewAtomicTarget& GraphTarget = GraphWriteEvidence.AtomicTargets[0];
TestEqual(TEXT("graph write target kind is graph_block"),
	GraphTarget.TargetKind,
	FString(TEXT("graph_block")));
TestEqual(TEXT("graph write target key uses actual block ref"),
	GraphTarget.TargetKey,
	FString(TEXT("graph:EventGraph:block:CE_DumpGlobalStateForReview0")));
TestEqual(TEXT("graph write visual group is graph body"),
	GraphTarget.VisualGroupKey,
	FString(TEXT("graph_body|EventGraph")));
```

- [ ] **Step 3: Run the focused automation test and verify it fails**

Build first:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence;Quit' -TestExit='Automation Test Queue Empty'
```

Expected before implementation:

```text
FAIL graph write target key uses actual block ref
actual = graph_block:EventGraph
expected = graph:EventGraph:block:CE_DumpGlobalStateForReview0
```

- [ ] **Step 4: Add StepResult block ref extraction**

In `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`, add these helper methods inside `FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils` after `SerializePayloadForAnchor`:

```cpp
static void AppendStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}
		FString Text = Value->AsString();
		Text.TrimStartAndEndInline();
		if (!Text.IsEmpty())
		{
			OutValues.AddUnique(Text);
		}
	}
}

static TArray<FString> ReadGraphBlockRefs(const FBlueprintHelperToolResultBase& StepResult)
{
	TArray<FString> BlockRefs;
	if (!StepResult.Data.IsValid())
	{
		return BlockRefs;
	}

	AppendStringArrayField(StepResult.Data, TEXT("block_refs"), BlockRefs);

	const TSharedPtr<FJsonObject>* AppendResult = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("append_result"), AppendResult) && AppendResult && AppendResult->IsValid())
	{
		AppendStringArrayField(*AppendResult, TEXT("block_refs"), BlockRefs);
	}

	return BlockRefs;
}

static FString MakeGraphBlockTargetKey(const FString& GraphName, const FString& BlockRef)
{
	return FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *BlockRef);
}
```

- [ ] **Step 5: Use block refs in BuildReviewEvidence**

In `BuildReviewEvidence`, replace the single `TargetKey` and single `OutEvidence.AtomicTargets.Add(Target)` path with this block after `OutEvidence.TaskStepIndex = StepIndex;`:

```cpp
const TArray<FString> BlockRefs = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ReadGraphBlockRefs(StepResult);
TArray<FString> TargetKeys;
for (const FString& BlockRef : BlockRefs)
{
	TargetKeys.AddUnique(FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::MakeGraphBlockTargetKey(GraphName, BlockRef));
}
if (TargetKeys.Num() == 0)
{
	TargetKeys.Add(FString::Printf(TEXT("graph_block:%s"), *GraphName));
}

for (int32 TargetIndex = 0; TargetIndex < TargetKeys.Num(); ++TargetIndex)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_block");
	Target.TargetKey = TargetKeys[TargetIndex];
	Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
	Target.DisplayLabel = FString::Printf(TEXT("%s %s"), *OperationKind, *GraphName);
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.Ownership = TEXT("graph_write");
	Target.AnchorJson = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::SerializePayloadForAnchor(LoweredStep.Payload);
	Target.ExecutionOrder = StepIndex;
	Target.TaskStepIndex = StepIndex;
	Target.AtomicIndex = TargetIndex;
	OutEvidence.AtomicTargets.Add(Target);
}
return OutEvidence.AtomicTargets.Num() > 0;
```

The fallback `graph_block:<GraphName>` exists only for old StepResult shapes with no result refs; the new regression test must cover the real `block_refs` path.

- [ ] **Step 6: Run the focused automation test and verify it passes**

Build:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence;Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
Automation Test Queue Empty
0 failed
```

- [ ] **Step 7: Record commit scope without committing**

Do not run `git add` or `git commit` from Codex. Record this manual commit scope for the user:

```powershell
git add -- BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp
git commit -m "fix: bind GraphWrite review evidence to block refs"
```

## Task 3: Lock PostIO / ReviewStore Regression For Real Graph Block Targets

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`

- [ ] **Step 1: Update the graph evidence fixture target key**

In `MakeGraphWriteEvidence`, replace:

```cpp
Target.TargetKey = TEXT("graph_block:EventGraph");
```

with:

```cpp
Target.TargetKey = TEXT("graph:EventGraph:block:CE_DumpGlobalStateForReview0");
```

- [ ] **Step 2: Update the expected target key assertion**

In `FBlueprintHelperTaskRuntimePostIoReviewStore_PreservesGraphWriteEvidenceFields::RunTest`, replace:

```cpp
TestEqual(TEXT("target key survives evidence construction"),
	Target.TargetKey,
	FString(TEXT("graph_block:EventGraph")));
```

with:

```cpp
TestEqual(TEXT("target key preserves concrete graph block ref"),
	Target.TargetKey,
	FString(TEXT("graph:EventGraph:block:CE_DumpGlobalStateForReview0")));
```

- [ ] **Step 3: Run the PostIO automation tests**

Build:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.TaskRuntime.PostIO;Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
Automation Test Queue Empty
0 failed
```

- [ ] **Step 4: If the test exposes net-no-change removal, add a concrete ReviewStore test**

If Step 3 fails because the concrete graph block target is filtered out of `VisibleChanges`, append this test to `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewStore_GraphBlockTargetKeepsConcreteBlockRef,
	"BlueprintHelper.Review.Store.GraphBlockTargetKeepsConcreteBlockRef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperReviewStore_GraphBlockTargetKeepsConcreteBlockRef::RunTest(const FString& Parameters)
{
	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = TEXT("archive_graph_block_concrete_ref");
	Evidence.TaskRunId = TEXT("task_graph_block_concrete_ref");
	Evidence.EvidenceId = TEXT("evidence_graph_block_concrete_ref");
	Evidence.CreatedAt = TEXT("2026-05-31T10:00:00Z");
	Evidence.AssetPath = TEXT("/Game/BP_Door");
	Evidence.OperationKind = TEXT("append_blueprint_graph");
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Evidence.DisplayLabel = TEXT("append_blueprint_graph");
	Evidence.TaskStepIndex = 1;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Evidence.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("graph_block");
	Target.TargetKey = TEXT("graph:EventGraph:block:CE_DumpGlobalStateForReview0");
	Target.VisualGroupKey = TEXT("graph_body|EventGraph");
	Target.DisplayLabel = TEXT("append_blueprint_graph EventGraph");
	Target.Ownership = TEXT("graph_write");
	Target.LatestEvidenceId = Evidence.EvidenceId;
	Target.SourceEvidenceIds.Add(Evidence.EvidenceId);
	Target.AnchorJson = TEXT("{\"graph_name\":\"EventGraph\"}");
	Evidence.AtomicTargets.Add(Target);

	const FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence({Evidence});

	TestEqual(TEXT("one review record is built"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("one visible graph change is built"), Records[0].VisibleChanges.Num(), 1);
	if (Records[0].VisibleChanges.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("concrete graph block target survives"),
		Records[0].VisibleChanges[0].AtomicTargets.Num() == 1
			? Records[0].VisibleChanges[0].AtomicTargets[0].TargetKey
			: FString(),
		FString(TEXT("graph:EventGraph:block:CE_DumpGlobalStateForReview0")));
	return true;
}
```

Run this focused test:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.Store.GraphBlockTargetKeepsConcreteBlockRef;Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
Automation Test Queue Empty
0 failed
```

- [ ] **Step 5: Record commit scope without committing**

Do not run `git add` or `git commit` from Codex. Record this manual commit scope for the user:

```powershell
git add -- BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
git commit -m "test: cover concrete GraphWrite review targets"
```

## Task 4: Add Conservative Field Owner Evidence Inference

**Files:**

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`

- [ ] **Step 1: Add a failing compiler test for default owner evidence**

Append to `AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`:

```ts
test('member variable get receives default field_owner_class for target blueprint', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'OutputText',
    value: { kind: 'get', name: 'SessionId' },
  });

  const value = statement.value as Record<string, unknown>;
  assert.equal(value.kind, 'field');
  assert.equal(value.field_operation, 'get');
  assert.equal(value.field_scope, 'variable');
  assert.deepEqual(value.context_evidence, {
    field_owner_class: '/Game/BP/BP_Door.BP_Door_C',
  });
});

test('explicit field_owner_class is preserved for member variable get', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'OutputText',
    value: {
      kind: 'get',
      name: 'SessionId',
      context_evidence: {
        field_owner_class: '/Game/Other/BP_Other.BP_Other_C',
      },
    },
  });

  const value = statement.value as Record<string, unknown>;
  assert.deepEqual(value.context_evidence, {
    field_owner_class: '/Game/Other/BP_Other.BP_Other_C',
  });
});
```

- [ ] **Step 2: Run the focused compiler test and verify it fails**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task/compiler/task-compiler.field.test.ts
```

Expected before implementation:

```text
FAIL member variable get receives default field_owner_class for target blueprint
actual context_evidence = undefined
```

- [ ] **Step 3: Add compile context owner support**

In `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`, update `CompileFlowContext` and `makeCompileFlowContext`:

```ts
interface CompileFlowContext {
  symbols: Map<string, CompiledSymbolValue>;
  defaultFieldOwnerClass?: string;
}

function makeCompileFlowContext(parent?: CompileFlowContext, defaultFieldOwnerClass?: string): CompileFlowContext {
  return {
    symbols: new Map(parent ? parent.symbols : []),
    defaultFieldOwnerClass: defaultFieldOwnerClass ?? parent?.defaultFieldOwnerClass,
  };
}
```

Add these helpers near `copyContextEvidence`:

```ts
function blueprintGeneratedClassPath(assetPath: string): string {
  const normalized = assetPath.trim();
  const slashIndex = normalized.lastIndexOf('/');
  const assetName = slashIndex >= 0 ? normalized.slice(slashIndex + 1) : normalized;
  return `${normalized}.${assetName}_C`;
}

function ensureDefaultFieldOwnerEvidence(target: Record<string, unknown>, context: CompileFlowContext): void {
  if (
    target.kind !== 'field' ||
    target.field_operation !== 'get' ||
    target.field_scope !== 'variable' ||
    typeof context.defaultFieldOwnerClass !== 'string' ||
    context.defaultFieldOwnerClass.length === 0
  ) {
    return;
  }

  const evidence = isRecord(target.context_evidence)
    ? { ...target.context_evidence }
    : {};
  if (!Object.hasOwn(evidence, 'field_owner_class')) {
    evidence.field_owner_class = context.defaultFieldOwnerClass;
  }
  target.context_evidence = evidence;
}
```

- [ ] **Step 4: Thread the default owner through graph compilation**

Update the edit-graph path near `compileTaskSpecToTaskPlan` so GraphWrite ops receive the target generated class:

```ts
const defaultFieldOwnerClass = blueprintGeneratedClassPath(taskSpec.target.asset_path);
const graphWriteOps = compileGraphWriteOps(behavior, defaultFieldOwnerClass);
```

Change these signatures:

```ts
function compileGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileAppendGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileReplaceGraphWriteOp(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp
function compilePatchGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileMergeGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileExternalMergeGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileExternalPatchGraphWriteOps(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp[]
function compileExternalReplaceBodyGraphWriteOp(behavior: Record<string, unknown>, defaultFieldOwnerClass?: string): GraphWriteCompiledOp
function compileLogicBodyToSemanticLogicSpec(body: { statements: BlueprintLogicStatement[] }, prefix: string, defaultFieldOwnerClass?: string): Record<string, unknown>
```

For each call that compiles a logic body, pass `defaultFieldOwnerClass`:

```ts
body: compileLogicBodyToSemanticLogicSpec(body, entryName, defaultFieldOwnerClass)
logic_spec: compileLogicBodyToSemanticLogicSpec(body, 'replace', defaultFieldOwnerClass)
body: compileLogicBodyToSemanticLogicSpec(body, `external_merge_${index}`, defaultFieldOwnerClass)
logic_spec: compileLogicBodyToSemanticLogicSpec(body, 'external_body_replace', defaultFieldOwnerClass)
```

Update `compileLogicBodyToSemanticLogicSpec`:

```ts
function compileLogicBodyToSemanticLogicSpec(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  defaultFieldOwnerClass?: string,
): Record<string, unknown> {
  return {
    schema: 'BlueprintLogicSpec.v2',
    statements: cloneLogicStatementSequenceWithCompiledIds(
      body.statements,
      `${toIdSegment(prefix)}_stmt`,
      makeCompileFlowContext(undefined, defaultFieldOwnerClass),
    ),
  };
}
```

- [ ] **Step 5: Thread context through clone functions**

Update clone signatures and recursive calls:

```ts
function cloneLogicExpressionWithCompiledIds(
  expression: unknown,
  nodeId: string,
  context: CompileFlowContext,
): unknown

function cloneLogicStatementWithCompiledIds(
  statement: BlueprintLogicStatement,
  statementId: string,
  context: CompileFlowContext,
): BlueprintLogicStatement

function cloneLogicStatementSequenceWithCompiledIds(
  statements: BlueprintLogicStatement[],
  idPrefix: string,
  context: CompileFlowContext,
): BlueprintLogicStatement[]
```

After every `applyFieldTaxonomy(...)` call in clone expression/statement paths, call:

```ts
ensureDefaultFieldOwnerEvidence(out, context);
```

When cloning nested expressions, pass the same context:

```ts
out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, context);
out.args = Object.fromEntries(
  Object.entries(args).map(([argName, argValue]) => [
    argName,
    cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, context),
  ]),
);
```

- [ ] **Step 6: Run the focused compiler test and verify it passes**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task/compiler/task-compiler.field.test.ts
```

Expected:

```text
PASS task/compiler/task-compiler.field.test.ts
```

- [ ] **Step 7: Run the full task-core test suite**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core test
```

Expected:

```text
build succeeds
node tests pass
```

- [ ] **Step 8: Record commit scope without committing**

Do not run `git add` or `git commit` from Codex. Record this manual commit scope for the user:

```powershell
git add -- AgentFaceService/task-core/src/task/compiler/task-compiler.ts AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts
git commit -m "fix: infer field owner for target blueprint gets"
```

## Task 5: Runtime Smoke Verification

**Files:**

- Read: `.tmp/review_evidence_gameinstance_smoke_20260531/write_global_state_flow.json`
- Read: `.tmp/review_evidence_gameinstance_smoke_20260531/write_global_state_flow.with_owner_evidence.json`
- Read/write runtime evidence outputs under `.tmp/review_evidence_gameinstance_smoke_20260531`

- [ ] **Step 1: Build C++ and TypeScript**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core test
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
task-core build succeeds
node tests pass
TemplateEditor build succeeds
```

- [ ] **Step 2: Run the focused UE automation set**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence;Automation RunTests BlueprintHelper.TaskRuntime.PostIO;Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
Automation Test Queue Empty
0 failed
```

- [ ] **Step 3: Re-run the raw TaskSpec preview**

Use the original raw fixture:

```powershell
bh.cmd task preview --input '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.json' --output '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.after_fix.preview.json'
```

Expected if Task 4 is implemented:

```text
preview passes
no Field variable action requires explicit owner evidence error
```

If Task 4 is intentionally not included in a build, expected fallback:

```text
preview fails at top level
passed=false
issues include field_owner_class / explicit owner evidence
```

- [ ] **Step 4: Re-run the with-owner execute/query smoke**

Run:

```powershell
bh.cmd task preview --input '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.json' --output '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.after_fix.preview.json'
bh.cmd task execute --input '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.json' --output '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.after_fix.execute.json'
bh.cmd review query --asset-path '/Game/BlueprintHelperTemp/BP_BH_Evidence_GameInstance' --output '.tmp\review_evidence_gameinstance_smoke_20260531\write_global_state_flow.with_owner_evidence.after_fix.query_review_records.json'
```

Expected:

```text
execute compile_succeeded = true
execute post_io.ok = true
execute diagnostics do not include review_evidence_produced_zero_records
query review records count >= 1
visible_changes contains a graph/event/body change
```

- [ ] **Step 5: Record final manual commit scope**

Do not run `git add` or `git commit` from Codex. Record this manual commit scope for the user:

```powershell
git status --short
git add -- AgentFaceService/task-core/src/task/service/task-spec-runner.ts AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts AgentFaceService/task-core/src/task/compiler/task-compiler.ts AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp
git commit -m "fix: close RestoreDevice review evidence pipeline"
```

Before the user runs this, ensure unrelated working-tree changes are not staged.

## Self-Review

Spec coverage:

```text
Preview false-positive is covered by Task 1.
GraphWrite Review target identity is covered by Task 2.
PostIO / ReviewStore regression is covered by Task 3.
field_owner_class前置合约 is covered by Task 4.
End-to-end runtime gates are covered by Task 5.
```

Placeholder scan:

```text
No unresolved placeholder phrases are intentionally left in this plan.
Each code-changing step names the exact file and supplies the code shape to apply.
```

Type consistency:

```text
TaskIssue uses code/path/message consistently with existing task-spec-runner.ts.
GraphWrite block target key uses graph:<GraphName>:block:<FullBlockId>, where short `append_result.block_refs` are expanded to the same `BlueprintHelperBlockId` stored on generated nodes.
field_owner_class uses generated class path format /Game/Path/BP_Name.BP_Name_C.
```
