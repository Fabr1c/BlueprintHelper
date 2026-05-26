# BlueprintHelper GraphWrite Operation Overlap Audit

日期：2026-05-26

## 口径

本审计只检查 GraphWrite 当前 contract / TaskSpec / UE runtime 中的能力重合，不把 UI、菜单、拖拽、Content Browser 选择态计入 GraphWrite 分母。

分类规则：

- `duplicate denominator`：同一能力已经有一个更明确的 GraphWrite operation 或工具簇 owner，再把另一行计入唯一能力分母会重复。
- `logical view only`：可以保留在 contract 中作为逻辑索引或 runtime cluster 视图，但不能与 canonical operation 相加计数。
- `owner mismatch`：TS contract 与 UE runtime owner 不一致，需要后续修契约或 runtime。
- `valid separate operation`：看起来相邻，但证据、节点族或 use-site 语义不同，可以保留。
- `needs decision`：当前代码可以双路表达，但需要确认唯一分母 owner。

## 主要结论

当前移除 interface-target dynamic cast 与 asset-specific spawner 后，旧 `159 / 159` 只能解释为“published logical contract consistency”，不能解释为“去重后的唯一语义能力数”。至少有以下重合点需要拆开统计：

1. `container_action` 与 `generic_ops.container.*` 是同一批 container action 语义的两个视图。
2. `generic_schedule` cluster 与 `generic_ops.schedule.timer_delegate_node` / `latent_or_async_node` 是同一批 generic schedule use-site 的两个视图。
3. `asset_action` cluster 与 `generic_ops.create.asset_action` 是同一能力的 cluster / logical group 双入口。
4. `op_coverage.array_identical` 与 `container.array.identical` 指向同一个 `Array_Identical` callable，唯一能力分母中应只保留一个。
5. `generic_ops.transform.type_promotion` 当前 TS contract 标为 `FunctionAction`，但 UE runtime 通过 Generic transform resolver 与 `FTypePromotion` evidence 处理，这是 owner mismatch。
6. `generic_ops.struct_select.set_fields_in_struct` 与 Field 的 `field.struct_member_set` 使用同一 `K2Node_SetFieldsInStruct` 节点族，需要确认唯一 owner。
7. `function_action.call_function` 是 runtime 顶级头；function-backed create / convert / schedule / op 不应再把 `call_function` 当作第二个 operation 分母。

## Findings

