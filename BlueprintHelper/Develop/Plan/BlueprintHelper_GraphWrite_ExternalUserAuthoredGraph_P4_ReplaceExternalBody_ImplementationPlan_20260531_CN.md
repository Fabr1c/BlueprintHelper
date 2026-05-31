# GraphWrite External User-Authored Graph P4 Replace External Body Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增高风险 `replace_external_body`：对显式 user-authored function、custom event 或 event body 执行 in-place rewrite / replace，并提供 full dry-run、dependents analysis、stale check 和 Review v2 evidence-before 回滚。

**Architecture:** P4 使用独立 replace service、external body snapshot service 和 dependents analysis service。保留 external entry node 的 user-authored 身份；替换后生成的 body 节点归属新的 BlueprintHelper-owned block。不得调用 owned replace service，不得隐式接管 entry，不开放任意 whole-graph replace。

**Tech Stack:** TypeScript、Zod、UE 5.6 C++、BlueprintHelper Task Runtime、GraphWrite semantic body builder、Review v2、Automation Tests。
---

## Contract

```ts
{
  graph_strategy: 'replace_external_body',
  external_replace: {
    scope: 'custom_event_body' | 'event_body' | 'function_body',
    anchor: ExternalGraphAnchorV1,
    body: BlueprintLogicSpec,
    expected_body_fingerprint: string,
    require_full_dry_run: true,
  },
}
```

Explicitly out of scope:

```text
graph
custom_event_definition
signature mutation
whole-graph delete and rebuild
entry ownership adoption
```

## Task 1: Add TypeScript Contract

**Files:**

- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.replace-external-body.test.ts`

- [ ] **Step 1: Add schema**

```ts
const GraphWriteExternalReplaceBodySchema = z.object({
  scope: z.enum(['custom_event_body', 'event_body', 'function_body']),
  anchor: ExternalGraphAnchorSchema.extend({
    semantic_role: z.literal('body_entry'),
  }),
  body: BlueprintLogicSpecSchema,
  expected_body_fingerprint: z.string().min(1),
  require_full_dry_run: z.literal(true),
}).strict();
```

- [ ] **Step 2: Lower exact policy**

```ts
constraints: {
  allow_modify_user_nodes: false,
  ownership_scope: 'external_user_authored',
  external_mutation_policy: {
    strategy: 'replace_external_body',
    allowed_mutations: ['body_replace'],
  },
}
```

- [ ] **Step 3: Add compiler rejection tests**

Reject:

- `scope='graph'`;
- missing expected body fingerprint;
- `require_full_dry_run=false`;
- selector by display name only;
- broad `allow_modify_user_nodes=true`;
- owned `replace` mixed with external replace.

## Task 2: Add External Body Snapshot and Dependents Analysis

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.cpp`

- [ ] **Step 1: Define snapshot API**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperExternalBodySnapshotService
{
public:
	bool CaptureBody(
		UEdGraph* Graph,
		UEdGraphNode* EntryNode,
		FBlueprintHelperExternalBodySnapshot& OutSnapshot,
		FString& OutError) const;
};
```

Snapshot must contain:

- preserved external entry GUID and class;
- deterministic body fingerprint;
- exact removable body node GUIDs;
- restore text for body nodes;
- entry-to-body boundary links;
- body-to-external dependent links.

- [ ] **Step 2: Define dependents analysis**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperExternalDependentsAnalysisService
{
public:
	bool Analyze(
		UEdGraph* Graph,
		const FBlueprintHelperExternalBodySnapshot& Snapshot,
		FBlueprintHelperExternalDependentsAnalysis& OutAnalysis,
		FString& OutError) const;
};
```

Block execute when body nodes have unsupported outgoing dependents. Dry-run must report:

```text
preserved_entry
nodes_to_remove
nodes_to_create
links_to_remove
links_to_restore
unsupported_dependents
```

## Task 3: Implement Dedicated Replace Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperReplaceExternalBodyService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperReplaceExternalBodyService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`

- [ ] **Step 1: Add service API**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperReplaceExternalBodyService
{
public:
	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;
};
```

- [ ] **Step 2: Implement preflight**

Required order:

1. validate external policy;
2. require full dry-run mode;
3. resolve body entry anchor;
4. validate node class is compatible with requested scope;
5. capture body snapshot;
6. compare expected body fingerprint;
7. run dependents analysis;
8. build exact remove/create/link plan;
9. only then execute.

- [ ] **Step 3: Preserve entry identity**

Execute:

1. keep external entry node and metadata untouched;
2. remove only analyzed body nodes;
3. generate replacement semantic body;
4. mark generated replacement body nodes with a new BlueprintHelper-owned block id;
5. reconnect preserved entry and supported external dependents;
6. emit body snapshot evidence.

Do not invoke `FBlueprintHelperReplaceBlueprintGraphService`.

- [ ] **Step 4: Register `replace_external_body`**

Add adapter operation to `FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation`. Entry constructs `FBlueprintHelperReplaceExternalBodyService` and registers its handler. Reuse the P2 registry dependency path; do not widen Bridge route or TaskRuntime cluster constructors again.

## Task 4: Add Review v2 Body Handler

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTargetKindRegistry.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.cpp`

- [ ] **Step 1: Register handler**

```cpp
GraphExternalBody
```

with:

```cpp
{ TEXT("graph_external_body"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_body"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalBody }
```

- [ ] **Step 2: Restore evidence-before body**

Reject restore:

1. resolve preserved entry by GUID;
2. remove replacement body nodes listed by latest-after snapshot;
3. import evidence-before restore text;
4. reconnect entry and external dependent links;
5. leave current mismatch details in diagnostics;
6. do not block Reject only because current differs from latest after.

## Task 5: Add Tests

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperReplaceExternalBodyTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`

- [ ] **Step 1: Register focused tests**

- `BlueprintHelper.GraphWrite.ExternalBodyReplace.FunctionPreviewListsExactPlan`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.FunctionExecutePreservesEntry`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.CustomEventExecutePreservesEntry`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.EventExecutePreservesEntry`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsWholeGraphScope`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsStaleBodyFingerprint`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.RejectsUnsupportedDependents`
- `BlueprintHelper.GraphWrite.ExternalBodyReplace.DoesNotAdoptExternalEntry`
- `BlueprintHelper.Review.ExternalBody.RejectRestoresEvidenceBeforeBody`
- `BlueprintHelper.Review.ExternalBody.CurrentMismatchDoesNotBlockReject`

## Task 6: Verification

- [ ] **Step 1: Run focused suites**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ExternalBodyReplace;Quit' -TestExit='Automation Test Queue Empty'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.Review.ExternalBody;Quit' -TestExit='Automation Test Queue Empty'
```

- [ ] **Step 2: Run common gate**

Use the master plan common verification gate.

## Manual Commit Checkpoint

Suggested commit message:

```text
新增内容：
1. 新增 GraphWrite replace_external_body 用户 body 原位重写能力
2. 新增 external body snapshot、dependents analysis 和 Review v2 回滚
```
