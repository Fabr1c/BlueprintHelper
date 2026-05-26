# BlueprintHelper GraphWrite OpCoverage 清洗与能力拓展文档

日期：2026-05-26

## 0. 输入与清洗边界

输入证据：

- `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_OpCoverage_UESourceReadResult_20260525_CN.md`
- `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_OpCoverage_UESourceReadResult_PublicOps_20260525_CN.csv`
- `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_OpCoverage_UESourceReadResult_DetailRows_20260525_CN.csv`
- `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_OpCoverage_UESourceReadResult_ExcludedCandidates_20260525_CN.csv`

本次清洗只保留可由 BlueprintHelper 通过 TaskSpec/GraphWrite 自动执行的图生成能力。偏 UI 交互不纳入：拖拽节点、拖拽 Pin、右键呼出菜单、Slate/SGraphPanel/SGraphNode/SGraphPin 鼠标键盘事件、FUICommandList 命令触发等都不作为 GraphWrite 能力。

上下文边界：

1. `TaskSpec` 只能作为全局共享上下文，提供目标资产、目标图、执行选项、schema 版本等稳定输入。
2. 每个 `statement[]` 内部作为独立上下文；解析一个 statement 所需的 operator、typed pins、stable function id、owner/function、expected return、literal/default、target object evidence 必须在该 statement 或其内部 expression 中给出。
3. 如需跨 statement 共享类型别名、临时 symbol、operation catalog、enum path registry 或 UI selection/menu context，必须先和用户讨论，不在本次文档中默认扩展。

## 1. 当前实现基线

当前 `edit_blueprint_graph` TaskSpec schema 只约束 `graph_strategy` 和 `entries/replace/patches/merges`，具体 statement 能力由 `logic_spec` 进入 GraphWrite 语义层：`AgentFaceService/task-core/src/task/schema/task-schemas.ts`。

GraphWrite 语义 IR 已有 expression `Op`，但 statement 层没有独立 `Op` statement；op 目前是表达式能力：`BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`。

当前 `Op` 解析路径：

1. expression `kind=op` 进入 `FunctionActionCluster`。
2. `FunctionActionCluster` 将 `Semantic.Kind=Op` 转给 `FBlueprintHelperOperatorActionResolver`。
3. `FBlueprintHelperOperatorActionResolver` 只把 token 映射到 UE TypePromotion 顶层 operator spawner。

当前已实现的 public op 语义只有 10 个 TypePromotion 顶层 operator：

| 已实现 public_operation | UE op |
| --- | --- |
| `add` | `Add` |
| `subtract` | `Subtract` |
| `multiply` | `Multiply` |
| `divide` | `Divide` |
| `greater` | `Greater` |
| `greater_equal` | `GreaterEqual` |
| `less` | `Less` |
| `less_equal` | `LessEqual` |
| `equal` | `EqualEqual` |
| `not_equal` | `NotEqual` |

说明：`call` statement/expression 已可通过 callable/stable function evidence 生成普通函数节点，`convert`、`schedule`、`container_action` 也已有各自语义通道；但这些不等同于已提供 `kind=op` 的 public_operation 语义。下面的“缺失能力”只指尚未作为 GraphWrite public op 接入，不把已有 callable/convert/schedule/container 能力误判为完全未实现。

## 2. 可拓展但当前缺失的能力

清洗后适合 BlueprintHelper 且当前 `kind=op` 未实现的 public_operation 数量为 38 个：

- `commutative_function`：8 个 public_operation，16 条 detail evidence。
- `call_function_compact`：29 个 public_operation，52 条 detail evidence。
- `array_identical` 专用节点：1 个 public_operation，1 条 detail evidence。

### P0：Commutative Function Op

这些能力不应走 `FTypePromotion::GetOperatorSpawner()`，应通过 ActionDatabase/callable evidence 解析到稳定 UFUNCTION，并在需要时创建 `UK2Node_CommutativeAssociativeBinaryOperator`。

| public_operation | owner/function evidence | TaskSpec statement/expression 需求 | 主要风险 |
| --- | --- | --- | --- |
| `bitwise_and` | `UKismetMathLibrary::And_Int64Int64/And_IntInt` | `kind=op` + `operator` + left/right typed pins 或 stable function id | 不得和 boolean `and` 混淆 |
| `bitwise_or` | `UKismetMathLibrary::Or_Int64Int64/Or_IntInt` | 同上 | 不得和 boolean `or` 混淆 |
| `boolean_and` | `UKismetMathLibrary::BooleanAND` | bool typed pins | 不提供短路语义 |
| `boolean_or` | `UKismetMathLibrary::BooleanOR` | bool typed pins | 不提供短路语义 |
| `boolean_nand` | `UKismetMathLibrary::BooleanNAND` | bool typed pins | 应作为布尔域 op |
| `max` | `UKismetMathLibrary::BMax/FMax/Max/MaxInt64` | typed pins 或 stable function id | 多数值类型必须确定性选择 |
| `min` | `UKismetMathLibrary::BMin/FMin/Min/MinInt64` | typed pins 或 stable function id | 多数值类型必须确定性选择 |
| `string_append` | `UKismetStringLibrary::Concat_StrStr` | string typed pins | 不得和数值 `add/+` 混淆 |

建议新增解析边界：在 `Op` resolver 中先识别 TypePromotion 10 个顶层 operator；否则进入 `op_callable` resolver，通过 `owner_class + function_name`、stable id、typed pins 和 metadata 解析 callable/commutative op。不要把 alias token 当作最终证据。

