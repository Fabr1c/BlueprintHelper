# BlueprintHelper GraphWrite TS 接线后覆盖率重算

日期：2026-05-26

## 口径

本次只回答“TS 接完后，当前 GraphWrite 覆盖率是否变化”。计算采用 ownership filter：

1. 其他工具簇已经拥有的能力不进入 GraphWrite 未实现分母。
2. `BlueprintSignature` 拥有的 custom/override/native event declaration、handler/signature creation、dispatcher creation 只作为 GraphWrite/EventDelegate 的 projected evidence 来源，不算 EventDelegate 未实现。
3. `rejected` / design-rejected 项不进入未实现分母。
4. UI/menu/drag/pin-drag/editor-only 行为不进入 GraphWrite 能力分母。
5. interface-target cast 视为 `dynamic_cast + interface target_class_path` 的参数化情形，不作为独立 GraphWrite operation。
6. `UBlueprintAssetNodeSpawner` / `FAssetData` 资产专用 spawner 不进入当前 GraphWrite 分母；GraphWrite 只保留已证实的 `asset_action` 图节点插入入口。

## 当前机器可枚举覆盖率

来源：`AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts` 当前 build 输出。

### Core clusters

| Cluster | Supported | Excluded / gated | Ownership-filtered coverage |
|---|---:|---:|---:|
| `function_action` | 2 | 0 | 100% |
| `field` | 2 | 0 | 100% |
| `event` | 1 | 2 discussion-gated Signature-owned entries | 100% |
| `asset_action` | 1 | 0 | 100% |
| `container_action` | 26 | 0 | 100% |
| `generic_schedule` | 2 | 0 | 100% |
| **合计** | **34** | **2 excluded/gated** | **34 / 34 = 100%** |

如果错误地把 `event.override_native_event` 和 `event.delegate_component_bound_event` 也计入 Event 分母，Event raw rate 会变成 `1 / 3 = 33.3%`；这不是当前推荐口径，因为这些 declaration / signature ownership 已归 `BlueprintSignature`。

### Logical operation groups

| Group | Evidence-backed supported | Rejected / excluded | Contract-ahead | Ownership-filtered coverage |
|---|---:|---:|---:|---:|
| `event_delegate` | 6 | 2 | 0 | 100% |
| `generic_ops.control` | 17 | 0 | 0 | 100% |
| `generic_ops.container` | 58 | 0 | 0 | 100% |
| `generic_ops.transform` | 10 | 1 | 0 | 100% |
| `generic_ops.create` | 11 | 0 | 0 | 100% |
| `generic_ops.schedule` | 15 | 0 | 0 | 100% |
| `generic_ops.struct_select` | 4 | 2 | 0 | 100% |
| `op_coverage` | 38 | 8 | 0 | 100% |
| **合计** | **159** | **13** | **0** | **159 / 159 = 100%** |

Published contract consistency is now `159 / 159 = 100%` after removing the two non-denominator rows from the public operation contract.

## 四簇覆盖率区间更新

这不是 final generality preflight 分数，而是 TS 接线后按 ownership filter 的 scoped implementation readiness。

| 四簇 | 接线后建议区间 | 变化原因 |
|---|---:|---|
| `FunctionActionCluster` | 85%-90% | Function-backed create / convert / schedule 已进入 TS public/compiler/template 路径；container/op coverage 继续归 FunctionAction；参数化 dynamic cast 与资产专用 spawner 不归 FunctionAction 分母。 |
| `FieldVariableActionCluster` | 80%-85% | 本轮 TS 接线没有实质改变 Field 分母；保持 first-class Field capability 后的区间。 |
| `EventDelegateActionCluster` | 80%-85% | 只统计 use-site：component-bound event 与 delegate bind/assign/unbind/call/clear。Signature-owned declaration/handler/signature creation 排除后，不再把这些项记为 Event 未实现。 |
| `GenericAssetStructControlActionCluster` | 85%-90% | Generic control、generic-owned create、generic schedule、struct/select 的 TS public/compiler/template 路径已接通；link-time conversion、split/recombine pin、参数化 dynamic cast 与资产专用 spawner 不进入已实现分母。 |

四簇等权估算：`(87.5 + 82.5 + 82.5 + 87.5) / 4 = 85.0%`。

建议对外同步为：**当前四簇 ownership-filtered scoped implementation coverage 约 82.5%-87.5%，中位估算约 85%。**

## 不应再标记为 GraphWrite 未实现的项

| 能力 | 处理 |
|---|---|
| `ensure_custom_event` / `ensure_override_event` / native event declaration | `BlueprintSignature` ownership；GraphWrite 只消费 evidence。 |
| function signature / handler signature / dispatcher creation | `BlueprintSignature` ownership；缺 evidence 时 GraphWrite 返回确定性失败。 |
| EventDelegate duplicate replace / merge policy | 设计性 rejected；不进入未实现分母。 |
| link-time automatic conversion | 需要 linker/readback 专门边界；当前为 rejected，不进入 Generic transform 未实现分母。 |
| split / recombine pin | 不是 graph statement operation；不进入 Generic struct/select 未实现分母。 |
| interface-target dynamic cast | `dynamic_cast` 的 target type 参数化情形，不作为独立 operation。 |
| asset-backed `UBlueprintAssetNodeSpawner` graph node | 资产专用 spawner，不属于当前 GraphWrite operation 分母；保留 `asset_action` 作为已证实入口。 |
| UI/menu/drag/pin-drag/editor-only behavior | 非 GraphWrite TaskSpec 能力分母。 |

## 结论

有更新：

- 机器可枚举的 ownership-filtered GraphWrite contract coverage：`34 / 34 = 100%` core cluster rows，`159 / 159 = 100%` logical operation rows。
- published contract 一致性同步更新为 `159 / 159 = 100%`；不再存在这两个 contract-ahead token。
- 四簇工程成熟度建议从之前约 `75%-80%` 更新为约 `82.5%-87.5%`，中位估算约 `85%`。
- 不能写成 final 100% completion；final ownership-filtered generality preflight 仍未完成，完整 `BlueprintHelper.GraphWrite` suite 当前仍有既有 call-function / replace / block-scoped 失败。
