# BlueprintHelper GraphWrite GenericOps TaskSpec 边界清洗与能力拓展文档

日期：2026-05-26

## 0. 清洗结论

本次清洗把 GenericOps 能力从“模拟 UE 编辑器 UI”收敛为“TaskSpec 驱动的普通 Blueprint Graph 写入”。可保留的能力必须满足三个条件：

1. 可以由 `TaskSpec`、当前目标 Blueprint/Graph、statement-local evidence、UE runtime readback 稳定表达。
2. 不依赖 Slate 事件、鼠标坐标、拖拽节点、拖拽 Pin、右键菜单展开、搜索框排序、Content Browser/MyBlueprint/SCS 的当前选中态。
3. 成功条件不是“菜单里存在某项”，而是“节点/连接已创建，关键 pin 已重建并 readback，wildcard 已收敛，编译/诊断结果可验证”。

上下文边界固定如下：

| 上下文层级 | 允许内容 | 不允许内容 |
| --- | --- | --- |
| `TaskSpec` 全局上下文 | schema 版本、目标资产、目标图、执行/Review/compile 策略、显式资产/类型/函数路径字典。 | 当前 UI 选中对象、右键菜单状态、拖拽来源、跨 statement 隐式推断状态。 |
| 单个 `statement[]` | 该 statement 自己的 operation、输入表达式、pin/type evidence、class/struct/enum/function path、handler reference、asset path、readback expectations。 | 从上一个 statement 偷用未声明的 pin context、menu context、临时 UI selection、未声明 alias。 |
| expression 内部 | 可嵌套 `construct/deconstruct/select/create/convert/container_action/op/call` 并在同一 expression 树内传递类型。 | 跨 sibling statement 的隐式符号共享。 |

如果后续需要跨 statement 共享类型别名、operation catalog、symbol registry、UI selection/menu context，必须先单独和用户讨论，不在本清洗版默认扩展。

## 1. 当前实现基线（静态源码核对）

本节只基于当前归档源码静态核对，没有运行 UE 编辑器烟测。

| 能力域 | 当前实现已有 | 当前没有或未完备 |
| --- | --- | --- |
| `control` | `branch`、`sequence`、`return`，以及 expression 级 `select` 的 singleton provider。源码：`BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`、`BlueprintHelperControlFragmentBuilder.cpp`。 | `switch_int/string/name/enum`、`multi_gate`、StandardMacros 宏实例族（`do_once`、`do_n`、`gate`、`flip_flop`、loop 系列）没有 first-class resolver/builder。 |
| `create` | `spawn_actor`、`create_widget`、`construct_object`、`make_array`、`make_map`、`make_set`、`asset_action`。源码：`BlueprintHelperGenericCreateActionResolver.cpp`。 | async action factory create、function-backed create、expose-on-spawn pin readback/assignment 的 first-class coverage 不完整。 |
| `convert/transform_operation` | `dynamic_cast`、`class_cast`、`type_promotion`。源码：`BlueprintHelperGenericTransformScheduleActionResolver.cpp`。 | function-backed conversion、BlueprintAutocast、schema link-time automatic conversion readback、soft object/class conversion 未统一成 first-class transform/link policy。 |
| `schedule/schedule_operation` | `timer_delegate_node`、`latent_or_async_node`，通过 projected ActionDatabase evidence 重新验证。 | timer by function/handle/clear/pause/unpause 等普通 UFunction schedule vocabulary 没有 first-class 分类；timer by event/async output handler 不应由 GraphWrite 隐式创建。 |
| `container_action` | array 12 项、map 8 项、set 6 项。源码：`task-schemas.ts`、`BlueprintHelperContainerActionVocabulary.cpp`。 | UE KismetArray/Map/Set library 的大量 callable surface 未覆盖，见第 3 节。 |
| `construct/deconstruct/select` | struct make/break、native make/break fallback、`UK2Node_Select` builder 已存在基础路径。 | `SetFieldsInStruct`、field pin policy/readback hardening、enum/object/class/soft/interface select proof、split/recombine pin 归属边界仍需收敛。 |
| `op` | 10 个 TypePromotion 顶层 operator。 | 38 个可做但未接入的 op 已在 `BlueprintHelper_GraphWrite_OpCoverage_CleanedCapabilityExtension_20260526_CN.md` 单独清洗。 |

## 2. 不适合 BlueprintHelper 当前 GraphWrite 的能力

以下内容只能作为 UE 源码/设计背景，不能作为当前 BlueprintHelper 可执行能力：

