# BlueprintHelper Current TODO (2026-05-06)

本页只记录当前主线待验证事项，不回写 `Resources/v0.3.6` 归档文档。

2026-05-09 清理: 全线测试执行入口已统一到 `Develop/Plan/BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`。本文件只保留主线 TODO 摘要，不再作为 smoke 执行清单。

## P1 GraphWrite closeout

- [x] `replace_owned_graph` full pipeline verified: Python compiler, Bridge preview, Bridge execute, compile, read-back.
- [x] Replace relink verified: preserved entry -> replacement body exec link is rebuilt; read-back shows 0 orphans.
- [x] Replace ownership metadata verified: replacement nodes carry grouped LogicJson `block_id` and can be targeted by Patch/Merge.
- [x] `patch_owned_graph` on Replace-created nodes verified with block-scoped `target_ref`.
- [x] `merge_owned_graph` verified for `insert_between + function_call`.
- [x] `merge_owned_graph` verified for `append_after + function_call`.
- [x] `merge_owned_graph` verified for `insert_between + custom_event_call`.
- [x] LogicJson grouped output includes `block_id`, `group_entry_node_path`, group-local `node_ref`, `pin_ref`, and `link_ref`.
- [x] Ownership metadata write target fixed: new BlueprintHelper-owned writes store machine fields such as `block_id` and `tx` in `FMetaData`, not `NodeComment`.
- [x] `NodeComment` `block_id` is legacy fallback only. The current slice does not delete old asset comments and does not change the TaskSpec / TaskPlan mainline.

## AgentGuide gaps found by Rerun 4

- [x] Document `blueprinthelper_preview_task` / `blueprinthelper_execute_task` wrapper: `{ "task_spec": { ... } }`.
- [x] Document Merge anchor field matrix: `append_after` uses `node_ref + pin_ref`; `insert_between` also requires `link_ref`; `link_ref` alone is invalid.
- [x] Document function call argument format: use `args` with structured literal values; do not use `params` or plain values.
- [x] Improve MCP/UE error detail for `append_after + custom_event_call`: 2026-05-07 smoke now returns diagnosable preview blocker `anchor_exec_pin_already_connected` with message/path.
- [x] Normalize empty task-level errors for GraphWrite execute failures: MCP task wrappers now ignore empty nested Bridge messages, and Bridge TaskRuntime preview/execute/get-journal entry points provide non-empty fallback messages.
- [x] Align runtime profile capability flags with verified GraphWrite Replace/Patch/Merge execution: source no longer reports `graph_write.merge = not_implemented`; UE smoke rerun still requires local build/editor reload.

## Remaining P1 validation gaps before or during P2

