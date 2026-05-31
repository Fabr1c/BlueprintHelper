# BlueprintHelper Legacy Residuals Source Audit 2026-05-31

> 文档类型：Gap / source audit result
> 范围：`BlueprintHelper/Source`、`AgentFaceService/cli/src`、`AgentFaceService/task-core/src`、`AgentFaceService/agent-guide/Templates`
> 结论日期：2026-05-31
> 审查方式：并行只读子任务 + 源码修复 + 最终只读复审；未执行 git add / commit / push。

## 1. 当前结论

本次审查最初确认两个 active legacy residual。按 `BlueprintHelper_LegacyResiduals_RemovalImplementationPlan_20260531_CN.md` 执行后，两个 Gap 均已关闭：

1. **已关闭：`set_node_position` / `node_position` 旧 GraphWrite layout 通道不再是 agent-facing / runtime-mutating path。**
2. **已关闭：Review Reject no-options 旧占位分支不再返回 first-slice placeholder，Reject 语义对齐 Review v2 evidence-before-snapshot。**

同时确认以下项本次不作为 blocking Gap：

- `bh bridge call` 不是无限制 Bridge 透传；当前存在窄白名单，且不包含 `read_blueprint_logic_json` / `export_logic`。
- `blueprint_open_editor` / `blueprint_close_editor` 是 blocked compat guard，不是可执行旧 lifecycle path。
- 旧 `blueprint_*` bridge command map 残留存在，但当前 normal CLI / tool registry 不暴露这些名字。
- `legacy_parent_class_field`、legacy append payload 错误、FunctionChain legacy schema normalization 属于迁移拒绝或结果归一化 guard。
- `read_task_context` / `TaskContextPack` 在本次审查范围内未命中 active source path。

## 2. Closed Gaps

### Gap 1. Deprecated GraphWrite layout path is still active end-to-end

状态：CLOSED，2026-05-31

严重度：High

旧路径：

```text
TaskSpec / TS schema
-> task-core compiler
-> TaskPlan patch op
-> UE TaskRuntime
-> GraphWrite PatchBlueprintGraphService
-> direct NodePosX / NodePosY mutation
```

原始证据：

| Layer | File | Evidence |
|---|---|---|
| TS schema | `AgentFaceService/task-core/src/task/schema/task-schemas.ts:411-483` | `options.preserve_layout` and `kind: 'set_node_position'` remain accepted. |
| TS contract | `AgentFaceService/task-core/src/task/schema/task-contract.ts:141-153`, `:629-641`, `:703` | `preserve_layout`, `set_node_position`, and `node_position` are still advertised as supported / runtime-supported. |
| TS compiler | `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1589-1594`, `:3436`, `:3512-3517` | compiler allows `set_node_position`, derives `node_position`, and builds patch payload. |
| TS regression test | `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts:875-907` | regression test still asserts compiled `set_node_position` op. |
| UE runtime lowering | `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:2601-2605`, `:3046-3049` | maps `set_node_position` into `patch_blueprint_graph`. |
| UE patch types | `BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h:18-19`, `:34`, `:49`, `:65-66`, `:81`, `:96` | parses / emits `node_position` and `set_node_position`. |
| UE mutation | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp:635-636`, `:747-772` | calls `ApplySetNodePosition` and mutates node position. |
| Review bypass signal | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp:386-392` | `SetNodePosition` is special-cased out of review recording. |

Why this is a real residual:

- Existing comments already label this path `DEPRECATED_LAYOUT`.
- The path is not only parser noise; it remains schema-visible, compiler-visible, runtime-routed, and mutates UE graph state.
- Layout ownership is expected to sit behind GraphLayout, not GraphWrite patch operations.

Correction from first-pass audit:

- This is broader than a C++ residual. TS schema / contract / compiler still surface it.
- `preserve_layout` in `BlueprintHelperReplaceBlueprintGraphService` is lower confidence than `set_node_position`; current evidence only shows parse / store, not an observed read / effect after storage.

Closure implementation:

1. Removed `set_node_position` / `node_position` / `preserve_layout` exposure from TaskSpec schema, public task contract, compiler lowering, and fixtures.
2. Replaced the old success regression with fail-fast coverage proving deprecated layout patches are rejected before GraphWrite lowering.
3. Removed UE TaskRuntime routing, PatchGraph enum parsing/emission, PatchBlueprintGraphService mutation, and the review-recording bypass for `SetNodePosition`.
4. Added an architecture boundary test that scans the runtime / GraphWrite patch service files and fails if the deprecated layout mutation symbols reappear.

