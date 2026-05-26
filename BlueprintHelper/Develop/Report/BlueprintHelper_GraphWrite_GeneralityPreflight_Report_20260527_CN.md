# GraphWrite Generality Preflight E2E Report

Gate: FAIL

- Operation pass rate: 74.10% (103/139)
- Variant pass rate: 74.14% (129/174)
- Data: `BlueprintHelper_GraphWrite_GeneralityPreflight_Data_20260527.csv`
- Summary JSON: `BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json`

## Unsupported Label Audit

This report is the raw `_005` node-presence E2E output. Rows with `unsupported_intent` are not all current unsupported capabilities. After the 2026-05-27 audit:

| Raw area | Current classification |
|---|---|
| `generic_ops.control.branch/sequence/return` | Supported; old fixture used stale/fake `generic.control.operation` evidence. |
| `generic_ops.control` StandardMacros rows and `function_action.macro_like` | Supported; old fixture used incomplete macro graph path / dynamic pin evidence. |
| `schedule.timer_delegate_node` / `schedule.latent_or_async_node` | Supported by `generic_schedule`; old fixture missed projected spawner / handler / latent graph evidence. |
| `generic_ops.struct_select.select` | Supported; old fixture used unresolved result-type proof. |
| `container.map.*` | Supported; old fixture generated `GWGenStringIntMap` with the wrong key type and the readback was only node-presence. |
| Duplicate / wrong-owner rows such as `generic_ops.container.*`, `generic_ops.schedule.*`, `generic_ops.create.asset_action`, `op_coverage.array_identical`, `generic_ops.struct_select.set_fields_in_struct`, `generic_ops.transform.interface_dynamic_cast`, and `generic_ops.create.asset_backed_graph_node` | Excluded from the GraphWrite unsupported denominator; use the canonical owner or parameterized operation instead. |

Do not use this raw report as the final unsupported-capability list. Re-run the generality E2E with the corrected fixtures/evidence and overwrite this report before publishing updated coverage percentages.

![Operation pass/fail](BlueprintHelper_GraphWrite_GeneralityPreflight_OperationChart_20260527.svg)

![Failure distribution](BlueprintHelper_GraphWrite_GeneralityPreflight_FailureChart_20260527.svg)

## Operation Table