- [x] `branch_fork` merge strategy has a UE smoke fixture: early reruns confirmed TaskSpec -> TaskPlan -> Bridge -> UE preview; R4 later verified the `custom_event_call` execute/read-back path.
- [x] `branch_fork` execute implementation / error normalization source fix integrated: `owned_block_call` now resolves an existing BlueprintHelper-owned custom event block and creates a callable node before applying `branch_fork`.
- [x] R4 rerun verified `branch_fork + custom_event_call` execute/read-back after UE build / Editor reload: Sequence was created, inserted call and original successor were both reachable.
- [x] Source patch integrated for `append_new_owned_graph` multi-step execution: dependent `graph_write` append steps now reuse `blueprint_signature`-created CustomEvent entries instead of attempting to create duplicate entries.
- [ ] Add one same-graph `branch_fork + owned_block_call` execute/read-back smoke so Level 3 is not only covered by `custom_event_call`. Fold this into the unified P1/P2 disposable fixture run instead of treating it as a separate build-blocked item.
- [x] AssetFactory normalizes ordinary Blueprint fixture creation: TaskSpec `asset_type=blueprint` and `asset_type=Actor` now lower to `asset_type=blueprint_class` with `parent_class=Actor`; UE TaskRuntime and direct Bridge `create_asset` share the same parser.
- [x] Level 6/7 disposable fixtures for UMGWidget and DataTable execute smoke are now covered by Unified SmokeRun (3.2/3.3) and passed.
- [ ] Level 8 controlled failure fixture is still needed for TaskRunJournal partial failure / topology blocking smoke. Recovery notes expectation mismatch has been fixed in source/tests; rerun is tracked by Unified SmokeRun Ring 1 and Ring 4.
- [ ] Composite `create_blueprint_feature` execute fixture still needs a disposable asset target and should run in the same unified smoke/P2 fixture batch.
- [ ] Fix `create_blueprint_feature` preview empty-error anti-pattern discovered while attempting fixture creation for ClassSettings smoke.
- [ ] Non-BlueprintHelper-owned graph content still needs a stable read/write anchor contract.
- [ ] GUID remains expert/debug fallback only; do not move it back into the Agent-facing main write contract.
- [ ] Future migration/repair may clean legacy `NodeComment` ownership fragments after fallback behavior and smoke coverage are agreed.
- [x] TaskSpec field cleanup: Agent-facing `intent` removed; completed task journals now use orchestration-generated `generated_intent`; `feature_name` is display / journal label only and no longer drives graph-name recommendations.
- [x] TaskRunJournal result lookup: `blueprinthelper_get_task_result` now prefers UE `TaskRunJournal.v1`, falls back to the MCP in-process summary only when UE has no journal, and normalizes missing `generated_intent` on completed UE journals.

## Review / diagnostics boundary corrections

- [x] Review / ReviewPanel is user-side only. Do not add Agent-facing Review tools, ReviewPanel flows, or AgentGuide instructions that tell normal Agents to operate the Review UI.
- [ ] Aggregate TransactionJournalQuery with Review into one persisted Review transaction record model. This should consume UE write transactions and `task_run_id` grouping for the user-side Review panel; normal Agents only receive TaskRunJournal/task result summaries.
- [x] Remove the deferred bulk-reference path from the current Agent-facing and debug roadmap. Prefer targeted `logic_md` / `logic_json` reads by block or context slice instead of exporting full asset bodies to Agents.
- [x] Replace the old DebugExport / LargePayload active contract with the current DebugCase / DebugBundle boundary: failure-facing results expose summary `debug_case_ids[]`; ReviewRecord stores `debug_case_ids[]`; DebugBundle is a developer export artifact, not an Agent bulk-read path.
- [x] ReviewPanel + DebugCase / DebugBundle automation tail is covered for targeted cases: pending visible change query, Accept / Reject / RejectAll status propagation, Reject `needs_action` / `reject_failed` writing `debug_case_ids[]`, DebugBundle Review summary artifact, and no active `debug_export_refs` contract.
- [ ] ReviewPanel live Editor smoke remains: verify current UI with real pending records for row highlights, selected-row Accept / Reject, asset-root Reject cascade, Graph diff block drawing, Debug export, and no `debug_export_refs`.
- [x] ReviewPanel information-panel placement per asset kind is decided and implemented for the current v2 contract: Blueprint/WidgetBlueprint keep Graph as center workspace; Blueprint uses Components + MyBlueprint side panels; WidgetBlueprint uses WidgetTree + MyBlueprint side panels; DataTable/DataAsset/Structure use dedicated center workspace presenters; Details is auxiliary/property-owned only.

## 2026-05-07 execution note

- [x] MCP regression rerun passed after plan adjustment: `npm.cmd test` reported Python 42/42 OK and Node 128/128 pass.
- [x] AgentGuide frozen direct-tool leak scan stayed clean.
- [x] UnrealEditor was closed through `blueprint_close_editor(save_all=true)` before retrying Build.
- [x] UE `Build.bat` is no longer the current blocker. User-side UE build has passed; this TODO now tracks Editor / Automation verification rather than C++ compilation access.

## 2026-05-09 verification update from user FullTestLog

