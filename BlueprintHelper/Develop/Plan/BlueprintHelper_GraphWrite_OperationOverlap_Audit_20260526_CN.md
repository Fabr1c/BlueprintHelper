# BlueprintHelper GraphWrite Operation Overlap Audit

日期：2026-05-26

## 口径

本审计只检查 GraphWrite 当前 contract / TaskSpec / UE runtime 中的能力重合，不把 UI、菜单、拖拽、Content Browser 选择态计入 GraphWrite 分母。

最新决策：重复视图不再作为 GenericOps / OpCoverage 公共索引保留，也不做 fallback 兜底。保留 canonical owner，删除 duplicate denominator。

## 已处理结论

| ID | 原重合项 | 处理结果 |
|---|---|---|
| OVL-001 | `container_action` cluster vs `generic_ops.container.*` | 删除 `generic_ops.container.*`；`container_action` 作为唯一 canonical owner，并同步为 58 个 first-class operation。 |
| OVL-002 | `generic_schedule` cluster vs `generic_ops.schedule.timer_delegate_node` / `generic_ops.schedule.latent_or_async_node` | 删除 `generic_ops.schedule` 整组；generic schedule use-site 只保留 `generic_schedule` core cluster。 |
| OVL-003 | `asset_action` cluster vs `generic_ops.create.asset_action` | 删除 `generic_ops.create.asset_action`；`asset_action` core cluster 作为唯一 canonical owner。 |
| OVL-004 | `op_coverage.array_identical` vs `container.array.identical` | 删除 `op_coverage.array_identical`；唯一 GraphWrite denominator 是 `container.array.identical`。 |
| OVL-005 | `generic_ops.transform.type_promotion` TS owner vs UE owner | 修正为 `GenericAssetStructControlAction` owner；保留为 Generic transform operation。 |
| OVL-006 | `generic_ops.struct_select.set_fields_in_struct` vs `field.struct_member_set` | 删除 `generic_ops.struct_select.set_fields_in_struct`；`field.struct_member_set` 作为唯一 canonical owner。 |
| OVL-007 | Event cluster discussion-gated rows vs EventDelegate / BlueprintSignature | 保持现状：Signature-owned declaration / handler / signature 不计入 GraphWrite/EventDelegate 未实现分母。 |
| OVL-008 | `function_action.call_function` vs function-backed create / convert / schedule / op / container | 保持 `call_function` 为 FunctionAction runtime header；不额外作为二阶段 operation 分母相加。 |
| OVL-009 | Function-backed rows under GenericOps families | 删除 `generic_ops.transform/create/schedule` 中 function-backed 子集；这些能力只通过 FunctionAction 语义和 compiler/runtime path 表达。 |

## 当前保留的 canonical owner

| 能力 | Canonical owner |
|---|---|
| container array/map/set 操作 | `container_action` |
| `container.array.identical` | `container_action` |
| Generic schedule use-site | `generic_schedule` core cluster |
| ActionDatabase-backed asset action | `asset_action` core cluster |
| Struct member set | `field.struct_member_set` |
| Generic transform type promotion | `generic_ops.transform.type_promotion` with `GenericAssetStructControlAction` owner |
| Function-backed create / convert / schedule | FunctionAction semantic/compiler/runtime path；不在 GenericOps 里发布二级索引 |
| Event declaration / handler / signature | `BlueprintSignature` evidence owner；GraphWrite/EventDelegate 只消费 projected evidence |

## 计数影响

旧 logical operation denominator 是 `159` supported rows。按最新清理后：

- Core cluster rows：`67 / 67 = 100%`。
- Logical operation rows：`72 / 72 = 100%`。
- Removed supported rows：`87`。
- Rejected / excluded logical rows：仍为 `13`，不进入未实现分母。

删除的 87 个 supported rows：

| 删除项 | 行数 |
|---|---:|
| `generic_ops.container.*` | 58 |
| `generic_ops.schedule.*` | 15 |
| `generic_ops.transform` function-backed subset | 7 |
| `generic_ops.create` function-backed subset | 4 |
| `generic_ops.create.asset_action` | 1 |
| `generic_ops.struct_select.set_fields_in_struct` | 1 |
| `op_coverage.array_identical` | 1 |

## 仍需避免混淆的三类数字

1. `runtime cluster readiness`：只看 core cluster 是否有稳定 runtime owner、review evidence、execute revalidation。
2. `logical operation contract consistency`：看 `operationGroups` 中 supported rows 是否有 TS / UE / template / readback 证据。
3. `unique semantic capability coverage`：先去掉 alias / view rows，再计算唯一能力分母。

最新可同步口径：旧 `159 / 159` 已废弃；当前使用 `67 / 67` core cluster rows 和 `72 / 72` logical operation rows。
