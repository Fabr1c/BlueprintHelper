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
- [x] BlueprintHelper-owned node comment no longer writes the obsolete `tool` field; it keeps `block_id` and `tx`.

## AgentGuide gaps found by Rerun 4

- [x] Document `blueprinthelper_preview_task` / `blueprinthelper_execute_task` wrapper: `{ "task_spec": { ... } }`.
- [x] Document Merge anchor field matrix: `append_after` uses `node_ref + pin_ref`; `insert_between` also requires `link_ref`; `link_ref` alone is invalid.
- [x] Document function call argument format: use `args` with structured literal values; do not use `params` or plain values.
- [ ] Improve MCP/UE error detail for `append_after + custom_event_call`, which currently returns an empty preview error.
- [ ] Align runtime profile capability flags with verified GraphWrite Replace/Patch/Merge execution so profile no longer reports stale `not_implemented` facts.

## Remaining P1 validation gaps before or during P2

- [ ] `branch_fork` merge strategy needs a UE smoke fixture.
- [ ] Level 6 disposable fixtures are still needed for ClassSettings, UMGWidget, and DataTable execute smoke.
- [ ] Level 8 controlled failure fixture is still needed for TaskRunJournal partial failure / topology blocking smoke.
- [ ] Composite `create_blueprint_feature` execute fixture still needs a disposable asset target.
- [ ] Non-BlueprintHelper-owned graph content still needs a stable read/write anchor contract.
- [ ] GUID remains expert/debug fallback only; do not move it back into the Agent-facing main write contract.
- [x] TaskSpec field cleanup: Agent-facing `intent` removed; completed task journals now use orchestration-generated `generated_intent`; `feature_name` is display / journal label only and no longer drives graph-name recommendations.

## P2 entry candidates

- [x] Function/Event Signature Management first slice source integrated: internal `FBlueprintHelperSignatureService`, Signature DTO folder, Runtime delegation, `ensure_function` inputs/outputs forwarding, `ensure_custom_event` entry creation first slice, `ensure_event_dispatcher` declaration path, `ensure_override_event` blocked preflight path, `remove_signature` TaskPlan preflight/blocked path, and UE automation coverage added.
- [x] DataAsset/ObjectProperty first slice source integrated: `object_property/property_edit` TaskSpec schema, TS/Python compiler, TaskPlan adapter, true dry-run service façade, and TaskRuntime dispatch added.
- [x] Cleanup/Rollback/Ownership first slice source integrated: `graph_cleanup_ownership/owned_block_lifecycle` TaskSpec schema, TS/Python compiler, TaskPlan adapter, and TaskRuntime dispatch to cleanup / convert / rollback services added.
- [ ] P2 unified verification pending: do not count Signature / ObjectProperty / CleanupOwnership as smoke-verified until the next grouped build + automation + disposable fixture run.
- [ ] Function/Event Signature remaining: full custom event body split, interface function vs interface event, event dispatcher signature mutation policy, override/native event execute policy, and remove-signature execute policy.
- [ ] DebugExport/LargePayload service and task debug payload references.
- [ ] DependencyAnalysis / ReferenceContextPack integration into high-risk preview blockers.