| 不适合项 | 原因 | 文档标记方式 |
| --- | --- | --- |
| 拖拽节点、拖拽 Pin、`DraggedFromPins` 真实 UI 来源 | 依赖 Slate/SGraphPanel/SGraphPin 鼠标状态；MCP/TaskSpec 无法稳定复现。 | 标为“UI-only / reference only”，替换为 statement-local typed pin evidence。 |
| 右键呼出菜单、搜索框、菜单项排序、display text alias | 这是 UE 编辑器交互路径；GraphWrite 只能使用稳定 spawner/function/class/type evidence。 | 标为“不要模拟菜单；只消费投影证据”。 |
| MyBlueprint/SCS/Content Browser 当前选中对象 | 当前选中态不是 TaskSpec 的稳定输入。 | 需要显式 component path / asset path / object reference。 |
| 隐式创建 event/custom event/delegate handler/signature | 会扩大 GraphWrite ownership，且可能产生签名错误。 | 归 `EventDelegate` / `BlueprintSignature`；GraphWrite 只使用显式 handler reference。 |
| split/recombine pin 作为高层 GraphWrite statement | 它是 schema pin operation，不是普通 statement 节点写入。 | 可作为未来 PinOperation 工具；GraphWrite 只读回结果。 |
| UMG Designer tree、Animation Blueprint 专属节点、Material/Niagara/Control Rig、Timeline editor 专用编辑 | 非普通 EventGraph / FunctionGraph K2 graph statement 范围。 | 标为 future non-GraphWrite tools。 |
| 基于 display name 的 action allowlist | 本地化/菜单分类/搜索排序不稳定。 | 必须使用 object/function/class/struct/enum/asset path 与 spawner signature。 |

## 3. 适合 BlueprintHelper、且当前实现没有的能力拓展

### 3.1 ControlFlow 拓展

#### 适合立即纳入 TaskSpec-bound 设计的 direct K2Node

| public operation | UE 节点族 | 所需 statement-local evidence | 当前缺口 |
| --- | --- | --- | --- |
| `switch_int` | `UK2Node_SwitchInteger` | selection expression、case int values、default body、case body 数量。 | 无 first-class contract/resolver/builder/readback。 |
| `switch_string` | `UK2Node_SwitchString` | selection expression、case string values、default body。 | 同上。 |
| `switch_name` | `UK2Node_SwitchName` | selection expression、case name values、default body。 | 同上。 |
| `switch_enum` | `UK2Node_SwitchEnum` | enum path、selection expression、enum case body 映射。 | 同上；enum path 不能从 UI menu 推断。 |
| `multi_gate` | `UK2Node_MultiGate` | output count、loop/random/start index/reset input、每个 output body。 | 缺动态 output pin readback。 |

#### 适合但需要先补 macro source evidence 的 StandardMacros

| public operation | UE 路径 | 所需 evidence | 本次状态 |
| --- | --- | --- | --- |
| `do_once` | `UK2Node_MacroInstance` / StandardMacros.DoOnce | macro graph path、pin shape snapshot、reset/then pin 名称。 | 适合，但 discussion-gated：先确认 TaskSpec 是否允许携带 macro graph path/pin shape。 |
| `do_n` | StandardMacros.Do N | macro graph path、N/reset/counter pins。 | 同上。 |
| `gate` | StandardMacros.Gate | enter/open/close/toggle/start closed/exit pins。 | 同上。 |
| `flip_flop` | StandardMacros.FlipFlop | exec in、A/B、bool output pins。 | 同上。 |
| `for_loop` | StandardMacros.ForLoop | first/last index、loop body、completed pins。 | 同上。 |
| `for_loop_with_break` | StandardMacros.ForLoopWithBreak | break pin、loop body、completed pins。 | 同上。 |
| `foreach_loop` | StandardMacros.ForEachLoop | array input、array element type、index/body/completed pins。 | 同上。 |
| `foreach_loop_with_break` | StandardMacros.ForEachLoopWithBreak | array input、break/body/completed pins。 | 同上。 |
| `while_loop` | StandardMacros.WhileLoop | condition/body/completed pin shape。 | 同上。 |

建议不要把这些能力称为“右键菜单 parity”。文档与代码应称为“TaskSpec-bound control operation parity”：操作名、case/body、macro source、pin shape 都来自 statement-local evidence 或 UE runtime readback。

### 3.2 ContainerAction 拓展

当前 V1 只覆盖 26 项。UE evidence 显示可扩到更完整的 KismetArray/Map/Set callable surface。建议按 `P0 wildcard/core`、`P1 read-only/query`、`P2 fixed-type sort/filter` 分批。

