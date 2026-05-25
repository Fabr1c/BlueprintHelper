# BlueprintHelper GraphWrite CapabilityContract

日期：2026-05-25

## Status

状态：STABLE-CANDIDATE / CORE GATES IMPLEMENTED

本文档记录 GraphWrite 需要补充的全局能力契约。当前 `AgentFaceService/task-core/src/task/schema/task-contract.ts` 中的 `supported_first_slice` 只描述 Agent-facing TaskSpec 的稳定第一批子集，不等同于 GraphWrite 内部、Bridge route、runtime adapter、ActionResolution cluster、Review evidence 的完整能力面。

## Goal

新增一份机器可读或可机械验证的 `GraphWriteCapabilityContract`，作为 GraphWrite 能力齐全与稳定性的统一事实来源。

契约应覆盖：

| 字段 | 含义 |
|---|---|
| `operation_id` | 标准化能力名，例如 `function.call`、`field.set_property`、`generic.asset_action`、`patch.connect_pins`。 |
| `surface` | `TaskSpec`、`Bridge`、`RuntimeAdapter`、`ActionResolution`、`GraphStatement`、`MutationCoordinator` 中哪些入口公开支持。 |
| `owner_cluster` | 能力归属簇，例如 Function、Field、Generic、EventDelegate、Merge/Patch mutation。 |
| `required_evidence` | 成功所需 projected evidence；缺失时的 deterministic diagnostic。 |
| `preview_support` | Preview 是否同源支持，以及是否允许 preview-only blocked。 |
| `execute_support` | Execute 是否同源支持。 |
| `review_evidence_support` | 是否产出 Review evidence，是否有 TaskRuntime -> Review model 消费测试。 |
| `readback_support` | 是否有 readback 或 graph assertion 证明。 |
| `test_status` | focused test、contract test、E2E smoke、generality preflight 的当前状态。 |
| `agent_facing_status` | `public`、`internal_only`、`blocked_until_evidence_fixture`、`retired`。 |

## Required Reliability Gates

1. Review evidence 证据链必须补全：GraphWrite step result -> cluster-owned evidence -> TaskRuntime journal/post-io batch -> Review model -> Review UI / DebugBundle / AcceptReject 消费，必须有端到端自动化断言。
2. Merge/Patch/ConnectPins 的 Review metadata 需要覆盖 `EvidenceId`、`ChangeId`、父子关系、graph target、operation kind、asset path、graph name。
3. legacy parsed-plan residue 必须全量移除；不允许只保留 `parsed_node_plan_unsupported` 作为长期阻断路径。
4. `asset_action` 对 Agent-facing TaskSpec 的 positive support 需要真实 ActionDatabase projection fixture，不能用手写 stable id 或 synthetic spawner 伪造成功。
5. GraphWrite 稳定性收敛后，统一执行 generality preflight 作为最终能力面验收，而不是当前修复前置门禁。

## Current Known Gaps

| Gap | 当前状态 | 影响 |
|---|---|---|
| 全局 capability contract 缺失 | CLOSED / IMPLEMENTED | `GRAPHWRITE_CAPABILITY_CONTRACT` 已落到 `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts` 并从 `task-schemas.ts` 导出。 |
| Review evidence 上层消费链路缺端到端断言 | CLOSED FOR PRODUCER -> POSTIO -> REVIEWSTORE | 已覆盖 GraphWrite producer-owned evidence、PostIO zero-record diagnostic、ReviewStore record field preservation；Review UI / DebugBundle 继续消费同一 ReviewStore 模型，不在 GraphWrite runtime 中增加 fallback。 |
| legacy parsed-plan residue | CLOSED / REMOVED | `BlueprintGraphMutationPlan*` 源文件和旧测试已删除，`parsed_node_plan_unsupported` 只保留在 contract test 禁止词中。 |
| `asset_action` TaskSpec positive smoke | CORE RESOLVER CLOSED / FINAL TASKSPEC PREFLIGHT PENDING | `asset_action` 已改为 ActionDatabase projection service + execute-time projected identity revalidation；query-only/node-class-only execute requests 被拒绝。完整 Agent-facing 45-op generality preflight 仍未实现。 |

## 2026-05-25 Implementation Binding

- Source of truth: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`.
- Public export: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`.
- Contract test: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`.
- Stability rule: no GraphWrite operation can be called stable unless the contract marks it `supported`, has a review evidence policy, and is covered by either direct automation or the final generality preflight.

## 2026-05-25 Stability Closure Execution Evidence

| Gate | Status | Evidence |
|---|---|---|
| Capability contract | PASS | `npm.cmd --prefix AgentFaceService/task-core run build`; `npm.cmd --prefix AgentFaceService/task-core run test:node`。 |
| Review evidence producer | PASS | `Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence`。 |
| Review evidence PostIO / ReviewStore | PASS | `Automation RunTests BlueprintHelper.TaskRuntime.PostIO`。 |
| Legacy parsed-plan removal | PASS | `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline`; source scan only reports forbidden-token tests. |
| `asset_action` projection/revalidation | PASS | `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.AssetAction`; `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner`。 |
| Function / Field focused smoke | PASS | Focused gate id `BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke`; covered by the full `Automation RunTests BlueprintHelper.GraphWrite` suite. |
| Event taxonomy focused smoke | PASS | Focused gate id `BlueprintHelper.GraphWrite.EventTaxonomy`; covered by the full `Automation RunTests BlueprintHelper.GraphWrite` suite. |
| Cluster context boundary | PASS | Focused gate id `BlueprintHelper.GraphWrite.ActionResolution.Clusters`; covered by the full `Automation RunTests BlueprintHelper.GraphWrite` suite. |
| Full GraphWrite automation suite | PASS | `Automation RunTests BlueprintHelper.GraphWrite` passes after FunctionAction real-spawner fixes, event taxonomy fixture evidence updates, and `asset_action` weak-selector rejection. |
| Final 45-operation generality preflight | PENDING | `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md` 仍是未实现计划；不能把本轮 focused gates 等同于最终泛化验收。 |
