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
| `asset_action` TaskSpec positive smoke | CORE RESOLVER CLOSED / FINAL TASKSPEC PREFLIGHT PENDING | `asset_action` 已改为 ActionDatabase projection service + execute-time projected identity revalidation；query-only/node-class-only execute requests 被拒绝。ownership-filtered Agent-facing generality preflight 仍未实现。 |

## 2026-05-25 Ownership Filter

以下项不再作为 GraphWrite 未完成能力追踪：

| Excluded from GraphWrite gaps | Owning boundary | Contract treatment |
|---|---|---|
| `ensure_custom_event`、`ensure_override_event`、`native_event` declaration、function signature、event dispatcher、handler declaration | `BlueprintSignature` | 作为 GraphWrite fixture/dependency/evidence 来源，不计入 GraphWrite operation pass/fail。 |
| `override_event` / `native_event` taxonomy | `BlueprintSignature` / UE native event ownership | 不在 GraphWrite/EventDelegate declaration taxonomy 内扩展。 |
| delegate handler 查找/创建、dispatcher 创建、handler signature | `BlueprintSignature` / ActionContext projection | 缺 evidence 时 GraphWrite/EventDelegate 确定性失败，不扫描资产补上下文。 |
| Merge/Patch/ConnectPins mutation ownership | Merge/Patch mutation services | 除非后续决定迁入 GraphStatement 主线，否则不计入 Spawner-Oriented Cluster gap。 |
| `anim_notify_event` / animation notify event entry | Animation Blueprint / Animation tooling | 已从 GraphWrite/EventDelegateActionCluster 移除；当前 GraphWrite 范围限定普通 Blueprint，动画蓝图事件入口不计入 GraphWrite capability gap。 |

按上述过滤后，真正仍需追踪的 GraphWrite gap 为：

| Remaining GraphWrite gap | Current status |
|---|---|
| real evidence / BuildEvidence defects | PARTIAL REMAINING：`timer_delegate_node`、`latent_or_async_node` Generic schedule success path 已按 `BlueprintHelper_GraphWrite_GenericScheduleSuccessPathPlan_20260525_CN.md` 落地；剩余为 `asset_action` Agent-facing positive TaskSpec/readback 与最终 ownership-filtered generality preflight。 |
| broad `container_action` | IMPLEMENTED / FOCUSED GATE PASS：`BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md` 已落地 V1 first-class 范围、FunctionAction-backed resolver、public shape、fragment role links、readback verifier 与 focused E2E；最终泛化矩阵仍由 ownership-filtered preflight 覆盖。 |
| contract expansion to filtered matrix | DEFERRED：当前能力面仍会变化；放到 evidence defects 与 `container_action` public shape 确定/完成后再对齐。 |
| ownership-filtered final generality preflight | DEFERRED：最终门禁放到 contract expansion 之后执行；matrix/spec factory/runner/report/readback gate 尚未实现，原 45-operation / 450-variant 口径需要按 ownership filter 重算。 |

Evidence status after source audit:

| Area | Already available / used | True missing defect |
|---|---|---|
| `context_evidence` passthrough | TS compiler preserves it into GraphWrite statement/expression; ActionContext passes it into resolution. | None. |
| `asset_action` projected identity keys | `asset_action_stable_id`、node class、spawner signature、owner path are consumed by ActionDatabase-backed resolver and execute-time revalidation; Review evidence intentionally stays graph-level `graph_block` / `graph_surface_atomic_target`，并由 `BlueprintHelper_GraphWrite_AssetActionReviewPolicy_GraphBlockPlan_20260525_CN.md` 固化防回退。 | No complete positive TaskSpec/readback fixture. |
| `type_promotion` projected evidence | stable id、operator、source/target/result pin type are preserved and consumed by `FBlueprintHelperTypePromotionSpawnerEvidenceResolver`. | Not a current evidence defect; keep only as final preflight coverage. |
| `timer_delegate_node` | `kind=schedule, schedule_operation=timer_delegate_node`, projected ActionDatabase spawner evidence, BlueprintSignature handler evidence. | SUPPORTED：resolver 通过当前 ActionDatabase spawner 重新验证；fragment builder 通过 CreateDelegate 链接既有 handler；GraphWrite 不创建 handler/signature。 |
| `latent_or_async_node` | `kind=schedule, schedule_operation=latent_or_async_node`, projected ActionDatabase spawner evidence, `graph_latent_allowed=true`. | SUPPORTED：resolver 通过当前 ActionDatabase spawner 重新验证；fragment builder 通过 shared adapter 生成节点；`graph_latent_allowed=false` 确定性阻断。 |
| non-make `container_action` | First-class V1 vocabulary now covers core array/map/set operations through TaskSpec public shape and FunctionAction-backed runtime resolution. | Final ownership-filtered generality preflight still needs to count the implemented operation set with 10 variants per owned operation. |

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
| `container_action` V1 | PASS | `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction` passes `SemanticIR`、`ContractValidation`、`ActionContext`、`Vocabulary`、`Resolver`、`FragmentDag`、`ArrayResultFragmentDag`、`EndpointPinTypeJsonRoundTrip`、`FocusedE2E`；TS contract/compiler tests cover public shape and lowering. |
| Generic schedule success path | PASS | `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule`；`Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule`；source scans show no Generic schedule owner mixing and no `UBlueprintNodeSpawner::Create(` in `BlueprintHelperGenericTransformScheduleActionResolver.cpp`. |
| Full GraphWrite automation suite | PASS | `Automation RunTests BlueprintHelper.GraphWrite` found 203 tests and exited with code 0 after the `container_action` implementation; includes the nine `BlueprintHelper.GraphWrite.ContainerAction.*` tests. |
| Final ownership-filtered generality preflight | PENDING | `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md` 仍是未实现计划；不能把本轮 focused gates 等同于最终泛化验收。 |
