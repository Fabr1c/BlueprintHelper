# BlueprintHelper Current TODO Ledger 2026-05-19

Date: 2026-05-19

This document is the current TODO ledger for `Develop/Plan`. It supersedes the older 2026-05-14/2026-05-17 unmet-expectations wording while keeping the same path so archived documents that point to this ledger continue to resolve.

## Status Rules

- `Open`: active work remains.
- `In Progress`: implementation has started and still needs follow-up work or verification.
- `Partial`: core implementation or automation exists, but a documented verification gap remains.
- `Watch`: no current blocker, but future reports should be debugged through the current architecture.
- `Future`: intentionally not part of the current closure path.
- `Closed`: no longer tracked as an open TODO.

## Current Open TODO

| Priority | Area | Status | Source | Next Closure Condition |
| --- | --- | --- | --- | --- |
| P0 | `logic_flow` compact read format | Open | `BlueprintHelper_ReadContext_LogicFlow_Rules_20260519_CN.md`, `BlueprintHelper_ReadContext_LogicFlow_Implementation_PLAN_20260519_CN.md` | Complete implementation Tasks 1-6, including task-core tests, docs/templates, ReadSpecs, and verification. |
| P0 | Settings runtime consumption | In Progress | `BlueprintHelper_SettingsRuntimeConsumption_ImplementationPlan_20260520_CN.md` | Finish ToolCluster / ReadContext `default_scope` semantics and run Automation / runtime smoke after high-risk developer exec is enabled. |
| P1 | ReadContext pin default end-to-end coverage | Partial | `BlueprintHelper_ReadContext_PinDefaults_Fix_20260518_CN.md` | Run real editor `read_context` coverage for `Text` and `Object/Class` defaults from UE pin data through raw JSON and LogicJson. |
| P1 | ReviewPanel v2 follow-up edge cases | Watch | `BlueprintHelper_ReviewPanelV2_ArchitecturePlan_20260519_CN.md` | No current blocker. If a new issue appears, diagnose via DebugBundle and fix through Store/PanelState/SurfaceView/RowBinding boundaries. |
| P2 | Function scope and Local Variables Review | Open / needs plan | Historical TODO only | Create a focused current plan before implementation; do not reopen old ReviewPanel v1 documents as active guidance. |
| P2 | Baseline semantic snapshot retention / compaction | Future | Archived migration record under `../v0.4.3/ArchivedReference/RetiredReviewDebugDocs_20260518/` | Decide retention/compaction policy separately. Core semantic target snapshot path is already complete. |
| P2 | Historical document pointer cleanup | Partial | `../v0.4.4/ArchivedReference/CompletedDevelopDocs_20260519/` | Most completed docs are archived. Refresh only if a live document still points to a moved file as if it were active. |

## Closed Or Replaced Since 2026-05-17

- Old ReviewPanel A1/A2/A5/A7/A8/A9/B1-UI smoke buckets are replaced by the ReviewPanel v2 ledger. Current v2 status says there is no clear blocking issue.
- Review/Debug v1 and pre-v2 docs are archived under `../v0.4.3/ArchivedReference/RetiredReviewDebugDocs_20260518/`.
- Completed v0.4.4-era Plan and Design records are archived under `../v0.4.4/ArchivedReference/CompletedDevelopDocs_20260519/`.
- Completed v0.5.0-v0.5.3 performance, legacy cleanup, and SettingsPanel implementation records are archived under `../v0.5.4/ArchivedReference/CompletedDevelopDocs_20260520/`.
- The Total PASS report is historical evidence and now lives at `../v0.4.4/ArchivedReference/CompletedDevelopDocs_20260519/Plan/BlueprintHelper_Total_PASS_Report_20260517_CN.md`.
- AgentFace field audit, CallFunction resolver/testing, FunctionChainContext, ReferenceContext, GraphLayout, architecture optimization, UE 5.3 compatibility, legacy fixture removal, and related completed records are archived and are not active TODO entries.

## Active Entry Points

- Active index: `README.md`
- ReviewPanel v2: `BlueprintHelper_ReviewPanelV2_ArchitecturePlan_20260519_CN.md`
- LogicFlow rules: `BlueprintHelper_ReadContext_LogicFlow_Rules_20260519_CN.md`
- LogicFlow implementation plan: `BlueprintHelper_ReadContext_LogicFlow_Implementation_PLAN_20260519_CN.md`
- Pin defaults follow-up: `BlueprintHelper_ReadContext_PinDefaults_Fix_20260518_CN.md`
- Settings runtime consumption plan: `BlueprintHelper_SettingsRuntimeConsumption_ImplementationPlan_20260520_CN.md`
- CLI operational tips: `BlueprintHelper_CLI_Tips_20260514_CN.md`

## Operating Notes

1. Do not treat archived docs as active implementation plans unless a current doc links to a specific historical evidence item.
2. ReviewPanel work must follow the v2 Data / Service / Presenter / UI boundaries; do not restore fuzzy action routing, UI-local state deletion, transaction fallback, or delay/retry refresh fixes.
3. `logic_flow` is a compact reading format only. It is not an anchor source for write workflows; anchor-sensitive work still requires `logic_json`.
4. Settings runtime consumption work must keep configurable values flowing through typed settings resolvers instead of reintroducing hardcoded defaults.