| collection | 当前已有 | 适合补齐的 missing operations |
| --- | --- | --- |
| array | `get,set,add,add_unique,append,insert,remove_item,remove_index,clear,contains,find,length` | `shuffle`, `shuffle_from_stream`, `identical`, `resize`, `reverse`, `is_empty`, `is_not_empty`, `last_index`, `swap`, `filter_array`, `is_valid_index`, `random`, `random_from_stream`, `sort_string`, `sort_name`, `sort_byte`, `sort_int`, `sort_int64`, `sort_float` |
| map | `add,remove,find,contains,keys,values,clear,length` | `is_empty`, `is_not_empty`, `get_key_value_by_index`, `get_last_index` |
| set | `add,remove,contains,clear,length,to_array` | `add_items`, `remove_items`, `is_empty`, `is_not_empty`, `intersection`, `union`, `difference`, `get_item_by_index`, `get_last_index` |

TaskSpec 规则：每个 operation 必须在 statement/expression 内携带 `container_kind`、`operation`、`target`、必要 roles、element/key/value type evidence。不能从右键菜单或拖拽 pin 猜类型。执行成功必须 readback UFunction path、collection pin type、wildcard replacement、返回 pin类型和 compile diagnostics。

### 3.3 Transform / Convert 拓展

| missing ability | 适合性 | 最小 evidence | 边界 |
| --- | --- | --- | --- |
| function-backed conversion | 适合 | source type、target type、UFunction path、metadata（如 `BlueprintAutocast`）。 | 可作为 explicit transform node；不依赖菜单文本。 |
| link-time automatic conversion | 适合 | source pin、target pin、schema connection policy、readback inserted node 或 direct link proof。 | 属于 link builder policy；只有用户显式要求或 schema 自动插入后 readback，才记录为 transform result。 |
| numeric/string/name/text/enum conversions | 适合 | stable function id 或 conversion metadata、source/target pin categories。 | 不归普通 `op`。 |
| object/class -> soft object/soft class | 适合 | source object/class type、target soft type、conversion function/node evidence。 | 必须显式目标类型；不能从 UI asset picker 推断。 |
| interface/object dynamic cast variants | 已有基础但需硬化 | target class/interface path、pure/impure cast mode、success/failure pins。 | target interface path 必须 statement-local。 |

### 3.4 Create / AssetAction 拓展

| missing ability | 适合性 | 最小 evidence | 不做的事 |
| --- | --- | --- | --- |
| async action create | 适合插入节点 | proxy factory function path、proxy class、node class、delegate output pins readback。 | 不隐式创建 async output delegate handler。 |
| function-backed create/spawn/construct | 适合 | UFunction path、return type、world context/latent metadata、class-determines-output metadata。 | 不通过 display name 匹配 `Create ...`。 |
| expose-on-spawn pin assignment/readback | 适合 | class path、property path、pin name/type、default/link value。 | 不猜测隐藏/高级 pin；创建后 readback 为准。 |
| asset-backed graph node | 适合 | asset path/FAssetData、node class、spawner class、asset reference filter proof。 | 不负责资产创建/导入/保存；不读取 Content Browser 当前选中。 |

### 3.5 Schedule 拓展

| missing ability | 适合性 | 所需 evidence | 归属边界 |
| --- | --- | --- | --- |
| timer by function name | 适合 | UFunction path、object ref、function name string、time/looping/initial delay pins。 | function name 是否存在由 validator 报告；GraphWrite 不创建函数。 |
| timer by handle / clear / pause / unpause | 适合 | UFunction path、timer handle pin、object/function/delegate pins。 | 纯 call node / schedule node。 |
| delay / retriggerable delay / delay until next tick | 适合 | latent UFunction path、graph_latent_allowed、duration/continuation pins。 | latent continuation 由 K2/GraphWrite link 处理。 |
| generic latent function call | 适合 | UFunction path、`Latent`/`LatentInfo`/`WorldContext` metadata、continuation pins。 | 不把任意 latent call 混入普通 call review；需要 schedule evidence。 |
| async proxy output delegate connection | 只适合使用显式 handler | handler reference、signature proof、delegate output pin。 | handler/signature 创建归 EventDelegate/BlueprintSignature。 |

### 3.6 Construct / Deconstruct / Select 硬化

这些能力已有基础，不应作为“全新缺失”，但仍有适合补齐的缺口。

| missing/hardening item | 适合性 | 最小 evidence | 边界 |
| --- | --- | --- | --- |
| `SetFieldsInStruct` | 适合 | struct path、selected field paths、input values、enabled optional pins readback。 | 不从 UI optional pin panel 选择字段。 |
| Make/Break struct field policy readback | 适合 | struct path、visible field policy、native make/break helper evidence。 | 成功以创建后 pin set 为准。 |
| Select enum/object/class/soft/interface proof | 适合 | index type/enum path、option values/links、expected result type。 | wildcard 未收敛必须失败。 |
| Split/recombine pin | 当前不纳入 GraphWrite statement | pin path、schema pin operation、readback cluster。 | 建议未来 PinOperation 工具处理。 |

