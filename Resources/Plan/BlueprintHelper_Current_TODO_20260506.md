# BlueprintHelper Current TODO (2026-05-06)

本页只记录当前主线待验证事项，不回写 `Resources/v0.3.6` 归档文档。

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

- [x] `branch_fork` merge strategy has a UE smoke fixture: preview passes through TaskSpec -> TaskPlan -> Bridge -> UE preview; execute currently fails with an empty error and no task_run_id.
- [x] `branch_fork` execute implementation / error normalization source fix integrated: `owned_block_call` now resolves an existing BlueprintHelper-owned custom event block and creates a callable node before applying `branch_fork`.
- [ ] Rerun `branch_fork` execute smoke after local UE build / Editor reload; Codex build attempt is blocked before plugin compilation by project/sibling-plugin `Intermediate` write restrictions.
- [x] AssetFactory normalizes ordinary Blueprint fixture creation: TaskSpec `asset_type=blueprint` and `asset_type=Actor` now lower to `asset_type=blueprint_class` with `parent_class=Actor`; UE TaskRuntime and direct Bridge `create_asset` share the same parser.
- [ ] Level 6 disposable fixtures are still needed for ClassSettings, UMGWidget, and DataTable execute smoke.
- [ ] Level 8 controlled failure fixture is still needed for TaskRunJournal partial failure / topology blocking smoke.
- [ ] Composite `create_blueprint_feature` execute fixture still needs a disposable asset target.
- [ ] Fix `create_blueprint_feature` preview empty-error anti-pattern discovered while attempting fixture creation for ClassSettings smoke.
- [ ] Non-BlueprintHelper-owned graph content still needs a stable read/write anchor contract.
- [ ] GUID remains expert/debug fallback only; do not move it back into the Agent-facing main write contract.
- [ ] Future migration/repair may clean legacy `NodeComment` ownership fragments after fallback behavior and smoke coverage are agreed.
- [x] TaskSpec field cleanup: Agent-facing `intent` removed; completed task journals now use orchestration-generated `generated_intent`; `feature_name` is display / journal label only and no longer drives graph-name recommendations.
- [x] TaskRunJournal result lookup: `blueprinthelper_get_task_result` now prefers UE `TaskRunJournal.v1`, falls back to the MCP in-process summary only when UE has no journal, and normalizes missing `generated_intent` on completed UE journals.

## P2 entry candidates

- [x] Function/Event Signature Management first slice source integrated: internal `FBlueprintHelperSignatureService`, Signature DTO folder, Runtime delegation, `ensure_function` inputs/outputs forwarding, `ensure_custom_event` entry creation first slice, `ensure_event_dispatcher` declaration path, `ensure_override_event` blocked preflight path, `remove_signature` TaskPlan preflight/blocked path, and UE automation coverage added.
- [x] DataAsset/ObjectProperty first slice source integrated: `object_property/property_edit` TaskSpec schema, TS/Python compiler, TaskPlan adapter, true dry-run service façade, and TaskRuntime dispatch added.
- [x] Cleanup/Rollback/Ownership first slice source integrated: `graph_cleanup_ownership/owned_block_lifecycle` TaskSpec schema, TS/Python compiler, TaskPlan adapter, and TaskRuntime dispatch to cleanup / convert / rollback services added.
- [ ] P2 unified verification pending: do not count Signature / ObjectProperty / CleanupOwnership as smoke-verified until the next grouped build + automation + disposable fixture run.
- [x] Function/Event Signature boundary policies fixed: interface function vs interface event lowering, event dispatcher `signature_mismatch_policy=block`, override/native `execute_policy=blocked_preflight`, and remove-signature `execute_policy=blocked_preflight` with reference-context requirement.
- [ ] Function/Event Signature remaining: full custom event body split, real override/native event creation policy, real remove execution after reference-analysis cleanup policy, and dispatcher signature migration strategy beyond block.
- [ ] DebugExport/LargePayload service and task debug payload references.
- [ ] DependencyAnalysis / ReferenceContextPack integration into high-risk preview blockers.