- [x] UE build passed on the user side.
- [x] Local recheck on 2026-05-09: full workspace `Build.bat` compile now passes (`MrStoneEditor Win64 Development ... -NoHotReload`).
- [x] Local recheck on 2026-05-09: plugin package compile now passes via `RunUAT BuildPlugin` and produces `PluginOut/BlueprintHelper` with no compile errors.
- [ ] Full Automation rerun is pending after the grouped failure fixes. The former failing set was ObjectFirst export JSON shape, BlueprintVariable localized category expectation, TaskRunJournal recovery notes, ObjectProperty invalid value dry-run, AssetFactory Structure/DataTable creation, and Signature override create-if-missing; rerun is now tracked by Unified SmokeRun Ring 1.
- [x] Review / Debug targeted Automation evidence exists in the log for `RejectNeedsActionCreatesDebugCase`, `RejectFailedCreatesDebugCase`, `PersistsDebugCaseIds`, `LoadPendingVisibleChangesUsesRecordQuery`, and `BundleSummaryExportIncludesReviewSummaryArtifact`.
- [x] ReviewPanel now has disposable/manual pending visible content in user smoke. The old `sourceGraph="<none>" previewGraph="EdGraph_0" previewNodes=0` note was the earlier empty-pending state and is no longer the active blocker.
- [ ] ReviewPanel live Editor smoke still needs final confirmation on the current records: asset-root Reject deletes the created asset and clears same-asset child reviews only after root success; Graph `ReplaceBlueprintGraph` records draw a center diff block with node guid or recorded bounds.
- [x] CleanupOwnership TaskRuntime cluster test was using the old `cleanup_ownership` capability name; the active TaskPlan capability is `graph_cleanup_ownership`. Test expectation has been corrected and targeted UE Automation rerun passed for `ResolvesLoweredSteps` and `FinalBatchClustersRecognizeOnlyOwnedSteps`.

## P2 entry candidates

- [x] Function/Event Signature Management first slice source integrated: internal `FBlueprintHelperSignatureService`, Signature DTO folder, Runtime delegation, `ensure_function` inputs/outputs forwarding, `ensure_custom_event` entry creation first slice, `ensure_event_dispatcher` declaration path, `ensure_override_event` blocked preflight path, `remove_signature` TaskPlan preflight/blocked path, and UE automation coverage added.
- [x] DataAsset/ObjectProperty first slice source integrated: `object_property/property_edit` TaskSpec schema, TS/Python compiler, TaskPlan adapter, true dry-run service façade, and TaskRuntime dispatch added.
- [x] Cleanup/Rollback/Ownership first slice source integrated: `graph_cleanup_ownership/owned_block_lifecycle` TaskSpec schema, TS/Python compiler, TaskPlan adapter, and TaskRuntime dispatch to cleanup / convert / rollback services added.
- [ ] P2 unified verification pending: do not count Signature / ObjectProperty / CleanupOwnership as smoke-verified until Unified SmokeRun Ring 7 passes. Build has passed and grouped failure fixes are integrated; Automation rerun is pending.
- [x] Function/Event Signature boundary policies fixed: interface function vs interface event lowering, event dispatcher `signature_mismatch_policy=block`, default override/native `execute_policy=blocked_preflight`, explicit override/native `execute_policy=create_if_missing` source path, remove-signature `execute_policy=blocked_preflight` with reference-context requirement, and `custom_event_definition` split into Signature declaration plus GraphWrite body rewrite.
- [ ] Function/Event Signature remaining: real remove execution after reference-analysis cleanup policy, dispatcher signature migration strategy beyond block, and grouped UE build / automation / smoke verification for the new Signature source paths.
- [x] DebugCase / DebugBundle developer diagnostics first contract is the current direction; old DebugExport LargePayload is deprecated as an active TODO.
- [x] Review reject debug linkage and DebugBundle Review summary export boundary have targeted automation evidence.
- [ ] Remaining diagnostics verification: compile/post-operation failure debug surfacing and retention / cleanup policy.
- [ ] DependencyAnalysis / ReferenceContextPack integration into high-risk preview blockers.