Closure evidence captured:

- `npm.cmd run build` PASS.
- `npm.cmd run test:node` PASS, `302/302`.
- UE 5.6 `TemplateEditor Win64 Development` build PASS.
- Focused production residual scan over task-core production files and GraphWrite runtime / patch service files returns no active `set_node_position` / `node_position` / `preserve_layout` / `DEPRECATED_LAYOUT` path.
- Remaining `set_node_position` source hits are negative tests / rejection guards, not accepted schema, compiler lowering, or UE mutation support.

### Gap 2. Review Reject still has reachable first-slice placeholder fallback

状态：CLOSED，2026-05-31

严重度：Medium

旧路径：

```text
Bridge apply_review_action reject
-> FBlueprintHelperReviewActionService::RejectVisibleChange(Change)
-> ResolvePersistedReviewTargetMatches(Change)
-> no matches
-> NeedsAction: "Archive-baseline rollback backend is not wired in the first Review UI slice."
```

原始证据：

| Layer | File | Evidence |
|---|---|---|
| Bridge route | `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:1378-1380` | reject action calls `FBlueprintHelperReviewActionService::RejectVisibleChange(*MatchedChange)` with no injected options. |
| Review action fallback | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp:100-131` | no-options overload returns the first-slice archive-baseline placeholder when persisted target matches are absent. |
| Real default dispatcher exists elsewhere | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp:135-163` | options overload can fall through to `RejectVisibleChangeWithDefaultDispatcher`. |
| Injection predicate | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.cpp:118-120` | injected options are defined by `CurrentHashesByTargetKey.Num() > 0`. |
| Default reject implementation | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewRejectService.cpp:57-121` | real default dispatcher records hash guard diagnostics instead of blocking solely on current/latest mismatch. |

Why this is a real residual:

- The placeholder is a runtime result, not a comment.
- Bridge reject reaches the no-options overload.
- The message explicitly references an old "first Review UI slice" implementation boundary.

Correction from first-pass audit:

- This is not merely a harmless overload left behind for tests.
- It is reachable when the persisted target resolver returns no match.

Closure implementation:

1. Replaced the no-options unmatched fallback result with precise `persisted_review_targets_not_found` `NeedsAction`.
2. Removed the runtime placeholder message `Archive-baseline rollback backend is not wired in the first Review UI slice.` from active source.
3. Updated Review tests so unmatched targets assert the precise diagnostic and matching snapshot targets exercise the real Review v2 restore path.
4. Updated the current-state mismatch test to match the hard rule: drift is recorded as hash-guard diagnostics; it does not block Reject when evidence-before-snapshot restore is available.
5. Updated the Review panel command-service null-service Reject fallback to return `review_action_service_unavailable` without archive-baseline rollback mode.
6. Migrated old fake `/Game/BP_Door` Reject purge fixtures to Review v2 node-comment snapshot targets so persisted Reject tests exercise real evidence-before-snapshot restore and purge semantics.

Closure evidence captured:

- `rg -n "first Review UI slice|Archive-baseline rollback backend is not wired|Reject requires archive-baseline rollback service" BlueprintHelper/Source -g "*.h" -g "*.cpp"` returns no hits.
- `rg -n 'Reject requires archive-baseline rollback service|RollbackMode\s*=\s*TEXT\("archive_baseline"\)' BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp -g "*.cpp"` returns no hits.
- UE 5.6 `TemplateEditor Win64 Development` build PASS.
- Targeted UE automation PASS:
  - `BlueprintHelper.Review.Action.RejectReportsPersistedTargetsNotFound`
  - `BlueprintHelper.Review.Action.RejectSucceedsWithMatchingHashAndSnapshotBaseline`
  - `BlueprintHelper.Review.Action.RejectBlocksCurrentStateMismatch`
- UE automation `BlueprintHelper.Review.PanelCommand` PASS, `2` tests.
- UE automation `BlueprintHelper.Review.Action.Reject` PASS; it found `9` tests and exited with code `0`.
- `bh.cmd bridge ping --select status,summary` reported `bridge_unavailable`, so Bridge live ping was not used as completion evidence; command-line UE automation supplied the Review coverage.

## 3. Non-Blocking Residuals / Noise

### 3.1 `bh bridge call`

Status: not a blocking legacy residual in this audit.

Evidence:

