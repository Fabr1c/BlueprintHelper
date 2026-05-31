# GraphWrite External User-Authored Graph P3 Patch External Graph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增低风险 `patch_external_graph`：只允许修改 user-authored node 的 pin default 或 node comment，并支持 stale check、preview、execute 和 Review v2 Reject。

**Architecture:** 新增独立 external patch service。P3 不开放 link patch、layout patch、node create/delete、ownership metadata 修改。每个 patch 必须携带 external node anchor、expected fingerprint 和 `expected_old_state`。Review v2 使用字段级 `graph_external_node` handler，不复用会断开 pin links 的现有 `GraphNode` 恢复逻辑。

**Tech Stack:** TypeScript、Zod、UE 5.6 C++、BlueprintHelper Task Runtime、Review v2、Automation Tests。
---

## Contract

```ts
{
  graph_strategy: 'patch_external_graph',
  external_patches: [{
    kind: 'set_external_pin_default' | 'set_external_node_comment',
    anchor: ExternalGraphAnchorV1,
    value: unknown,
    expected_old_state: Record<string, unknown>,
  }],
}
```

P3 allowlist:

```ts
['pin_default', 'node_comment']
```

Explicitly rejected:

```text
set_node_position
connect_pins
disconnect_pins
replace_pin_connection
node_create
node_delete
ownership_metadata
```

## Task 1: Add TypeScript Contract

**Files:**

- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.patch-external-graph.test.ts`

- [ ] **Step 1: Add separate schema**

```ts
const GraphWriteExternalPatchSchema = z.object({
  kind: z.enum(['set_external_pin_default', 'set_external_node_comment']),
  anchor: ExternalGraphAnchorSchema,
  value: z.unknown(),
  expected_old_state: z.record(z.unknown()),
}).strict();
```

- [ ] **Step 2: Lower exact policy**

```ts
constraints: {
  allow_modify_user_nodes: false,
  ownership_scope: 'external_user_authored',
  external_mutation_policy: {
    strategy: 'patch_external_graph',
    allowed_mutations: ['pin_default', 'node_comment'],
  },
}
```

- [ ] **Step 3: Add rejection tests**

Assert schema/compiler rejects link mutation, layout mutation, missing fingerprint, missing `expected_old_state`, owned patch fields mixed into external strategy, and broad `allow_modify_user_nodes=true`.

## Task 2: Implement Dedicated Patch Service

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalGraphService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/BlueprintHelper.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperRequestValidator.cpp`

- [ ] **Step 1: Add service API**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperPatchExternalGraphService
{
public:
	FBlueprintHelperToolResultBase Execute(const TSharedRef<FJsonObject>& Payload) const;
};
```

- [ ] **Step 2: Implement preflight**

Required order:

1. validate external policy;
2. resolve anchor;
3. verify fingerprint;
4. compare `expected_old_state`;
5. validate exact patch allowlist;
6. build dry-run result;
7. execute one field mutation inside transaction.

Use mutation coordinator for `SetPinDefault`. Use a focused comment mutation helper inside the external patch service for comment changes. Do not add link or layout branches.

- [ ] **Step 3: Register `patch_external_graph`**

Add adapter operation to `FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation`. Entry constructs `FBlueprintHelperPatchExternalGraphService` and registers its handler. Bridge route and TaskRuntime cluster already consume the registry from P2; do not add new direct service constructor parameters.

## Task 3: Add Field-Level Review v2 Handler

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTargetKindRegistry.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.cpp`

- [ ] **Step 1: Register handler**

```cpp
GraphExternalNode
```

with:

```cpp
{ TEXT("graph_external_node"), EBlueprintHelperReviewSurface::Graph, false, TEXT("graph_external_node"), EBlueprintHelperReviewTargetHandlerKind::GraphExternalNode }
```

- [ ] **Step 2: Snapshot and restore only selected field**

Snapshot JSON:

```json
{
  "target_kind": "graph_external_node",
  "node_guid": "...",
  "field_kind": "pin_default",
  "pin_name": "Value",
  "value": "before"
}
```

Reject restores only the selected comment or pin default. It must not call existing GraphNode restore because that path breaks and rebuilds pin links.

## Task 4: Add Tests

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperPatchExternalGraphTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`

- [ ] **Step 1: Register focused tests**

- `BlueprintHelper.GraphWrite.ExternalPatch.PinDefaultPreview`
- `BlueprintHelper.GraphWrite.ExternalPatch.PinDefaultExecute`
- `BlueprintHelper.GraphWrite.ExternalPatch.NodeCommentExecute`
- `BlueprintHelper.GraphWrite.ExternalPatch.RejectsStaleAnchor`
- `BlueprintHelper.GraphWrite.ExternalPatch.RejectsExpectedOldStateMismatch`
- `BlueprintHelper.GraphWrite.ExternalPatch.RejectsLayoutMutation`
- `BlueprintHelper.GraphWrite.ExternalPatch.RejectsLinkMutation`
- `BlueprintHelper.GraphWrite.ExternalPatch.DoesNotWriteOwnershipMetadata`
- `BlueprintHelper.Review.ExternalNode.RejectRestoresSelectedFieldOnly`

## Task 5: Verification

- [ ] **Step 1: Run focused suites**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ExternalPatch;Quit' -TestExit='Automation Test Queue Empty'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.Review.ExternalNode;Quit' -TestExit='Automation Test Queue Empty'
```

- [ ] **Step 2: Run common gate**

Use the master plan common verification gate.

## Manual Commit Checkpoint

Suggested commit message:

```text
新增内容：
1. 新增 GraphWrite patch_external_graph 受控字段修改能力
2. 新增 Review v2 graph_external_node 字段级回滚目标
```