## 4. 建议的 TaskSpec 片段

### 4.1 switch_enum statement

```json
{
  "kind": "control",
  "control_operation": "switch_enum",
  "enum_path": "/Script/Engine.ECollisionChannel",
  "selection": { "kind": "field", "path": "TraceChannel" },
  "cases": [
    { "name": "ECC_Visibility", "body": [ { "kind": "call", "stable_function_id": "..." } ] },
    { "name": "ECC_Camera", "body": [ { "kind": "call", "stable_function_id": "..." } ] }
  ],
  "default": []
}
```

### 4.2 array random expression

```json
{
  "kind": "container_action",
  "container_kind": "array",
  "operation": "random",
  "target": { "kind": "field", "path": "Candidates" },
  "element_type": { "category": "object", "object_path": "/Script/Engine.Actor" }
}
```

### 4.3 explicit conversion

```json
{
  "kind": "convert",
  "transform_operation": "function_conversion",
  "source_type": { "category": "object", "object_path": "/Script/Engine.Texture" },
  "target_type": { "category": "soft_object", "object_path": "/Script/Engine.Texture" },
  "stable_function_id": "/Script/Engine.KismetSystemLibrary:Conv_ObjectToSoftObjectReference",
  "value": { "kind": "field", "path": "TextureObject" }
}
```

### 4.4 async action create without handler creation

```json
{
  "kind": "create",
  "create_operation": "async_action",
  "proxy_factory_function": "/Script/Engine.AssetManager:AsyncLoadPrimaryAsset",
  "proxy_class": "/Script/Engine.AsyncActionLoadPrimaryAsset",
  "delegate_outputs": {
    "Completed": { "handler_ref": "Existing_LoadCompleted_Handler" }
  }
}
```

缺少 `handler_ref` 时应返回 blocked / needs_more_semantic_context，而不是创建 CustomEvent。

## 5. 原位标记清单

本次已在以下 active docs 加入 `[清洗标记 2026-05-26]`：

| 文档 | 标记目的 |
| --- | --- |
| `Develop/Evidence/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md` | 把 ActionMenu/右键/selected/DraggedFromPins 等降级为 evidence source，不作为执行路径；标出各能力的 TaskSpec-bound 可用边界。 |
| `Develop/Design/UE_NodeSpawner_RightClick_Spawn_Chain_Analysis_20260521_CN.md` | 标明右键链路是源码参考，不模拟 UI。 |
| `Develop/Design/BlueprintHelper_UEActionContext_InputMatrix_20260522_CN.md` | 将 `DraggedFromPins`、`SelectedObjects` 等改判为不可稳定输入，只能用 statement-local typed evidence / explicit asset/component path 替代。 |
| `Develop/Plan/BlueprintHelper_GraphWrite_BroadControlFlowPlan_20260525_CN.md` | 把 broad control 的目标改为 TaskSpec-bound control operation，不以右键菜单 parity 为验收名义。 |
| `Develop/Plan/BlueprintHelper_GraphWrite_GenericScheduleSuccessPathPlan_20260525_CN.md` | 锁定 handler/signature 不由 GraphWrite 隐式创建。 |
| `Develop/Plan/BlueprintHelper_GraphWrite_AssetActionReviewPolicy_GraphBlockPlan_20260525_CN.md` | 锁定 asset action 只消费显式 asset evidence，不消费当前选中资产。 |
| `Develop/Plan/BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md` | 标出 V1 已有集合与待扩展集合；不走 UI menu。 |
| `Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md` | 增补 2026-05-26 TaskSpec-bound 清洗约束。 |

## 6. 执行验收口径

新增或扩展任何能力时，至少需要通过以下验收：

1. TaskSpec schema/validator 明确拒绝缺失 evidence 的 statement。
2. resolver 不使用 display text / menu string 作为唯一证据。
3. builder 创建后执行 pin readback，记录 node class、source object/function/type、pin name/type/direction、dynamic pin count。
4. wildcard residual、schema rejection、handler missing、latent not allowed、asset reference mismatch 必须进入 DebugBundle。
5. `statement[]` 之间不新增隐式共享上下文；需要共享时必须有明确引用字段或先讨论新上下文模型。
6. 相关 active docs 同步标注，不修改 `Develop/v*` 归档文档作为最新依据。
