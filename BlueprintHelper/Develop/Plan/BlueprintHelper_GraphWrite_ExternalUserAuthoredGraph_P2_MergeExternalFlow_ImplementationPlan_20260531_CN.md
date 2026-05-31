# GraphWrite External User-Authored Graph P2 Merge External Flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `merge_external_flow`：在稳定 external exec boundary 上插入新的 BlueprintHelper-owned semantic body，并支持 preview、execute、Review v2 Reject 回滚。

**Architecture:** `merge_external_flow` 是独立策略和独立 service，不复用 `merge_owned_graph` 的 block-scoped resolver。外部 boundary resolver 只定位和验证 user-authored endpoint；新插入逻辑使用新生成的 owned `block_id`。Review evidence 同时记录外部 boundary relation 与 inserted owned block。

**Tech Stack:** TypeScript、Zod、UE 5.6 C++、BlueprintHelper Task Runtime、GraphWrite mutation coordinator、Review v2、Automation Tests。
---

## Contract

Agent-facing TaskSpec:

```ts
{
  task_type: 'edit_blueprint_graph',
  scope_policy: {
    graph_name: 'EventGraph',
    allow_modify_user_nodes: false,
    external_mutation_policy: {
      strategy: 'merge_external_flow',
      allowed_mutations: ['exec_boundary_link'],
    },
  },
  behavior: {
    graph_strategy: 'merge_external_flow',
    external_merges: [{
      kind: 'insert_external_flow',
      insert_strategy: 'append_after' | 'insert_between' | 'branch_fork',
      anchor: ExternalGraphAnchorV1,
      inserted: { body: BlueprintLogicSpec },
      sequence_order?: ['inserted_logic', 'original_successor'],
    }],
  },
}
```

Runtime adapter operation:

```text
merge_external_flow
```

## Task 1: Add TypeScript Schema and Compiler Lowering

**Files:**

- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.merge-external-flow.test.ts`

- [ ] **Step 1: Add external anchor schema**

```ts
const ExternalGraphAnchorSchema = z.object({
  schema: z.literal('BlueprintHelper.ExternalGraphAnchor.v1'),
  asset_path: z.string().min(1),
  graph_name: z.string().min(1),
  node_guid: z.string().min(1),
  node_class: z.string().min(1),
  pin_name: z.string().min(1),
  pin_direction: z.literal('output'),
  semantic_role: z.literal('exec_boundary'),
  fingerprint: z.string().min(1),
}).strict();
```

- [ ] **Step 2: Add external merge schema**

Use a separate `external_merges` field. Do not overload owned `merges`.

```ts
const GraphWriteExternalMergeSchema = z.object({
  kind: z.literal('insert_external_flow'),
  insert_strategy: z.enum(['append_after', 'insert_between', 'branch_fork']),
  anchor: ExternalGraphAnchorSchema,
  inserted: z.object({ body: BlueprintLogicSpecSchema }).passthrough(),
  sequence_order: z.array(z.enum(['inserted_logic', 'original_successor'])).optional(),
}).passthrough();
```

- [ ] **Step 3: Lower to explicit external IR**

```ts
{
  capability: 'graph_write',
  write: {
    strategy: 'external_graph_edit',
    ops: [{ op: 'insert_external_flow', ...merge }],
  },
  constraints: {
    allow_modify_user_nodes: false,
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'merge_external_flow',
      allowed_mutations: ['exec_boundary_link'],
    },
  },
}
```

- [ ] **Step 4: Add RED/GREEN compiler tests**

Cover:

- stable anchor required;
- raw `nodes[0]`, display name and ad hoc JSONPath rejected;
- `allow_modify_user_nodes=true` rejected;
- `branch_fork` requires sequence order;
- owned `merges` cannot appear with `merge_external_flow`;
- lowering emits `external_graph_edit`.

## Task 2: Add Registry Boundary and Route New Adapter Operation

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`

- [ ] **Step 1: Add registry instead of constructor fan-out**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteServiceRegistry
{
public:
	using FExecuteHandler = TFunction<FBlueprintHelperToolResultBase(const TSharedRef<FJsonObject>&)>;

	static bool IsKnownOperation(const FString& Operation);
	void RegisterHandler(const FString& Operation, FExecuteHandler Handler);
	bool HasHandler(const FString& Operation) const;
	FBlueprintHelperToolResultBase Execute(
		const FString& Operation,
		const TSharedRef<FJsonObject>& Payload) const;
};
```

Registry owns dispatch for:

```text
append_blueprint_graph
replace_blueprint_graph
patch_blueprint_graph
merge_blueprint_graph
merge_external_flow
```

Entry constructs services and registers handlers. Migrate GraphWrite bridge routes and TaskRuntime GraphWrite cluster to depend on one registry reference. Do not add a fifth direct service parameter and repeat that pattern in P3/P4.

`FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep` and `FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand` delegate static operation classification to `FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation`. `FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence` delegates operation-specific target construction to `FBlueprintHelperGraphWriteReviewEvidenceBuilder`.

- [ ] **Step 2: Add runtime lowering**

`TryBuildGraphWriteIrPayload` must branch by:

```cpp
Strategy == TEXT("external_graph_edit")
OpName == TEXT("insert_external_flow")
```

Then validate through `FBlueprintHelperGraphWriteDomainPolicy::ValidateExternalRequest`.

- [ ] **Step 3: Add dedicated adapter route**

Register `merge_external_flow` to the new service. Do not call `FBlueprintHelperMergeBlueprintGraphService`.

## Task 3: Implement External Boundary Relation and Merge Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBoundaryRelationTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.cpp`