| ID | 重合项 | 分类 | 证据 | 建议 denominator 动作 |
|---|---|---|---|---|
| OVL-001 | `container_action` cluster vs `generic_ops.container.*` | `logical view only` + count mismatch | `task-schemas.ts` 导出 58 个 `CONTAINER_ACTION_OPERATION_IDS`；UE `BlueprintHelperContainerActionVocabulary.cpp` 也 `Reserve(58)`；但 `graphwrite-capability-contract.ts` 的 core `container_action` cluster 当前只列 26 个。 | 不要把 core cluster 与 `generic_ops.container` 相加。若 core cluster 表示 first-class container_action，应同步为 58；若 core cluster 只表示 stable seed，则覆盖率文档必须显式写“26 seed / 58 logical view”。 |
| OVL-002 | `generic_schedule` cluster vs `generic_ops.schedule.timer_delegate_node` / `generic_ops.schedule.latent_or_async_node` | `logical view only` | Contract core cluster 列 `schedule.timer_delegate_node` / `schedule.latent_or_async_node`；GenericOps schedule 同时列同名二阶段 operation，并由 Generic owner 路由。 | 保留一个 canonical denominator。建议 core cluster 作为 runtime owner readiness，GenericOps row 作为 logical index，不相加。 |
| OVL-003 | `asset_action` cluster vs `generic_ops.create.asset_action` | `logical view only` | Contract core cluster `create.asset_action` 与 GenericOps create `asset_action` 使用相同 ActionDatabase projected evidence 边界。 | 保留 `asset_action` runtime cluster 为 canonical readiness；GenericOps create row 只作为 create family logical index。 |
| OVL-004 | `op_coverage.array_identical` vs `container_action` / `generic_ops.container.array.identical` | `duplicate denominator` | `OP_COVERAGE_P2_OPERATIONS` 包含 `array_identical`；container action vocabulary 使用 `/Script/Engine.KismetArrayLibrary:Array_Identical` 实现 `container.array.identical`。 | 唯一语义能力分母建议保留 `container.array.identical`，把 `op_coverage.array_identical` 标为 alias / excluded，除非用户明确需要同时暴露 `kind=op` 和 `container_action` 两种 API 形态。 |
| OVL-005 | `generic_ops.transform.type_promotion` TS owner vs UE owner | `owner mismatch` | TS contract 当前把 `type_promotion` 放在 FunctionAction transform list；UE `GenericTransformScheduleActionResolver::IsSupportedTransformOperation()` 接受 `type_promotion`，并调用 `BlueprintHelperTypePromotionSpawnerEvidenceResolver`。 | 后续应把 contract 中 `type_promotion` 的 runtime owner 改为 `GenericAssetStructControlAction`，或明确拆成 FunctionAction operator path 与 Generic FTypePromotion path 两个不同 operation。 |
| OVL-006 | `generic_ops.struct_select.set_fields_in_struct` vs `field.struct_member_set` | `needs decision` | Field capability spec 定义 `field.struct_member_set -> set_fields_in_struct`；StructField builder 又把 `set_fields_in_struct` 写成 `field.struct_member_set` plan 并打 `generic.struct.operation` tag。 | 建议由 Field 作为 property/member mutation canonical owner；GenericOps 可保留 `make_struct` / `break_struct` / `select`，`set_fields_in_struct` 是否继续计入 GenericOps 分母需要用户决策。 |
| OVL-007 | Event cluster discussion-gated rows vs EventDelegate / BlueprintSignature | `other-cluster ownership` | Coverage 文档已排除 `event.override_native_event` 与 `event.delegate_component_bound_event`；Task contract 写明 EventDelegate 只是 use-site，handler/signature 由 `blueprint_signature` dependency 提供。 | 当前处理正确：Signature-owned declaration 不计入 GraphWrite/EventDelegate 未实现；EventDelegate use-site 不与 Event declaration 相加。 |
| OVL-008 | `function_action.call_function` vs function-backed create / convert / schedule / op / container | `logical view only` | Runtime demand collector 给 Call/Op/Convert/Schedule/ContainerAction 分别设置 `function_call`、`operator_function`、`convert_function`、`schedule_function`、`container_action`。 | `call_function` 只作为 FunctionAction runtime header；unique operation denominator 应使用具体 second-stage operation，不再额外加 `call_function`。 |

## 建议的计数口径

短期不要再把以下三类数字混成一个百分比：

1. `runtime cluster readiness`：只看 core cluster 是否有稳定 runtime owner、review evidence、execute revalidation。
2. `logical operation contract consistency`：看 `operationGroups` 中 supported rows 是否有 TS / UE / template / readback 证据。
3. `unique semantic capability coverage`：先去掉 alias / view rows，再计算唯一能力分母。

按当前证据，`159 / 159` 仍可作为 logical contract consistency，但不能作为 unique semantic capability coverage。

## 建议后续动作

优先级：

1. 先修 `type_promotion` owner mismatch：这是 contract 与 UE runtime 明确不一致。
2. 再处理 `array_identical`：确认是否保留 `kind=op` alias；若不保留，从 OpCoverage supported 分母移到 excluded/alias。
3. 再统一 container 计数：决定 core `container_action` cluster 是 26 seed 还是 58 first-class operation。
4. 最后确认 `set_fields_in_struct` canonical owner 是 Field 还是 GenericOps struct_select。

