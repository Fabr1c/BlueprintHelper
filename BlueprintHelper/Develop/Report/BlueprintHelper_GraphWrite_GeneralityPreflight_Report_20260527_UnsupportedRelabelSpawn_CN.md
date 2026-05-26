# GraphWrite Generality Preflight E2E Report

Gate: FAIL

- Operation pass rate: 93.94% (31/33)
- Variant pass rate: 95.24% (40/42)
- Data: `BlueprintHelper_GraphWrite_GeneralityPreflight_Data_20260527_UnsupportedRelabelSpawn.csv`
- Summary JSON: `BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json`

Failure taxonomy: `unsupported_intent` is reserved for current implementation diagnostics that explicitly reject an operation or statement kind. Missing projected spawner identity, stale fixture proof, or missing proof evidence is reported as `missing_evidence`.

![Operation pass/fail](BlueprintHelper_GraphWrite_GeneralityPreflight_OperationChart_20260527_UnsupportedRelabelSpawn.svg)

![Failure distribution](BlueprintHelper_GraphWrite_GeneralityPreflight_FailureChart_20260527_UnsupportedRelabelSpawn.svg)

## Operation Table

| Asset | Operation | Mode | Requested | Candidate count | Actual spawned | Result | Variants | Failure kinds |
|---|---|---|---:|---:|---:|---|---:|---|
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_add` | `container.map.add` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_clear` | `container.map.clear` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_contains` | `container.map.contains` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_find` | `container.map.find` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_get_key_value_by_index` | `container.map.get_key_value_by_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_get_last_index` | `container.map.get_last_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_is_empty` | `container.map.is_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_is_not_empty` | `container.map.is_not_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_keys` | `container.map.keys` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_length` | `container.map.length` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_remove` | `container.map.remove` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_container_map_values` | `container.map.values` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_function_action_macro_like` | `function_action.macro_like` | parameterized_10 | 10 | 10 | 10 | PASS | 10/10 | none:10 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_branch` | `generic_ops.control.branch` | singleton_1 | 1 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_do_n` | `generic_ops.control.do_n` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_do_once` | `generic_ops.control.do_once` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_flip_flop` | `generic_ops.control.flip_flop` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_for_loop` | `generic_ops.control.for_loop` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_for_loop_with_break` | `generic_ops.control.for_loop_with_break` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_foreach_loop` | `generic_ops.control.foreach_loop` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_foreach_loop_with_break` | `generic_ops.control.foreach_loop_with_break` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_gate` | `generic_ops.control.gate` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_multi_gate` | `generic_ops.control.multi_gate` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_return` | `generic_ops.control.return` | singleton_1 | 1 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_sequence` | `generic_ops.control.sequence` | singleton_1 | 1 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_switch_enum` | `generic_ops.control.switch_enum` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_switch_int` | `generic_ops.control.switch_int` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_switch_name` | `generic_ops.control.switch_name` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_switch_string` | `generic_ops.control.switch_string` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_control_while_loop` | `generic_ops.control.while_loop` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_generic_ops_struct_select_select` | `generic_ops.struct_select.select` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_schedule_latent_or_async_node` | `schedule.latent_or_async_node` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | missing_evidence:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GWRelabel003_schedule_timer_delegate_node` | `schedule.timer_delegate_node` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | missing_evidence:1 |
