# GraphWrite External User-Authored Graph Master Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不扩大隐式写权限、不复用 legacy anchor fallback 的前提下，为 GraphWrite 增加用户 authored graph 的受控写入能力：`merge_external_flow`、`patch_external_graph`、`replace_external_body`。

**Architecture:** 保留 `TaskSpec` 作为唯一共享外层上下文。新增 external anchor、external domain policy、boundary relation 与 Review v2 target handler。owned graph 和 external graph 共享中性的 mutation coordinator，但使用不同 resolver、policy validator、service 和 Review evidence。任何外部修改都必须携带稳定 anchor、expected fingerprint、显式 mutation allowlist 和 dry-run 结果。

**Tech Stack:** TypeScript、Zod、UE 5.6 C++、Blueprint Graph API、BlueprintHelper Task Runtime、Review v2、Automation Tests、`Build.bat`、`UnrealEditor-Cmd.exe`。
---

## Source Assessment

本计划基于：

- `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_UserAuthoredAnchor_ArchitectureAssessment_20260531_CN.md`
- `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`

## Hard Decisions

1. `allow_modify_user_nodes=true` 不作为通用授权开关。编译器继续拒绝这个宽泛开关。外部写权限必须通过 `scope_policy.external_mutation_policy` 的策略枚举和 allowlist 精确表达。
2. `block_id` 只表示 BlueprintHelper-owned block。外部 user-authored node 不伪装成 owned block，不写入 ownership metadata。
3. external anchor 是独立契约，不使用 raw LogicJson 数组下标、显示名、ad hoc JSONPath 或 comment fallback 作为稳定定位。
4. existing `FBlueprintHelperGraphWriteMutationCoordinator` 继续作为中性 mutation executor。外部服务只能在 policy validator 和 resolver 成功后构造 intent。
5. Review v2 增加 `graph_external_boundary`、`graph_external_node`、`graph_external_body` 三类 target handler。不得把外部回滚偷挂到现有 `graph_link -> GraphNode` handler。
6. Reject 仍以 evidence before snapshot 为回滚目标。`current != latest after` 只进入 diagnostics / DebugBundle，不阻塞 Reject。
7. P4 只处理显式 body scope：`custom_event_body`、`event_body`、`function_body`。任意 whole-graph replace 不在本计划范围内。
8. 所有新增 C++ class 使用独立 `.h/.cpp` 文件。只有 enum 和纯数据 struct 可以合并到 types header。

## External Contract

```ts
type ExternalGraphAnchorV1 = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1';
  asset_path: string;
  graph_name: string;
  node_guid: string;
  node_class: string;
  pin_name?: string;
  pin_direction?: 'input' | 'output';
  semantic_role: 'exec_boundary' | 'node' | 'body_entry';
  fingerprint: string;
};

type ExternalMutationPolicy = {
  strategy: 'merge_external_flow' | 'patch_external_graph' | 'replace_external_body';
  allowed_mutations: string[];
};
```

Task compiler lower 后必须显式产生：

```ts
{
  write: { strategy: 'external_graph_edit' },
  constraints: {
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'merge_external_flow',
      allowed_mutations: ['exec_boundary_link'],
    },
  },
}
```

## Phase Dependency

| Phase | Deliverable | Depends On | Independent Acceptance |
| --- | --- | --- | --- |
| P0 | owned-only policy closure and legacy fallback removal | current mainline | owned flows remain green; external edits still rejected |
| P1 | read-only external anchor generation and stale validation | P0 | read context emits stable anchors; no graph mutation |
| P2 | `merge_external_flow` | P1 | external boundary merge preview, execute, Reject rollback |
| P3 | `patch_external_graph` | P2 | allowlisted pin default and comment patch only |
| P4 | `replace_external_body` | P3 | explicit body rewrite with dependents analysis and full rollback |

## Implementation Status 2026-05-31

| Phase | Status | Evidence |
| --- | --- | --- |
| P0 | DONE | Owned-only policy is enforced; implicit user-authored adoption paths are rejected by tests. |
| P1 | DONE | External anchor read/resolution and stale validation coverage is present in `BlueprintHelperExternalGraphAnchorTests.cpp`. |
| P2 | DONE | `merge_external_flow` compiler/runtime path and external boundary Review rollback are implemented. |
| P3 | DONE | `patch_external_graph` compiler/runtime path supports allowlisted pin default and comment edits only. |
| P4 | DONE | `replace_external_body` compiler/runtime path, body snapshot, dependents analysis, Review evidence, and reject restore are implemented. |