- `AgentFaceService/cli/src/cli/run.ts:71-79` defines `READ_ONLY_BRIDGE_COMMANDS`.
- `AgentFaceService/cli/src/cli/run.ts:174-180` rejects commands not in the allowlist.
- `AgentFaceService/cli/src/cli/run.ts:373` parses `bh bridge call --command`.
- `AgentFaceService/cli/src/cli/help.ts:306-314`, `:371` documents it as a narrow diagnostic entry.

Conclusion:

- This is not an unrestricted old bridge tunnel.
- `read_blueprint_logic_json` and `export_logic` are not in the allowlist.

### 3.2 Blocked lifecycle compatibility aliases

Status: not an active legacy lifecycle path.

Evidence:

- `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts:30-31` marks `blueprint_open_editor` / `blueprint_close_editor` as compat-only.
- `AgentFaceService/task-core/src/tool-surface/local/local-tool-dispatcher.ts:20-39` always returns `lifecycle_mcp_required`.
- `AgentFaceService/cli/src/cli/run.ts:315-317` maps grouped `open_editor` / `close_editor` commands into those blocked names.
- `AgentFaceService/cli/src/cli/help.ts:388-390`, `:469-470` points agents to MCP lifecycle tools.

Conclusion:

- The aliases remain visible, but they fail closed.
- They do not execute editor lifecycle work through CLI.

### 3.3 Frozen / old `blueprint_*` bridge command map

Status: source residual, but not normal agent-facing CLI/tool-registry surface.

Evidence:

- `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-command-map.ts:12-56` still contains old `blueprint_*` mappings.
- `AgentFaceService/task-core/src/tool-surface/bridge/generic-bridge-tool-handler.ts:17-22` still has special normalization for `blueprint_get_logic` and `blueprint_get_logic_json`.
- `AgentFaceService/task-core/src/tool-surface/registry/tool-metas.ts:11-31` omits those old names from the normal registry.
- `AgentFaceService/cli/src/cli/run.ts:345` and `AgentFaceService/cli/src/cli/tool-command.ts:30-39` only invoke names returned by `getBlueprintHelperTool`.
- Final review reported MCP-side frozen registrations are intentionally suppressed in `AgentFaceService/mcp/src/mcp/tools/register-tools.ts`.

Conclusion:

- The command map is still cleanup debt.
- It should not be counted as a currently exposed default agent-facing surface unless a separate MCP registration path proves exposure.

### 3.4 Migration guards and normalization

Status: acceptable guard behavior.

Evidence:

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:860-868` rejects `behavior.parent_class` with `legacy_parent_class_field`.
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1145-1151` rejects legacy append nodes / links payloads in favor of `logic_spec`.
- `AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-result-utils.ts:7-8`, `:25-35` normalizes a legacy FunctionChain schema label.

Conclusion:

- These are migration boundaries or result normalization, not old execution support.

## 4. Negative Search Results

The following searches were clean for active source paths in the requested scope:

```powershell
rg -n "read_task_context|TaskContextPack|frozen|Frozen|direct MCP|legacy tool|LegacyTool|deprecated tool|DeprecatedTool" AgentFaceService/cli/src AgentFaceService/task-core/src BlueprintHelper/Source -g "*.ts" -g "*.h" -g "*.cpp"
```

Result:

- No active `read_task_context` / `TaskContextPack` source path in the checked roots.
- The only narrow-scan `legacy tool field` hit was test-only wording in `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp:727`.

Additional focused check:

```powershell
rg -n "blueprint_get_logic|blueprint_get_logic_json|blueprint_get_|blueprint_[a-z_]+" AgentFaceService/task-core/src/tool-surface AgentFaceService/cli/src -g "*.ts"
```

Result:

- Old `blueprint_*` names remain in bridge command map and local blocked lifecycle aliases.
- Normal CLI / tool registry does not expose most frozen old bridge tool names through `toolMetas`.

## 5. Remaining Follow-Up

1. Consider a cleanup-only pass for frozen bridge command map residuals if MCP registration paths confirm they are no longer needed.
2. Keep any future graph node placement behavior behind GraphLayout service / coordinator boundaries, not GraphWrite patch operations.

## 6. Manual Commit Scope

The implementation pass changed these task-owned files:

```text
AgentFaceService/task-core/src/task/compiler/task-compiler.ts
AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts
AgentFaceService/task-core/src/task/schema/task-contract.ts
AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts
AgentFaceService/task-core/src/task/schema/task-schemas.ts
AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts
AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts
AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts
BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_LegacyResiduals_RemovalImplementationPlan_20260531_CN.md
BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp
BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h
BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h
```

Do not include unrelated existing worktree changes in the commit.