### P1：Compact Call Function Op

这些能力应通过 `UK2Node_CallFunction` 和 stable function id 生成；`CompactNodeTitle` / `ScriptOperator` 只能作为 UI 显示别名证据，不能作为最终选择依据。

| 能力域 | public_operation |
| --- | --- |
| 布尔/位运算 | `boolean_not`, `boolean_xor`, `boolean_nor`, `bitwise_not`, `bitwise_xor` |
| 数值/向量/几何 | `abs`, `modulo`, `negate`, `dot`, `dot3`, `cross`, `cross3`, `near_equal`, `intpoint_equal`, `transform_compose` |
| 严格/忽略大小写比较 | `equal_exact`, `not_equal_exact`, `equal_ignore_case`, `not_equal_ignore_case` |
| DateTime/Timespan | `datetime_add_datetime`, `datetime_add_timespan`, `datetime_subtract_datetime`, `datetime_subtract_timespan`, `datetime_equal`, `datetime_not_equal`, `datetime_greater`, `datetime_greater_equal`, `datetime_less`, `datetime_less_equal` |

TaskSpec/expression 最小需求：

```json
{
  "kind": "op",
  "operator": "modulo",
  "op_spawn_path": "call_function_compact",
  "stable_function_id": "/Script/Engine.KismetMathLibrary:Percent_IntInt",
  "args": {
    "A": { "kind": "literal", "type": "int", "value": "7" },
    "B": { "kind": "literal", "type": "int", "value": "3" }
  },
  "type": "int"
}
```

如果不想扩展 public op schema，也可以先要求 agent 使用 `kind=call` + stable function id；但那只能算 callable 兜底，不算 `kind=op` 覆盖闭环。

### P2：Special Node Op

| public_operation | 结论 | 原因 |
| --- | --- | --- |
| `array_identical` | 可作为后续专用 builder 候选 | evidence 是 `UKismetArrayLibrary::Array_Identical` + `UK2Node_CallArrayFunction`，语义为数组值比较，不依赖 UI 操作 |
| `enum_equal` | 本次排除 | evidence 依赖 Editor `GetMenuActions`/专用菜单节点语境；当前 statement 独立上下文不模拟右键 Action Menu |
| `enum_not_equal` | 本次排除 | 同上 |

`array_identical` 必须要求 array typed pin evidence；不能伪装成 TypePromotion，也不能用 `equal` 静默替代。

## 3. 不适合项与原位标记

已经在原 evidence 中做原位标记：

| 文件 | 标记项 | 标记结果 |
| --- | --- | --- |
| `...PublicOps_20260525_CN.csv` | `enum_equal`, `enum_not_equal` | `included_in_graphwrite_op=no`，原因标注为依赖 Editor `GetMenuActions`/右键 Action Menu 语境 |
| `...DetailRows_20260525_CN.csv` | `enum_equal`, `enum_not_equal` | 同上 |
| `...DetailRows_20260525_CN.csv` | `USlateBlueprintLibrary::EqualEqual_SlateBrush` | `included_in_graphwrite_op=no`，原因标注为 Slate/UMG UI 语义 |
| `...UESourceReadResult_20260525_CN.md` | 同上三类行 | 表格原位改为 `no` 并写入排除原因 |

保留不改为 `no`、但需要后续确认的边界：

| 边界项 | 当前处理 | 原因 |
| --- | --- | --- |
| `UInputDeviceLibrary::EqualEqual_InputDeviceId/NotEqual_InputDeviceId` | 暂不排除 | 它可能是 gameplay runtime typed comparison，不等同于 UI 拖拽/菜单操作 |
| `UInputDeviceLibrary::EqualEqual_PlatformUserId/NotEqual_PlatformUserId` | 暂不排除 | 同上 |
| `UKismetInputLibrary::EqualEqual_InputChordInputChord/EqualEqual_KeyKey` | 暂不排除，需用户确认 | 输入/按键域是否纳入 GraphWrite public op 需要业务边界确认 |

`ExcludedCandidates` 中已有的 4 项维持原判定：

- `convert_numeric`、`convert_string_text_name`：应归 `convert_function`，不归 `kind=op`。
- `array_map_set_mutation`：当前架构已有 `container_action` 语义入口，不应混入普通 op。
- `validity_predicate`：应作为 predicate/function 能力讨论，不作为 operator/op-like 值节点。

## 4. 后续实现建议

建议按三个增量推进：

1. 扩展 `Op` resolver：保留现有 TypePromotion 10 个直连路径，新增 `op_callable` resolver 支持 `commutative_function` 与 `call_function_compact`。
2. 扩展 TaskSpec/GraphWrite 语义字段：在 expression 内显式承载 `op_spawn_path`、`stable_function_id`、`owner_class`、`function_name`、`argument_pin_types`、`expected_return_pin_type`，不增加跨 statement 隐式上下文。
3. 单独评估 `array_identical` dedicated builder：只有在 array typed pin evidence 完整时才允许生成。

不建议做的实现：

- 不通过拖拽、拖拽 Pin、右键菜单、Slate command 或选中态模拟来生成节点。
- 不让 UI widget 本地分支维护 GraphWrite op 语义。
- 不把 `CompactNodeTitle`、alias token、menu display name 当作唯一证据。
- 不为了支持 `enum_equal/enum_not_equal` 临时加入全局 menu context；如果要支持 enum 专用节点，应先讨论 statement-local enum path evidence 与 dedicated builder 设计。