Final verification run:

- `npm.cmd --prefix AgentFaceService/task-core run build` -> pass.
- `npm.cmd --prefix AgentFaceService/task-core run test:node` -> 280 passed, 0 failed.
- `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReloadFromIDE` -> pass.
- `Automation RunTests BlueprintHelper.GraphWrite` -> 319 succeeded, 22 succeeded with warnings, 0 failed.
- `Automation RunTests BlueprintHelper.Review.ExternalBody` -> 1 succeeded, 0 failed.
- `Automation RunTests BlueprintHelper.TaskRuntime.GraphWrite.ReplaceExternalBody` -> 1 succeeded, 0 failed.
- `Automation RunTests BlueprintHelper.Router.Cluster` -> 7 succeeded, 0 failed.
- `Automation RunTests BlueprintHelper.TaskRuntime.Cluster` -> 6 succeeded, 0 failed.

## Plan Documents

- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_P0_OwnedSafetyClosure_ImplementationPlan_20260531_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_P1_ExternalAnchorRead_ImplementationPlan_20260531_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_P2_MergeExternalFlow_ImplementationPlan_20260531_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_P3_PatchExternalGraph_ImplementationPlan_20260531_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ExternalUserAuthoredGraph_P4_ReplaceExternalBody_ImplementationPlan_20260531_CN.md`

## Shared File Ownership

新增 shared boundaries：

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Review/BlueprintHelperGraphWriteReviewEvidenceBuilder.cpp`

`FBlueprintHelperGraphWriteServiceRegistry` 在 P2 建立统一 adapter-operation dispatcher。Bridge route 和 TaskRuntime cluster 只依赖 registry；P3/P4 通过 registry 增加 operation，不再扩散新的 service 构造参数。

必须删除或隔离的 legacy 路径：

- `FBlueprintHelperGraphWriteBlockScopedResolver::NodeCommentMentionsBlockId`
- block-scoped resolver 中无 `block_id` 时回退到 `FBlueprintHelperLogicJsonPathService`
- replace owned flow 将 user-authored entry 隐式标记为 BlueprintHelper-owned 的路径

## Shared Review v2 Rules

| Target Kind | Handler Kind | Before Snapshot | Reject Restore |
| --- | --- | --- | --- |
| `graph_block` | existing `GraphBlock` | inserted BlueprintHelper-owned nodes | remove inserted block or restore prior owned block |
| `graph_external_boundary` | new `GraphExternalBoundary` | exact external exec links before mutation | restore original links without deleting external nodes |
| `graph_external_node` | new `GraphExternalNode` | selected node comment or selected pin default | restore only allowlisted field |
| `graph_external_body` | new `GraphExternalBody` | preserved entry plus body restore text and links | remove replacement body and import evidence-before body |

P2 Reject atomic restore order:

1. restore `graph_external_boundary`;
2. restore/remove inserted `graph_block`;
3. report diagnostics if current state diverged from latest after snapshot.

P4 Reject atomic restore order:

1. validate target body entry;
2. remove replacement body nodes only;
3. import evidence-before body snapshot;
4. reconnect preserved external entry;
5. report diagnostics without blocking rollback.

## Common Verification Gate

每个阶段完成后按顺序运行。`task-core` build 与 node tests 不并行。

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty'
git diff --check
git status --short
```

Expected:

- TypeScript build exits `0`.
- node tests exit `0`.
- UE 5.6 `TemplateEditor` build exits `0`.
- full `BlueprintHelper.GraphWrite` suite reports zero failed tests.
- `git diff --check` prints no diagnostics.
- `git status --short` contains only intended phase files plus pre-existing user changes.

## Worker Handoff Rule

Workers must not execute `git add`, `git commit`, or `git push`. At the end of each phase:

- report modified files;
- report focused and full gate evidence;
- provide a suggested commit message;
- stop for manual user commit.