| Asset | Operation | Mode | Requested | Candidate count | Actual spawned | Result | Variants | Failure kinds |
|---|---|---|---:|---:|---:|---|---:|---|
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_add` | `container.array.add` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_add_unique` | `container.array.add_unique` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_append` | `container.array.append` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_clear` | `container.array.clear` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_contains` | `container.array.contains` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_filter_array` | `container.array.filter_array` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_find` | `container.array.find` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_get` | `container.array.get` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_identical` | `container.array.identical` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_insert` | `container.array.insert` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_is_empty` | `container.array.is_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_is_not_empty` | `container.array.is_not_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_is_valid_index` | `container.array.is_valid_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_last_index` | `container.array.last_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_length` | `container.array.length` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_random` | `container.array.random` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_random_from_stream` | `container.array.random_from_stream` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_remove_index` | `container.array.remove_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_remove_item` | `container.array.remove_item` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_resize` | `container.array.resize` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_reverse` | `container.array.reverse` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_set` | `container.array.set` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_shuffle` | `container.array.shuffle` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_00_container_array_shuffle_from_stream` | `container.array.shuffle_from_stream` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_byte` | `container.array.sort_byte` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_float` | `container.array.sort_float` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_int` | `container.array.sort_int` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_int64` | `container.array.sort_int64` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_name` | `container.array.sort_name` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_sort_string` | `container.array.sort_string` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_array_swap` | `container.array.swap` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_add` | `container.map.add` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_clear` | `container.map.clear` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_contains` | `container.map.contains` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_find` | `container.map.find` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_0_container_map_get_key_value_by_index` | `container.map.get_key_value_by_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_get_last_index` | `container.map.get_last_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_is_empty` | `container.map.is_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_is_not_empty` | `container.map.is_not_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_keys` | `container.map.keys` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_length` | `container.map.length` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_remove` | `container.map.remove` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_map_values` | `container.map.values` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_add` | `container.set.add` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_add_items` | `container.set.add_items` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_clear` | `container.set.clear` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_contains` | `container.set.contains` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_difference` | `container.set.difference` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_get_item_by_index` | `container.set.get_item_by_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_get_last_index` | `container.set.get_last_index` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_intersection` | `container.set.intersection` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_is_empty` | `container.set.is_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_is_not_empty` | `container.set.is_not_empty` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_length` | `container.set.length` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_remove` | `container.set.remove` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_remove_items` | `container.set.remove_items` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_to_array` | `container.set.to_array` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_container_set_union` | `container.set.union` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_create_asset_action` | `create.asset_action` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_custom_event` | `event.custom_event` | parameterized_10 | 10 | 10 | 10 | PASS | 10/10 | none:10 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_0_event_delegate_component_bound_event` | `event_delegate.component_bound_event` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_delegate_delegate_assign` | `event_delegate.delegate.assign` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_delegate_delegate_bind` | `event_delegate.delegate.bind` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_delegate_delegate_call` | `event_delegate.delegate.call` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_delegate_delegate_clear` | `event_delegate.delegate.clear` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_event_delegate_delegate_unbind` | `event_delegate.delegate.unbind` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_field_component_ref` | `field.component_ref` | limited_real_nodes | 10 | 2 | 2 | PASS | 2/2 | none:2 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_field_field_access` | `field.field_access` | limited_real_nodes | 10 | 8 | 8 | PASS | 8/8 | none:8 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_field_struct_member_set` | `field.struct_member_set` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_function_action_call_function` | `function_action.call_function` | parameterized_10 | 10 | 10 | 10 | PASS | 10/10 | none:10 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_function_action_macro_like` | `function_action.macro_like` | parameterized_10 | 10 | 10 | 0 | FAIL | 0/10 | preview_failure:10 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_branch` | `generic_ops.control.branch` | singleton_1 | 1 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_do_n` | `generic_ops.control.do_n` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_do_once` | `generic_ops.control.do_once` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_flip_flop` | `generic_ops.control.flip_flop` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_for_loop` | `generic_ops.control.for_loop` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_2026052_generic_ops_control_for_loop_with_break` | `generic_ops.control.for_loop_with_break` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_foreach_loop` | `generic_ops.control.foreach_loop` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_202_generic_ops_control_foreach_loop_with_break` | `generic_ops.control.foreach_loop_with_break` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_gate` | `generic_ops.control.gate` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_multi_gate` | `generic_ops.control.multi_gate` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_return` | `generic_ops.control.return` | singleton_1 | 1 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_sequence` | `generic_ops.control.sequence` | singleton_1 | 1 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_switch_enum` | `generic_ops.control.switch_enum` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_switch_int` | `generic_ops.control.switch_int` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_switch_name` | `generic_ops.control.switch_name` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_switch_string` | `generic_ops.control.switch_string` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_control_while_loop` | `generic_ops.control.while_loop` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_00_generic_ops_create_construct_object` | `generic_ops.create.construct_object` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_create_create_widget` | `generic_ops.create.create_widget` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_create_make_array` | `generic_ops.create.make_array` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_create_make_map` | `generic_ops.create.make_map` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_create_make_set` | `generic_ops.create.make_set` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_create_spawn_actor` | `generic_ops.create.spawn_actor` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_generic_ops_struct_select_break_struct` | `generic_ops.struct_select.break_struct` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527__generic_ops_struct_select_make_struct` | `generic_ops.struct_select.make_struct` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | preview_failure:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_struct_select_select` | `generic_ops.struct_select.select` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_transform_class_cast` | `generic_ops.transform.class_cast` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_generic_ops_transform_dynamic_cast` | `generic_ops.transform.dynamic_cast` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_0_generic_ops_transform_type_promotion` | `generic_ops.transform.type_promotion` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_abs` | `op.abs` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_bitwise_and` | `op.bitwise_and` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_bitwise_not` | `op.bitwise_not` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_bitwise_or` | `op.bitwise_or` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_bitwise_xor` | `op.bitwise_xor` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_and` | `op.boolean_and` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_nand` | `op.boolean_nand` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_nor` | `op.boolean_nor` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_not` | `op.boolean_not` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_or` | `op.boolean_or` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_boolean_xor` | `op.boolean_xor` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_cross` | `op.cross` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_cross3` | `op.cross3` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_add_datetime` | `op.datetime_add_datetime` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_add_timespan` | `op.datetime_add_timespan` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_equal` | `op.datetime_equal` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_greater` | `op.datetime_greater` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_greater_equal` | `op.datetime_greater_equal` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_less` | `op.datetime_less` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_less_equal` | `op.datetime_less_equal` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_not_equal` | `op.datetime_not_equal` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_subtract_datetime` | `op.datetime_subtract_datetime` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_datetime_subtract_timespan` | `op.datetime_subtract_timespan` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_dot` | `op.dot` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_dot3` | `op.dot3` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_equal_exact` | `op.equal_exact` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_equal_ignore_case` | `op.equal_ignore_case` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_intpoint_equal` | `op.intpoint_equal` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_max` | `op.max` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_min` | `op.min` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_modulo` | `op.modulo` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_near_equal` | `op.near_equal` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_negate` | `op.negate` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_not_equal_exact` | `op.not_equal_exact` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_not_equal_ignore_case` | `op.not_equal_ignore_case` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_string_append` | `op.string_append` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_op_transform_compose` | `op.transform_compose` | limited_real_nodes | 10 | 1 | 1 | PASS | 1/1 | none:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_schedule_latent_or_async_node` | `schedule.latent_or_async_node` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
| `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality_GraphWriteGenerality_E2E_20260527_005_schedule_timer_delegate_node` | `schedule.timer_delegate_node` | limited_real_nodes | 10 | 1 | 0 | FAIL | 0/1 | unsupported_intent:1 |