- [ ] **Step 1: Define relation evidence**

```cpp
struct FBlueprintHelperExternalBoundaryRelation
{
	FBlueprintHelperExternalGraphAnchor Anchor;
	FString InsertedBlockId;
	TArray<FString> BeforeLinks;
	TArray<FString> AfterLinks;
};
```

- [ ] **Step 2: Add service API**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperMergeExternalFlowService
{
public:
	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;
};
```

- [ ] **Step 3: Implement preflight before mutation**

Preflight order:

1. parse external anchor;
2. validate external policy;
3. resolve exact source exec pin;
4. verify fingerprint;
5. compute existing successors;
6. validate insert strategy;
7. create dry-run relation and inserted block id;
8. only on execute, build semantic body and mutation intents.

- [ ] **Step 4: Reuse only neutral mutation coordinator**

Allowed intent kinds:

```cpp
EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBodyAfterPin
EBlueprintHelperGraphWriteMutationIntentKind::InsertSemanticBodyBetweenPins
EBlueprintHelperGraphWriteMutationIntentKind::BranchForkSemanticBody
```

Inserted nodes receive a new `InsertedBlockId`. Never reuse anchor block id and never mark the external anchor node owned.

## Task 4: Add Review v2 Evidence and Restore

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTargetKindRegistry.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp`

- [ ] **Step 1: Add handler**

```cpp
enum class EBlueprintHelperReviewTargetHandlerKind : uint8
{
	Unsupported,
	GraphNode,
	GraphBlock,
	GraphExternalBoundary,
	// existing kinds...
};
```

Register:

```cpp
{ TEXT("graph_external_boundary"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_boundary"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalBoundary }
```

- [ ] **Step 2: Build two atomic targets**

`FBlueprintHelperGraphWriteReviewEvidenceBuilder` emits:

```text
graph_external_boundary:<graph>:<anchor-guid>:<pin>
graph_block:<graph>:<inserted-block-id>
```

The boundary target before snapshot records exact original links. The block target records inserted owned nodes.

- [ ] **Step 3: Add restore route**

`GraphExternalBoundary` restore:

1. resolve external anchor by GUID and pin;
2. break only links represented by latest-after relation;
3. restore before-snapshot links;
4. never delete external nodes.

Keep Reject mismatch diagnostics non-blocking.

## Task 5: Add Tests

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperMergeExternalFlowTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`

- [ ] **Step 1: Register focused tests**

- `BlueprintHelper.GraphWrite.ExternalMerge.AppendAfterPreview`
- `BlueprintHelper.GraphWrite.ExternalMerge.AppendAfterExecute`
- `BlueprintHelper.GraphWrite.ExternalMerge.InsertBetweenPreservesSuccessor`
- `BlueprintHelper.GraphWrite.ExternalMerge.BranchForkUsesSequenceOrder`
- `BlueprintHelper.GraphWrite.ExternalMerge.RejectsStaleBoundary`
- `BlueprintHelper.GraphWrite.ExternalMerge.ExternalNodeMetadataUntouched`
- `BlueprintHelper.GraphWrite.ExternalMerge.RejectRestoresBoundaryAndRemovesOwnedBlock`
- `BlueprintHelper.TaskRuntime.GraphWrite.MergeExternalFlowRoutesDedicatedService`
- `BlueprintHelper.GraphWrite.Registry.DispatchesExistingAndExternalOperations`
- `BlueprintHelper.Review.ExternalBoundary.CurrentMismatchDoesNotBlockReject`

## Task 6: Verification

- [ ] **Step 1: Run focused suites separately**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ExternalMerge;Quit' -TestExit='Automation Test Queue Empty'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.Review.ExternalBoundary;Quit' -TestExit='Automation Test Queue Empty'
```

- [ ] **Step 2: Run common gate**

Use the master plan common verification gate.

## Manual Commit Checkpoint

Suggested commit message:

```text
新增内容：
1. 新增 GraphWrite merge_external_flow 外部边界插入能力
2. 新增 Review v2 graph_external_boundary 回滚目标
```
