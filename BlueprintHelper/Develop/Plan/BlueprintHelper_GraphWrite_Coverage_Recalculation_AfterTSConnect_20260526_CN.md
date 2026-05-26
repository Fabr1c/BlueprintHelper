# BlueprintHelper GraphWrite TS 接线后覆盖率重算

日期：2026-05-26

## 口径

本次按最新去重口径重算 GraphWrite 覆盖率：

1. 其他工具簇已经拥有的能力不进入 GraphWrite 未实现分母。
2. `BlueprintSignature` 拥有的 custom/override/native event declaration、handler/signature creation、dispatcher creation 只作为 GraphWrite/EventDelegate 的 projected evidence 来源。
3. `rejected` / design-rejected 项不进入未实现分母。
4. UI/menu/drag/pin-drag/editor-only 行为不进入 GraphWrite 能力分母。
5. interface-target cast 是 `dynamic_cast + target_class_path` 的参数化情形，不作为独立 GraphWrite operation。
6. `UBlueprintAssetNodeSpawner` / `FAssetData` 资产专用 spawner 不进入当前 GraphWrite 分母；GraphWrite 只保留 canonical `asset_action` 图节点插入入口。
7. 以下重复索引已从 GenericOps / OpCoverage 删除，不保留 fallback、alias 或公共索引：`generic_ops.container.*`、`generic_ops.schedule.*`、`generic_ops.create.asset_action`、`op_coverage.array_identical`、`generic_ops.struct_select.set_fields_in_struct`，以及 `generic_ops.transform/create/schedule` 中 function-backed 子集。

## 当前机器可枚举覆盖率

来源：`AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts` 当前合同。

### Core clusters

| Cluster | Supported | Excluded / gated | Ownership-filtered coverage |
|---|---:|---:|---:|
| `function_action` | 2 | 0 | 100% |
| `field` | 3 | 0 | 100% |
| `event` | 1 | 2 discussion-gated Signature-owned entries | 100% |
| `asset_action` | 1 | 0 | 100% |
| `container_action` | 58 | 0 | 100% |
| `generic_schedule` | 2 | 0 | 100% |
| **合计** | **67** | **2 excluded/gated** | **67 / 67 = 100%** |

`field` 增加的第 3 行是 canonical `field.struct_member_set`。`container_action` 已从旧 26 seed 同步为 58 个 first-class container operation，其中包含 `container.array.identical`。

### Logical operation groups

| Group | Evidence-backed supported | Rejected / excluded | Contract-ahead | Ownership-filtered coverage |
|---|---:|---:|---:|---:|
| `event_delegate` | 6 | 2 | 0 | 100% |
| `generic_ops.control` | 17 | 0 | 0 | 100% |
| `generic_ops.transform` | 3 | 1 | 0 | 100% |
| `generic_ops.create` | 6 | 0 | 0 | 100% |
| `generic_ops.struct_select` | 3 | 2 | 0 | 100% |
| `op_coverage` | 37 | 8 | 0 | 100% |
| **合计** | **72** | **13** | **0** | **72 / 72 = 100%** |

旧表的 `159 / 159` 已不再使用。最新合同去掉了 87 个 supported duplicate / wrong-owner 行：58 个 `generic_ops.container.*`、15 个 `generic_ops.schedule.*`、4 个 function-backed create 行、7 个 function-backed transform 行、1 个 `generic_ops.create.asset_action`、1 个 `generic_ops.struct_select.set_fields_in_struct`、1 个 `op_coverage.array_identical`。

## 四簇覆盖率区间更新

这不是 final generality preflight 分数，而是 TS 接线并完成去重后的 scoped implementation readiness。

| 四簇 | 建议区间 | 当前口径 |
|---|---:|---|
| `FunctionActionCluster` | 85%-90% | 保留 `function_action` runtime header、canonical container action 与 Function-owned op coverage；GenericOps 不再重复发布 function-backed create / convert / schedule 索引。 |
| `FieldVariableActionCluster` | 82.5%-87.5% | `field.struct_member_set` 作为 Field canonical owner 保留，`generic_ops.struct_select.set_fields_in_struct` 删除。 |
| `EventDelegateActionCluster` | 80%-85% | 只统计 use-site：component-bound event 与 delegate bind/assign/unbind/call/clear；Signature-owned declaration/handler/signature creation 排除。 |
| `GenericAssetStructControlActionCluster` | 85%-90% | 只保留 Generic-owned control、transform、create、struct/select 与 canonical `generic_schedule`；asset_action、container、function-backed 子集不再计入 GenericOps。 |

四簇等权估算：`(87.5 + 85.0 + 82.5 + 87.5) / 4 = 85.625%`。

建议对外同步为：**当前四簇 ownership-filtered scoped implementation coverage 约 82.5%-87.5%，中位估算约 85.6%。**

## 不应再标记为 GraphWrite 未实现的项

| 能力 | 处理 |
|---|---|
| `ensure_custom_event` / `ensure_override_event` / native event declaration | `BlueprintSignature` ownership；GraphWrite 只消费 evidence。 |
| function signature / handler signature / dispatcher creation | `BlueprintSignature` ownership；缺 evidence 时 GraphWrite 返回确定性失败。 |
| EventDelegate duplicate replace / merge policy | 设计性 rejected；不进入未实现分母。 |
| link-time automatic conversion | 需要 linker/readback 专门边界；当前为 rejected，不进入 Generic transform 未实现分母。 |
| split / recombine pin | 不是 graph statement operation；不进入 Generic struct/select 未实现分母。 |
| interface-target dynamic cast | `dynamic_cast` 的 target type 参数化情形，不作为独立 operation。 |
| asset-backed `UBlueprintAssetNodeSpawner` graph node | 资产专用 spawner，不属于当前 GraphWrite operation 分母；保留 `asset_action` canonical 入口。 |
| `generic_ops.container.*` | 删除；唯一 owner 是 `container_action`。 |
| `generic_ops.schedule.*` | 删除；generic schedule use-site 只保留 `generic_schedule` core cluster，function-backed schedule 走 FunctionAction 语义。 |
| `generic_ops.create.asset_action` | 删除；唯一 owner 是 `asset_action` core cluster。 |
| `op_coverage.array_identical` | 删除；唯一 GraphWrite denominator 是 `container.array.identical`。 |
| `generic_ops.struct_select.set_fields_in_struct` | 删除；唯一 owner 是 `field.struct_member_set`。 |
| UI/menu/drag/pin-drag/editor-only behavior | 非 GraphWrite TaskSpec 能力分母。 |

## 结论

有更新：

- 机器可枚举的 ownership-filtered GraphWrite contract coverage：`67 / 67 = 100%` core cluster rows，`72 / 72 = 100%` logical operation rows。
- published logical operation denominator 从旧 `159` 收敛到 `72`，不再保留重复或 wrong-owner 索引。
- 四簇工程成熟度建议维持在约 `82.5%-87.5%`，中位估算约 `85.6%`。
- 不能写成 final 100% completion；final ownership-filtered generality preflight 和完整 UE 自动化门禁仍需要单独完成。
