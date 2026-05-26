# BlueprintHelper GraphWrite Generic Ops UE Editor Capability Source Read Result 20260525 CN

## 执行摘要

本次只读核查基于上传任务文档与 `引擎源码.7z` 中抽取的 UE 5.6 Engine Source 子集执行，目标是判断 `construct`、`deconstruct`、`select`、`control`、`create`、`convert + transform_operation`、`schedule + schedule_operation`、`asset_action`、`container_action` 是否能接近普通 Blueprint EventGraph / FunctionGraph 右键菜单可创建节点能力的 80% 覆盖，并给出实现边界。结论是：`construct`/`deconstruct`、`select`、`container_action` 在引擎事实层面已经具备接近 80% 的通用化基础，但当前 BlueprintHelper 仍应补齐 ActionDatabase evidence、UE pin inference 委托、readback 与 compile proof；`control`、`create`/`asset_action`、`convert`/`transform_operation`、`schedule`/`schedule_operation` 低于 80%，原因不是缺少 UE 入口，而是编辑器入口横跨 direct K2Node、macro-backed node、function-backed node、asset spawner、autocast、latent/async metadata 与 delegate handler/signature ownership；本次没有发现阻塞这些能力设计的 UE 源码事实，但 StandardMacros 的精确 pin 形状需要在编辑器运行时或 Engine Content 资产层读取，BlueprintHelper 项目源码未随输入提供，因此“当前覆盖”只能以任务交接文档声明为基线，不能声称完成源码级逐文件核验。

## 输入与执行范围

- 任务文档：`BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadTask_20260525_CN.md`。
- 引擎源码：`/mnt/data/引擎源码.7z`，抽取后用于检索的子集位于 `/mnt/data/engine_subset`。
- 输出目标：`BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`。
- 范围：普通 Blueprint EventGraph / FunctionGraph 右键菜单；不纳入 Animation Blueprint 专属节点、UMG Designer 控件树、Material graph、Niagara graph、Control Rig graph。
- 只读原则：未修改 BlueprintHelper 代码，未修改引擎源码。
- 重要限制：当前会话文件系统没有 `D:\UEProjects\Template\Plugins\BlueprintHelper` 项目源码，因此任务中要求对 `AgentFaceService/task-core` 与 `BlueprintHelper/Source/BlueprintHelper` 的 `rg` 源码核验无法在本容器内执行；报告内关于当前 BlueprintHelper 覆盖的判断来自任务交接文档的 Current Status，而非源码级复核。

## 源码证据索引

| 证据文件 | 内容 |
| --- | --- |
| `/mnt/data/evidence/action_menu_key_numbered.txt` | Action menu 构建、ActionDatabase 注册、NodeSpawner 创建/Invoke、ActionFilter 过滤路径。 |
| `/mnt/data/evidence/construct_select_key_numbered.txt` | MakeStruct、BreakStruct、SetFieldsInStruct、SplitPin、Select pin lifecycle。 |
| `/mnt/data/evidence/container_make_key_numbered.txt` | MakeArray、MakeMap、MakeSet、MakeContainer 默认 pin、重建与 add-pin 行为。 |
| `/mnt/data/evidence/control_key_numbered.txt` | Branch、Sequence、Switch、MultiGate、MacroInstance/StandardMacros 证据。 |
| `/mnt/data/evidence/create_key_numbered.txt` | SpawnActor、ConstructObject、CreateWidget、AssetNodeSpawner、AsyncAction 证据。 |
| `/mnt/data/evidence/transform_key_numbered.txt` | DynamicCast、ClassDynamicCast、schema conversion/autocast、soft object/class conversion。 |
| `/mnt/data/evidence/schedule_header_numbered.txt` | Delay、RetriggerableDelay、MoveComponentTo、timer by event/handle/function name。 |
| `/mnt/data/evidence/container_headers_rg.txt` | KismetArrayLibrary、BlueprintMapLibrary、BlueprintSetLibrary callable 函数清单。 |

## Editor Action Inventory

### Action menu ground truth

普通蓝图右键菜单不是固定字符串列表，而是由 Kismet UI 收集上下文后通过 ActionDatabase、menu builder、filter 与 spawner 生成：

- `SBlueprintActionMenu::OnGetActionList()` 构造 `FBlueprintActionContext`，调用 `FBlueprintActionMenuUtils::MakeContextMenu()`，并允许 schema 插入额外 action。证据：`Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:531-569`。
- `FBlueprintActionMenuUtils::MakeContextMenu()` 组合 context-sensitive flag、target class、selected objects、asset reference filter、component/level actor filters。证据：`BlueprintActionMenuUtils.cpp:491-620`。
- `FBlueprintActionMenuBuilder::RebuildActionList()` 从 `FBlueprintActionDatabase::Get().GetAllActions()` 取 registry，逐个 `UBlueprintNodeSpawner` 生成 menu item。证据：`BlueprintActionMenuBuilder.cpp:566-622`。
- `FBlueprintActionDatabase::AddClassFunctionActions()` 为可调用 UFunction 创建 `UBlueprintFunctionNodeSpawner`，并为可放置为事件的函数创建 event spawner。证据：`BlueprintActionDatabase.cpp:654-718`。
- `FBlueprintActionDatabase::AddClassCastActions()` 为允许的 class 创建 `UK2Node_DynamicCast` 与 `UK2Node_ClassDynamicCast` spawner，并通过 customize delegate 设置 `TargetType`。证据：`BlueprintActionDatabase.cpp:810-834`。
- `FBlueprintActionDatabase::GetNodeSpecificActions()` 调用每个非抽象 `UK2Node` CDO 的 `GetMenuActions()`。证据：`BlueprintActionDatabase.cpp:909-918`。
- `UBlueprintNodeSpawner::Create()` 保存 node class 与 customize delegate；`Invoke()` 调用 spawn；`SpawnEdGraphNode()` 创建 node、设置 guid/position、执行 customize delegate、`AllocateDefaultPins()`、`PostPlacedNewNode()` 并加入 graph。证据：`BlueprintNodeSpawner.cpp:51-66`、`:275-277`、`:325-354`。
- `FBlueprintActionFilter::IsFiltered()` / `IsFilteredByThis()` 对 action 逐个执行 rejection tests 与扩展 menu filters。证据：`BlueprintActionFilter.cpp:2297-2415`。

### Required evidence fields

| Field | Meaning | GraphWrite 投影建议 |
| --- | --- | --- |
| `node_class` | 编辑器 action menu 最终选择的 `UK2Node` class。 | 存为稳定 class path，例如 `/Script/BlueprintGraph.K2Node_Select`。 |
| `spawner_class` | `UBlueprintNodeSpawner` 子类或 factory path。 | 存 spawner class、factory source、customize delegate 关键参数。 |
| `action_source` | Function、macro、asset、struct、enum、cast、async proxy 或 schema action source。 | 用 object path/function path/asset data/struct path/enum path，不用 display text。 |
| `filter_reason` | action 在 EventGraph / FunctionGraph 合法的原因。 | 记录 schema compatibility、graph type、pin context、latent/impure/pure 限制。 |
| `menu_signature` | 可投影到 TaskSpec evidence 的稳定菜单签名。 | `{node_class, spawner_class, source_path, source_kind, target_graph_schema, pin_context}`。 |
| `readback_signature` | 创建与 pin 重建后可观察的稳定数据。 | 节点 class、source member、pin names/types/directions、dynamic pin count、compile status。 |

### Editor menu action families found

| Action family | Editor source | Spawner / node source | Normal graph legality | Stable menu signature | Readback signature |
| --- | --- | --- | --- | --- | --- |
| Node-specific K2 nodes | `UK2Node::GetMenuActions()` via ActionDatabase | `UBlueprintNodeSpawner::Create<UK2Node_X>()` | `FBlueprintActionFilter` + schema compatibility | node class + optional source object | node class + pin set |
| Function calls | `CanUserKismetCallFunction()` | `UBlueprintFunctionNodeSpawner::Create(UFunction)` | UFunction flags, graph type, latent/impure filter | function path + owning class | `UK2Node_CallFunction` + function reference + pins |
| Events | `FunctionCanBePlacedAsEvent()` | `UBlueprintEventNodeSpawner` | event legality, graph type | function path + event source | event node class + delegate/function ref |
| Casts | `AddClassCastActions()` | `UBlueprintNodeSpawner<UK2Node_DynamicCast/ClassDynamicCast>` | allowable Blueprint variable type and class permission | target class path + cast node class | target type, input object/class pin, success/fail pins |
| Macro instances | `AddBlueprintGraphActions()` / macro library | factory macro node spawner -> `UK2Node_MacroInstance` | graph/schema filter + macro source graph | macro graph object path + library path | macro graph reference + expanded pins |
| Struct make/break/set | node-specific `GetMenuActions()` | make/break/set fields K2 nodes | struct `CanBeMade` / `CanBeBroken` / schema filter | struct path + operation | node class + struct pin + visible field pins |
| Container make | node-specific `GetMenuActions()` | `UK2Node_MakeArray/Map/Set` | K2 schema action + wildcard/concrete pin inference | container kind + pin types | output container pin + input element/key/value pins |
| Container library calls | native UFunction action | `UK2Node_CallFunction` through function spawner | function filter + custom thunk metadata | UFunction path + wildcard metadata | call function + resolved wildcard collection type |
| Asset-backed nodes | ActionDatabase asset actions | `UBlueprintAssetNodeSpawner` | asset reference filter + graph schema | asset data + node class/spawner class | created node class + asset reference + stable pins |
| Async actions | `RegisterClassFactoryActions<UBlueprintAsyncActionBase>` | `UK2Node_AsyncAction` | factory UFunction + async proxy legality | proxy factory function + proxy class | async node + activate/factory data + delegate output pins |
| Automatic conversions | K2 schema link path | schema inserts conversion node | `CanCreateConnection()` / `TryCreateConnection()` | source pin type + target pin type + conversion UFunction/node | inserted conversion node or direct link proof |

## BlueprintHelper Current Coverage

此表为本次任务交接文档的基线解读。由于 BlueprintHelper 项目源码未挂载，以下“当前覆盖”不作源码级确认。

| Capability | 任务交接基线 | 本次 UE 源码核查后的判断 | 主要缺口 |
| --- | --- | --- | --- |
| `construct` | 部分覆盖 | UE 侧可通过 MakeStruct/MakeContainer 系列和 split pin schema 覆盖大部分；接近 80% 的前提是 GraphWrite 走 spawner + readback，而不是固定节点名。 | struct metadata、optional pin policy、native make helper、split pin 归属。 |
| `deconstruct` | 部分覆盖 | BreakStruct 与 split pin 机制清晰；接近 80% 的前提是字段可见性与 native break helper 回读完善。 | field pin policy、native break helper、split struct pin overlap。 |
| `select` | 部分覆盖 | UE `UK2Node_Select` 已内置 wildcard、enum、index/option/return pin 推断；接近 80% 取决于是否让 UE 完成 pin 推断并做失败回读。 | enum/object/class/soft/interface 类型 readback、wildcard 未收敛失败策略。 |
| `control` | 未完整覆盖 | direct K2Node 只覆盖 Branch/Sequence/Switch/MultiGate 等；标准循环和 Gate/DoOnce/FlipFlop 多为 StandardMacros 宏实例，当前低于 80%。 | macro source evidence、StandardMacros pin shape、switch variants、dynamic pin count。 |
| `create` | 部分覆盖 | SpawnActor/ConstructObject/CreateWidget/MakeContainer/AsyncAction/asset spawner 均存在，但固定 create list 不能覆盖 function-backed create 和 async proxy。 | async create、asset action node、function-backed create、expose-on-spawn pin proof。 |
| `convert + transform_operation` | 部分覆盖 | DynamicCast/ClassDynamicCast、BlueprintAutocast、schema automatic conversion 均存在；当前低于 80% 的关键是 link-time conversion 与 function conversion 未统一。 | automatic conversion insertion proof、specialized conversion、soft refs、function-backed conversion。 |
| `schedule + schedule_operation` | 部分覆盖 | Delay/timer/latent/async 源码路径清楚；低于 80% 的关键是 handler/signature 创建不应由 GraphWrite 隐式承担。 | timer by event handler、latent continuation、async proxy delegates、timeline ownership。 |
| `asset_action` | 部分覆盖 | UE asset node spawner 可作为通用入口；Review surface 应收敛为 `graph_block`。 | asset action menu 分类、runtime revalidation、DebugBundle 低层 evidence。 |
| `container_action` | 部分覆盖 | UE KismetArray/Map/Set callable 函数完整可枚举；接近 80% 的前提是 first-class vocabulary 覆盖所有 custom thunk + wildcard readback。 | 缺失 callable operation、sort/filter 特例、internal Get 节点入口。 |

## Coverage Matrix

### Construct / Deconstruct

| Node family | Editor behavior | Current support baseline | Missing work | Proposed owner |
| --- | --- | --- | --- | --- |
| Make struct | `UK2Node_MakeStruct` 用 `FMakeStructPinManager` 根据 `BlueprintVisible`、read-only、metadata、default value 创建输入 field pins；`GetMenuActions()` 通过 `CanBeMade` 注册。证据：`K2Node_MakeStruct.cpp:58-166`、`:287-335`。 | 部分覆盖。 | 将 struct path、visible/hidden field policy、默认值、native make helper 转为 evidence；创建后读取 output struct pin 和 field pins。 | GraphWrite。 |
| Break struct | `UK2Node_BreakStruct` 创建 input struct pin 与输出 field pins；`GetMenuActions()` 通过 `CanBeBroken` 注册。证据：`K2Node_BreakStruct.cpp:160-230`、`:350-375`。 | 部分覆盖。 | 记录 output field pin visibility、native break helper、字段名与 FProperty 对齐。 | GraphWrite。 |
| Set fields in struct | `UK2Node_SetFieldsInStruct` 创建 `StructRef` 输入与 `StructOut` 输出；字段 pin 由 optional pin manager 控制，未启用字段应拒绝连接。证据：`K2Node_SetFieldsInStruct.cpp:145-190`、`:400-415`。 | 未证明完整。 | TaskSpec 需要表达 selected fields；readback 需要确认只显示/连接启用字段。 | GraphWrite。 |
| Make array | `UK2Node_MakeArray` 继承 `UK2Node_MakeContainer`，默认 wildcard/container type，可 add pin 并在连接时收敛。 | 部分覆盖。 | add-pin count、element type inference、wildcard reset/readback。 | GraphWrite。 |
| Make map | `UK2Node_MakeMap` 继承 `UK2Node_MakeContainer`，需要 key/value 双 pin 类型收敛。 | 部分覆盖。 | key/value type evidence、pair pin count、link 后类型一致性。 | GraphWrite。 |
| Make set | `UK2Node_MakeSet` 继承 `UK2Node_MakeContainer`，元素类型推断与 array 类似。 | 部分覆盖。 | element type evidence、duplicate semantics 不由 GraphWrite 处理。 | GraphWrite。 |
| Split struct pin | `UEdGraphSchema_K2::CreateSplitPinNode()` 可根据 pin 方向创建 MakeStruct/BreakStruct 或 native make/break call。证据：`EdGraphSchema_K2.cpp:7200-7360`。 | 未明确归属。 | 不建议 GraphWrite 直接硬建 split 节点；应由 pin 操作/Schema split cluster 负责，GraphWrite 只消费 readback。 | Pin 操作工具；GraphWrite 只读回。 |

### Select

| Select kind | Editor behavior | GraphWrite can create | Link and infer | Readback requirement | Failure condition |
| --- | --- | --- | --- | --- | --- |
| Boolean select | 默认 index 兼容 bool，两个 option pins。 | 是。 | 让 UE 根据 index/option connections 推断。 | node class、`Index` pin bool、2 options、return type。 | option/return wildcard 未收敛。 |
| Integer select | index wildcard 可在默认值或连接下变为 int；option count 可增减。 | 是。 | 连接 int index 或设置 index type 后重建。 | index int、option count、return type。 | index 非 int/enum/bool 且无法转换。 |
| Enum select | enum index 触发 option pins 按 enum entries 展开。 | 是。 | 通过 enum pin type 或 spawner/source 指定。 | enum path、option pins 对应 enum entries。 | enum pin 与 option count 不一致。 |
| Wildcard select | option/return 初始 wildcard；连接非 wildcard 后复制类型。证据：`K2Node_Select.cpp:473-568`。 | 是。 | 必须先连接至少一个具体 option 或 return consumer。 | final result pin 非 wildcard，所有 option pins 同 type。 | 最终仍 wildcard。 |
| Object select | option/return object class 类型收敛。 | 是。 | 依赖 UE pin compatible logic。 | return object class、option class compatibility。 | common type 不成立或隐式转换失败。 |
| Class select | option/return class pin 类型收敛。 | 是。 | 依赖 pin type copy。 | class pin subtype、return class pin。 | class/object 混接无合法 conversion。 |
| Soft object select | soft object pin 类型可由连接指定。 | 是。 | 需要允许 soft object pin type。 | soft object asset class、return type。 | option/return 类型被重建为 wildcard。 |
| Interface select | interface object pin 类型可由连接指定。 | 是。 | 依赖 interface pin compatible logic。 | interface class path、option pins。 | object/interface cast 需求未显式处理。 |

UE evidence：`UK2Node_Select` 默认 `NumOptionPins=2` 且 index/option/return 起始为 wildcard，`AllocateDefaultPins()`、`ReallocatePinsDuringReconstruction()`、`NotifyPinConnectionListChanged()`、`ChangePinType()` 负责类型传播与重建。证据：`K2Node_Select.cpp:193-262`、`:335-568`、`:700-955`。

### Broad Flow Control

| Flow action | Node class or macro source | Required pins / dynamic behavior | Graph type legality | Readback signature | Covered by current baseline |
| --- | --- | --- | --- | --- | --- |
| branch | `UK2Node_IfThenElse` | exec in, bool Condition, Then/Else outputs。证据：`K2Node_IfThenElse.cpp:106-120`。 | EventGraph/FunctionGraph exec graph。 | node class + `execute/then/else/Condition` pins。 | 是，基础。 |
| sequence | `UK2Node_ExecutionSequence` | exec in, dynamic Then pins。 | EventGraph/FunctionGraph exec graph。 | node class + then pin count。 | 是，基础。 |
| return | function graph return node / schema action | function outputs。 | FunctionGraph only。 | return node + output pins。 | 是，基础。 |
| switch enum | `UK2Node_SwitchEnum` | selection enum pin，case exec pins from enum。 | exec graph。 | enum path + case pins。 | 未完整证明。 |
| switch int | `UK2Node_SwitchInteger` | selection int，case pins dynamic。 | exec graph。 | int cases + default pin。 | 未完整证明。 |
| switch string | `UK2Node_SwitchString` | selection string，case pins dynamic。 | exec graph。 | string cases + default pin。 | 未完整证明。 |
| switch name | `UK2Node_SwitchName` | selection name，case pins dynamic。 | exec graph。 | name cases + default pin。 | 未完整证明。 |
| switch object | function/macro or specialized switch availability depends on action menu | object selection with equality/cases where available。 | context-filtered。 | source spawner + object pin/case pins。 | 未覆盖。 |
| do once | `UK2Node_MacroInstance` from `StandardMacros.DoOnce` | exec in, completed, reset。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| do n | `UK2Node_MacroInstance` from `StandardMacros.Do N` | exec in, n, reset, counter/output。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| gate | `UK2Node_MacroInstance` from `StandardMacros.Gate` | enter/open/close/toggle/start closed/exit。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| multi-gate | `UK2Node_MultiGate` direct node | reset/random/loop/start index + dynamic out pins。证据：`K2Node_MultiGate.cpp:100-180`、`:690-715`。 | exec graph。 | node class + output pin count。 | 未完整证明。 |
| flip flop | `UK2Node_MacroInstance` from `StandardMacros.FlipFlop` | exec in, A/B, bool output。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| for loop | `UK2Node_MacroInstance` from `StandardMacros.ForLoop` | first/last index, loop body, completed。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| for loop with break | `UK2Node_MacroInstance` from `StandardMacros.ForLoopWithBreak` | first/last index, break, body, completed。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |
| foreach loop | `UK2Node_MacroInstance` from `StandardMacros.ForEachLoop` | array input, array element, index/body/completed。 | exec graph。 | macro graph reference + array element type。 | 计划中但未证明。 |
| foreach loop with break | `UK2Node_MacroInstance` from `StandardMacros.ForEachLoopWithBreak` | array input, break, body/completed。 | exec graph。 | macro graph reference + array element type。 | 计划中但未证明。 |
| while loop | `UK2Node_MacroInstance` from `StandardMacros.WhileLoop` | condition delegate/exec loop pins from macro。 | exec graph。 | macro graph reference + pins。 | 计划中但未证明。 |

Macro evidence：`UK2Node_MacroInstance` explicitly recognizes StandardMacros names `ForLoop`、`ForLoopWithBreak`、`WhileLoop`、`Gate`、`Do N`、`DoOnce`、`FlipFlop`、`ForEachLoop`、`ForEachLoopWithBreak` for flow-control macro identity. 证据：`K2Node_MacroInstance.cpp:457-489`。

### Create / Asset Action

| Create row | Editor behavior | GraphWrite ownership | Required spawner evidence | TaskSpec shape | Readback proof |
| --- | --- | --- | --- | --- | --- |
| make array | `UK2Node_MakeArray` via node spawner; wildcard element type + dynamic pins。 | GraphWrite。 | node class + container kind。 | `construct/create` with container kind, element type, item count。 | output array type, input item pins, compile result。 |
| make map | `UK2Node_MakeMap` via node spawner; key/value pins。 | GraphWrite。 | node class + key/value type。 | container kind map, key/value type, entries。 | output map type, key/value pins。 |
| make set | `UK2Node_MakeSet` via node spawner。 | GraphWrite。 | node class + element type。 | container kind set, element type, items。 | output set type, element pins。 |
| spawn actor from class | `UK2Node_SpawnActorFromClass` exposes Class/Transform/CollisionHandling/Owner and spawn vars; expands to deferred spawn/finish. 证据：`K2Node_SpawnActorFromClass.cpp:71-96`、`:439-620`。 | GraphWrite for graph statement; class asset creation is Asset tooling。 | node class + actor class path + expose-on-spawn props。 | `create.spawn_actor` with class, transform, spawn var assignments。 | return actor type, spawn pins, exec success path。 |
| construct object from class | `UK2Node_ConstructObjectFromClass` creates Class, ReturnValue, Outer/WorldContext and exposes spawn vars. 证据：`K2Node_ConstructObjectFromClass.cpp:60-160`。 | GraphWrite。 | node class + base class + factory UFunction。 | `create.construct_object` with class, outer, expose-on-spawn values。 | return object type, class pin, outer pin。 |
| create widget | `UK2Node_CreateWidget` extends construct object, base class `UUserWidget`, owns player pin and expansion to `UWidgetBlueprintLibrary::Create`。证据：`K2Node_CreateWidget.cpp:1-160`、`:320-420`。 | GraphWrite for node creation; UMG Designer tree excluded。 | node class + widget class + owning player pin。 | `create.create_widget` with widget class, owning player, expose-on-spawn values。 | return widget type, owning player pin。 |
| async action create | `UK2Node_AsyncAction` registers factory actions for `UBlueprintAsyncActionBase`; stores proxy factory function/class。证据：`K2Node_AsyncAction.cpp:31-79`。 | GraphWrite only when action creates graph node; handler/delegate binding belongs EventDelegate/schedule。 | proxy factory function path + proxy class + node class。 | generic projected spawner action。 | async node factory data, delegate output pins, activation path。 |
| asset action node | `UBlueprintAssetNodeSpawner` creates asset-backed node from `FAssetData`。 | GraphWrite only for graph node insertion; asset lifecycle belongs Asset tooling。 | asset data + spawner class + node class。 | `asset_action` with graph-block evidence, not public atomic target。 | created node class, asset reference, stable pins。 |
| function-backed create node | Any callable UFunction named create/spawn/construct surfaced by function spawner。 | FunctionAction by default; GraphWrite can wrap only when semantic create shape is needed。 | function path + metadata (`DeterminesOutputType`, `WorldContext`, latent if any)。 | function action or projected generic create。 | call function node + function ref + return type。 |

### Convert / Transform Operation

| Transform row | Editor action source | GraphWrite TaskSpec shape | Required evidence | Link behavior | Readback check |
| --- | --- | --- | --- | --- | --- |
| object dynamic cast | `UK2Node_DynamicCast` spawner from `AddClassCastActions()`。 | `transform_operation.dynamic_cast` or projected cast spawner。 | target class path + node class。 | input object pin; impure cast success/fail exec pins or pure cast success bool。 | target output object/interface type, cast failed/succeeded pins。 |
| class dynamic cast | `UK2Node_ClassDynamicCast` spawner。 | `transform_operation.class_dynamic_cast`。 | target class path + class cast node class。 | input class pin to output class pin。 | class output type and success/fail pins。 |
| interface cast | `UK2Node_DynamicCast` with interface target. | dynamic_cast with target interface path。 | interface class path。 | object to interface cast。 | output interface pin type。 |
| enum to string/name/text | `BlueprintAutocast` or function-backed conversion in Kismet libraries。 | projected conversion function or automatic link conversion。 | UFunction path + `BlueprintAutocast` metadata。 | direct call or inserted conversion during link。 | call function node/source-target pin types。 |
| numeric conversion | type promotion / conversion functions. | `transform_operation.type_promotion` or projected function。 | function path or type promotion spawner evidence。 | may insert conversion node during link。 | source numeric type, output numeric type。 |
| string/name/text conversion | Kismet text/string libraries with `BlueprintAutocast`。 | function-backed conversion。 | UFunction path, compact title optional。 | call function or autoconv node。 | output pin category string/name/text。 |
| object to soft object | `UKismetSystemLibrary::Conv_ObjectToSoftObjectReference` / asset conversion node。 | `transform_operation.convert`。 | UFunction path or `UK2Node_ConvertAsset`。 | direct conversion node. | soft object pin type and asset class。 |
| class to soft class | `UKismetSystemLibrary::Conv_ClassToSoftClassReference`。 | `transform_operation.convert`。 | UFunction path or `UK2Node_ConvertAsset`。 | direct conversion node。 | soft class pin type。 |
| automatic conversion inserted during link | `UEdGraphSchema_K2::TryCreateConnection()` / `CreateAutomaticConversionNodeAndConnections()`。证据：`EdGraphSchema_K2.cpp:2160-2320`、`:2690-2970`。 | link-level option, not standalone transform unless requested。 | source pin type, target pin type, inserted node/function。 | GraphWrite should attempt schema link, then read back direct link or conversion node。 | conversion node exists or pins directly linked；compile passes。 |
| function-backed conversion | `UBlueprintFunctionNodeSpawner` for conversion UFunctions. | projected function action or `transform_operation.function_conversion`。 | function path + metadata。 | call node inserted between pins。 | call function ref + input/output pin types。 |

Dynamic cast evidence：`UK2Node_DynamicCast` creates exec, `CastSucceeded`/`CastFailed` outputs for impure cast, input object pin, typed cast result and success bool. 证据：`K2Node_DynamicCast.cpp:83-143`。

### Schedule / Schedule Operation

| Schedule row | Editor source | GraphWrite should create only call node | Handler/signature owner | Required evidence | Readback proof |
| --- | --- | --- | --- | --- | --- |
| timer by function name | `UKismetSystemLibrary::K2_SetTimer` UFunction, DisplayName `Set Timer by Function Name`。证据：`KismetSystemLibrary.h:859-871`。 | 是。 | Function/event existence validation outside GraphWrite; function name string is user-specified。 | UFunction path + object/function name pins。 | call function node, return `FTimerHandle`。 |
| timer by event/delegate | `K2_SetTimerDelegate` with `FTimerDynamicDelegate`。证据：`KismetSystemLibrary.h:687-708`。 | 仅 call node。 | EventDelegate owns custom event/function signature and delegate binding。 | UFunction path + delegate pin metadata。 | delegate pin connected or explicit failure；return timer handle。 |
| clear timer | by event/handle/function UFunctions, several event variants deprecated。 | 是。 | None, except delegate target if by event。 | UFunction path + deprecation awareness。 | call function node + handle/delegate/function pins。 |
| pause timer | by event/handle/function UFunctions。 | 是。 | None, except delegate target if by event。 | UFunction path。 | call function node。 |
| unpause timer | by event/handle/function UFunctions。 | 是。 | None, except delegate target if by event。 | UFunction path。 | call function node。 |
| delay | `UKismetSystemLibrary::Delay`, latent metadata。证据：`KismetSystemLibrary.h:641-651`。 | 是。 | K2 compiler/latent manager owns continuation; GraphWrite links exec continuation。 | UFunction path + `Latent` + `LatentInfo` + `WorldContext`。 | call function node, latent pins hidden/managed, exec continuation。 |
| delay until next tick | `UKismetSystemLibrary::DelayUntilNextTick`。证据：`KismetSystemLibrary.h:653-660`。 | 是。 | K2 latent system。 | UFunction path + latent metadata。 | continuation exec pin。 |
| retriggerable delay | `UKismetSystemLibrary::RetriggerableDelay`。证据：`KismetSystemLibrary.h:662-670`。 | 是。 | K2 latent system。 | UFunction path + latent metadata。 | duration pin + continuation。 |
| latent function call | any callable UFunction with `Latent`/`LatentInfo` metadata, e.g. `MoveComponentTo`。证据：`KismetSystemLibrary.h:672-685`。 | 是。 | Callee/latent system; handler not GraphWrite unless delegate pin exists。 | UFunction path + latent metadata。 | latent call node, continuation pins, compile result。 |
| async action node | `UK2Node_AsyncAction` with proxy factory function/class。 | Create node; delegate handlers outside GraphWrite。 | EventDelegate owns event handler/signature wiring。 | proxy factory function + proxy class + output delegate pins。 | async node readback + output delegate pins。 |
| timeline-like scheduler | Timeline nodes are editor graph specialized and often asset/member-backed. | 不纳入 generic schedule first phase。 | Future specialized Blueprint graph/timeline tool。 | node/spawner identity if later added。 | timeline component/member evidence。 |
| tick-like scheduler | Tick events / binding are event entry concerns。 | 不作为 schedule call node。 | EventEntry/EventDelegate。 | event source function or override evidence。 | event node + owning graph。 |

### Asset Action

| Asset action row | Editor behavior | Current support baseline | Missing work | Proposed owner |
| --- | --- | --- | --- | --- |
| asset-backed node from `FAssetData` | `UBlueprintAssetNodeSpawner` creates node from asset source. | 部分覆盖，已有 projected spawner evidence 方向。 | 需要 runtime revalidation，确保 asset reference filter、graph schema、node class 与 pins 匹配。 | GraphWrite for graph insertion；Asset tooling for asset lifecycle。 |
| graph-block review | 任务政策要求 asset_action 不需要 public asset-action atomic target，本阶段用 `graph_block` Review surface。 | 已在任务文档中定调。 | DebugBundle 可保留 spawner/asset 低层诊断，但 public Review 不扩散 atomic target。 | GraphWrite Review。 |
| asset create/import/select | 非图节点写入。 | 不应扩入 GraphWrite。 | 用已有 asset tooling 处理。 | Asset tooling。 |

### Container Action

| collection_kind | operation | engine_function | callable_in_normal_graph | overlaps_function_action | needs_first_class_shape | readback_success |
| --- | --- | --- | --- | --- | --- | --- |
| array | add | UKismetArrayLibrary::Array_Add | 是 | 是 | 是：ArrayParm + ArrayTypeDependentParams 绑定 NewItem | UFunction 路径、TargetArray 元素类型、NewItem 类型、返回 int index、编译通过 |
| array | add_unique | UKismetArrayLibrary::Array_AddUnique | 是 | 是 | 是：同 add，且语义非重复添加 | 返回 int index 或 INDEX_NONE；数组元素类型不回退为 wildcard |
| array | shuffle | UKismetArrayLibrary::Array_Shuffle | 是 | 是 | 中：仅需集合 pin 绑定，first-class 可读性更好 | TargetArray 为 array，执行 pin 存在，编译通过 |
| array | shuffle_from_stream | UKismetArrayLibrary::Array_ShuffleFromStream | 是 | 是 | 中：额外 FRandomStream ref pin | TargetArray array + RandomStream ref pin，编译通过 |
| array | identical | UKismetArrayLibrary::Array_Identical | 是 | 是 | 是：两个 array 必须同元素类型 | ArrayA/ArrayB 元素类型一致，返回 bool |
| array | append | UKismetArrayLibrary::Array_Append | 是 | 是 | 是：TargetArray/SourceArray 同元素类型 | 两个 array pin 类型一致，编译通过 |
| array | insert | UKismetArrayLibrary::Array_Insert | 是 | 是 | 是：NewItem 跟随 TargetArray 元素类型 | TargetArray 元素类型、NewItem 类型、Index pin、编译通过 |
| array | remove_index | UKismetArrayLibrary::Array_Remove | 是 | 是 | 中：固定 int index + array pin | TargetArray array、Index int、执行 pin 存在 |
| array | remove_item | UKismetArrayLibrary::Array_RemoveItem | 是 | 是 | 是：Item 跟随元素类型 | Item 类型与元素类型一致，返回 bool |
| array | clear | UKismetArrayLibrary::Array_Clear | 是 | 是 | 中：简单 callable，可由 FunctionAction 调用；first-class 有助于语义一致 | TargetArray array、执行 pin 存在 |
| array | resize | UKismetArrayLibrary::Array_Resize | 是 | 是 | 中：array + size | TargetArray array、Size int |
| array | reverse | UKismetArrayLibrary::Array_Reverse | 是 | 是 | 中：array only | TargetArray array、执行 pin 存在 |
| array | length | UKismetArrayLibrary::Array_Length | 是 | 是 | 中：pure function，first-class 可提供稳定 operation 名 | TargetArray array、返回 int |
| array | is_empty | UKismetArrayLibrary::Array_IsEmpty | 是 | 是 | 中：pure bool | TargetArray array、返回 bool |
| array | is_not_empty | UKismetArrayLibrary::Array_IsNotEmpty | 是 | 是 | 中：pure bool | TargetArray array、返回 bool |
| array | last_index | UKismetArrayLibrary::Array_LastIndex | 是 | 是 | 中：pure int | TargetArray array、返回 int |
| array | get | UKismetArrayLibrary::Array_Get | 菜单内部/上下文可见；函数标记 BlueprintInternalUseOnly | 是，但不宜裸调用 | 是：Get 节点是数组语义核心，需按 schema/menu spawner 使用 | TargetArray array、Index int、Item 输出为元素类型 |
| array | set | UKismetArrayLibrary::Array_Set | 是 | 是 | 是：Item 跟随元素类型，bSizeToFit 行为需保留 | TargetArray array、Index int、Item 类型、bSizeToFit bool |
| array | swap | UKismetArrayLibrary::Array_Swap | 是 | 是 | 中：两个 index | TargetArray array、First/Second int |
| array | find | UKismetArrayLibrary::Array_Find | 是 | 是 | 是：ItemToFind 跟随元素类型 | TargetArray array、Item 类型、返回 int |
| array | contains | UKismetArrayLibrary::Array_Contains | 是 | 是 | 是：ItemToFind 跟随元素类型 | TargetArray array、Item 类型、返回 bool |
| array | filter_array | UKismetArrayLibrary::FilterArray | 是 | 是 | 是：DeterminesOutputType + DynamicOutputParam，不是泛型数组模板 | FilterClass 决定 FilteredArray 元素类，返回 array 类型正确 |
| array | is_valid_index | UKismetArrayLibrary::Array_IsValidIndex | 是 | 是 | 中：array + int | TargetArray array、Index int、返回 bool |
| array | random | UKismetArrayLibrary::Array_Random | 是 | 是 | 是：OutItem 跟随元素类型 | OutItem 类型与元素类型一致，OutIndex int |
| array | random_from_stream | UKismetArrayLibrary::Array_RandomFromStream | 是 | 是 | 是：OutItem + RandomStream ref | OutItem 类型、OutIndex int、RandomStream ref |
| array | sort_string | UKismetArrayLibrary::SortStringArray | 是 | 是 | 否/低：固定 FString array，不依赖 wildcard | TargetArray string array ref，SortOrder enum |
| array | sort_name | UKismetArrayLibrary::SortNameArray | 是 | 是 | 否/低：固定 FName array | TargetArray name array ref，SortOrder enum |
| array | sort_byte | UKismetArrayLibrary::SortByteArray | 是 | 是 | 否/低：固定 byte array | TargetArray byte array ref，SortOrder enum |
| array | sort_int | UKismetArrayLibrary::SortIntArray | 是 | 是 | 否/低：固定 int array | TargetArray int array ref，SortOrder enum |
| array | sort_int64 | UKismetArrayLibrary::SortInt64Array | 是 | 是 | 否/低：固定 int64 array | TargetArray int64 array ref，SortOrder enum |
| array | sort_float | UKismetArrayLibrary::SortFloatArray | 是 | 是 | 否/低：固定 double array | TargetArray double array ref，SortOrder enum |
| map | add | UBlueprintMapLibrary::Map_Add | 是 | 是 | 是：MapParam + Key/Value 模板绑定 | TargetMap map、Key/Value 类型匹配、执行 pin |
| map | remove | UBlueprintMapLibrary::Map_Remove | 是 | 是 | 是：Key 跟随 map key 类型 | Key 类型匹配、返回 bool |
| map | find | UBlueprintMapLibrary::Map_Find | 是 | 是 | 是：Key/Value 模板绑定 | Key 类型、Value 输出类型、返回 bool |
| map | contains | UBlueprintMapLibrary::Map_Contains | 是 | 是 | 是：Key 模板绑定 | Key 类型、返回 bool |
| map | keys | UBlueprintMapLibrary::Map_Keys | 是 | 是 | 是：Keys 输出 array key 类型 | Keys array 元素类型等于 map key |
| map | values | UBlueprintMapLibrary::Map_Values | 是 | 是 | 是：Values 输出 array value 类型 | Values array 元素类型等于 map value |
| map | length | UBlueprintMapLibrary::Map_Length | 是 | 是 | 中：pure int | TargetMap map、返回 int |
| map | is_empty | UBlueprintMapLibrary::Map_IsEmpty | 是 | 是 | 中：pure bool | TargetMap map、返回 bool |
| map | is_not_empty | UBlueprintMapLibrary::Map_IsNotEmpty | 是 | 是 | 中：pure bool | TargetMap map、返回 bool |
| map | clear | UBlueprintMapLibrary::Map_Clear | 是 | 是 | 中：callable clear | TargetMap map、执行 pin |
| map | get_key_value_by_index | UBlueprintMapLibrary::Map_GetKeyValueByIndex | 是 | 是 | 是：Key/Value 输出模板绑定 | Index int、Key/Value 输出类型匹配 map |
| map | get_last_index | UBlueprintMapLibrary::Map_GetLastIndex | 是 | 是 | 中：pure int | TargetMap map、返回 int |
| set | add | UBlueprintSetLibrary::Set_Add | 是 | 是 | 是：SetParam 将 TargetSet/NewItem 绑定为同元素类型 | TargetSet set、NewItem 类型、执行 pin |
| set | add_items | UBlueprintSetLibrary::Set_AddItems | 是 | 是 | 是：set 元素类型与 NewItems array 元素类型一致 | TargetSet set、NewItems array 元素类型 |
| set | remove | UBlueprintSetLibrary::Set_Remove | 是 | 是 | 是：Item 跟随 set 元素类型 | Item 类型、返回 bool |
| set | is_empty | UBlueprintSetLibrary::Set_IsEmpty | 是 | 是 | 中：pure bool | TargetSet set、返回 bool |
| set | is_not_empty | UBlueprintSetLibrary::Set_IsNotEmpty | 是 | 是 | 中：pure bool | TargetSet set、返回 bool |
| set | remove_items | UBlueprintSetLibrary::Set_RemoveItems | 是 | 是 | 是：Items array 元素类型与 set 一致 | TargetSet set、Items array 类型 |
| set | to_array | UBlueprintSetLibrary::Set_ToArray | 是 | 是 | 是：Result array 元素类型等于 set 元素类型 | Result array 类型正确 |
| set | clear | UBlueprintSetLibrary::Set_Clear | 是 | 是 | 中：callable clear | TargetSet set、执行 pin |
| set | length | UBlueprintSetLibrary::Set_Length | 是 | 是 | 中：pure int | TargetSet set、返回 int |
| set | contains | UBlueprintSetLibrary::Set_Contains | 是 | 是 | 是：ItemToFind 跟随元素类型 | ItemToFind 类型、返回 bool |
| set | intersection | UBlueprintSetLibrary::Set_Intersection | 是 | 是 | 是：A/B/Result 三个 set 元素类型一致 | A/B/Result set 类型一致 |
| set | union | UBlueprintSetLibrary::Set_Union | 是 | 是 | 是：A/B/Result 三个 set 元素类型一致 | A/B/Result set 类型一致 |
| set | difference | UBlueprintSetLibrary::Set_Difference | 是 | 是 | 是：A/B/Result 三个 set 元素类型一致 | A/B/Result set 类型一致 |
| set | get_item_by_index | UBlueprintSetLibrary::Set_GetItemByIndex | 是 | 是 | 是：Item 输出跟随元素类型 | Index int、Item 类型 |
| set | get_last_index | UBlueprintSetLibrary::Set_GetLastIndex | 是 | 是 | 中：pure int | TargetSet set、返回 int |

Container evidence：`UKismetArrayLibrary`、`UBlueprintMapLibrary`、`UBlueprintSetLibrary` 均是 `UBlueprintFunctionLibrary`，多数操作使用 `CustomThunk` 与 `ArrayParm` / `MapParam` / `SetParam` metadata 实现 wildcard/template pin 绑定。证据：`KismetArrayLibrary.h:36-335`、`BlueprintMapLibrary.h:27-140`、`BlueprintSetLibrary.h:25-173`。

## Implementation Boundary

| Boundary | 属于该边界的工作 | 不属于该边界的工作 |
| --- | --- | --- |
| GraphWrite | 普通 Blueprint graph statement 的节点创建、连接、K2 schema link、node spawner 投影、pin type readback、compile proof、graph_block Review。 | 创建资产、导入资产、修改非 K2 图域、隐式创建 event handler/signature、复杂跨 transaction 编排。 |
| FunctionAction | 任意 `UBlueprintFunctionNodeSpawner` 可调用函数，包括 function-backed create、conversion、schedule、container fallback。 | 需要 first-class 结构语义的 Make/Break/Select/Container op 不应只靠裸 function action。 |
| EventDelegate | custom event、delegate handler、function signature、event binding、timer by event 的事件端、async output delegate 的 handler。 | 单纯插入 timer/latent/async call node。 |
| Asset tooling | 资产创建、查找、导入、保存、asset metadata、asset picker 候选。 | 将 asset-backed spawner 创建为 graph node 之后的 pin/link/readback。 |
| Pin operation / schema split | split struct pin、recombine pin、schema-level pin transform。 | 高层 GraphWrite statement 编排。 |
| Future non-GraphWrite tools | Timeline editor、UMG Designer tree、Animation Blueprint nodes、Material、Niagara、Control Rig。 | 普通 EventGraph / FunctionGraph K2 action parity。 |

## Evidence And Readback Matrix

| Capability | TaskSpec evidence | Resolver evidence | Fragment builder evidence | Graph readback | Compile/output proof |
| --- | --- | --- | --- | --- | --- |
| `construct` | operation kind, struct/container path, requested fields/items, optional pins。 | selected spawner/node class, struct/container type, menu signature。 | created node class, requested pins linked。 | node class, struct/container type, visible pins, linked pins。 | compiled graph without struct/container reconstruction errors。 |
| `deconstruct` | source struct type, requested fields/consumers。 | BreakStruct/split action, field policy。 | break node or schema split operation。 | source type, output field pins, consumers。 | compiled graph without break/split type errors。 |
| `select` | index kind/type, option count, optional enum/source type, option values/links。 | `UK2Node_Select` spawner and optional index pin type。 | node creation, option pin add/remove, links。 | node class, option count, final index type, final result type, option pin types。 | wildcard fully settled and compile passes。 |
| `control` | control kind, cases/outputs, macro source when applicable。 | direct node or macro spawner identity。 | exec pins, dynamic pin allocation, links。 | node or macro source, exec pin names, dynamic pin count, graph legality。 | graph compiles and exec flow reachable。 |
| `create` | create kind, class/widget/actor/object/asset source, spawn vars。 | node/spawner/function path, class constraints。 | create node and expose-on-spawn pins。 | node class, class/object/widget/asset source, spawn/create pins, return pin type, linked success path。 | compile passes and return pin type matches class。 |
| `convert` | source type, target type, conversion mode。 | cast node/function/autocast/schema conversion evidence。 | explicit transform node or schema link operation。 | node class or function source, input type, output type, cast success pins where applicable。 | direct link or inserted conversion node compiles。 |
| `schedule` | schedule kind, timer/latent/async source, handler reference if any。 | UFunction/proxy factory function, latent/timer metadata。 | call/async node and continuation links。 | node class or function source, latent/timer/async metadata, handler/signature ownership result, continuation pins。 | compile passes; handler missing is explicit blocked result。 |
| `asset_action` | projected spawner evidence, asset source, graph target。 | asset node spawner + asset filter revalidation。 | created graph node through the asset_action evidence boundary。 | selected spawner identity, graph-block Review target, created node class and stable pins。 | graph mutation reviewed at graph_block level。 |
| `container_action` | collection kind, operation, collection type, key/value/element type。 | engine UFunction path + custom thunk metadata。 | call node and typed links。 | engine function path, collection type, wildcard replacement, linked pins。 | compile result and no wildcard residual pins。 |

## Detailed findings by task

### Task 1: Action Menu And Spawner Ground Truth

ActionDatabase 是普通蓝图右键菜单的核心事实源。GraphWrite 不应维护 display-name allowlist，而应记录 ActionDatabase registry 中可被当前 graph/pin context 过滤通过的 spawner identity。最低 evidence 组合为：`node_class`、`spawner_class`、`action_source`、`filter_reason`、`menu_signature`、`readback_signature`。这正好覆盖任务要求的六个字段。GraphWrite 的 resolver 应把 ActionDatabase projection 作为执行前 revalidation 的输入，而不是把 TaskSpec 中的 operation 名直接映射为硬编码 node class。

### Task 2: Construct And Deconstruct

Make/Break/SetFieldsInStruct 的共性是 field pin 由 struct metadata、property flags 和 optional pin manager 决定；MakeContainer 的共性是 container pin 起始可为 wildcard，并在链接/重建时收敛为具体 element/key/value 类型。GraphWrite 可把 `construct` 和 `deconstruct` 提升到接近 80% 覆盖，但前提是：创建前用 spawner/struct source revalidate，创建后读取 pins 而不是假设 pins，split struct pin 交给 schema pin operation 并读回结果。

### Task 3: Select

`UK2Node_Select` 的 UE 实现已经承担大部分复杂性：默认两个 option pins，index/option/return 以 wildcard 起步；重建时从旧 pin、index default、连接 pin 复制类型；连接变化时传播 non-wildcard 类型到 option 与 return pins。GraphWrite 的 select builder 应减少自定义 pin type 列表，改为：创建节点、连接 index/options/return consumer、触发重建、readback final pin types、在 wildcard 未收敛时失败。

### Task 4: Broad Flow Control

Branch、Sequence、Switch、MultiGate 是 direct K2Node 路径；DoOnce、Do N、Gate、FlipFlop、ForLoop、ForLoopWithBreak、ForEachLoop、ForEachLoopWithBreak、WhileLoop 通过 StandardMacros macro instance 路径暴露。Broad control 的 80% parity 不能靠固定创建 `UK2Node_IfThenElse` 和 `UK2Node_ExecutionSequence`；必须支持 macro spawner identity。StandardMacros 的精确 pins 需要在编辑器运行时从 macro graph 或 template node 读取。

### Task 5: Create And Asset Action

Create family 至少包含 MakeContainer、SpawnActorFromClass、ConstructObjectFromClass、CreateWidget、AsyncAction、asset-backed node 和 function-backed create。GraphWrite 应拥有普通 graph statement 中的 create node 插入和连接，但不拥有资产创建、UMG Designer tree 操作或 async delegate handler 生成。Asset action 的 public Review surface 应保持 `graph_block`，DebugBundle 可保留 asset/spawner 诊断细节。

### Task 6: Convert And Transform Operation

转换不是单一路径：dynamic cast/class cast 是 action database cast spawner；numeric/string/text/name/enum 可能是 function-backed conversion 或 `BlueprintAutocast`；object/class 到 soft references 可能走 specialized conversion / ConvertAsset；pin link 时 schema 还可自动插入 conversion node。GraphWrite 的 transform_operation 应支持两种模式：显式 transform node 与 link-time conversion readback。缺一都会低于 80%。

### Task 7: Schedule And Schedule Operation

Schedule family 横跨 timer UFunctions、latent UFunctions、async action nodes。GraphWrite 可以创建 call node / async node / continuation links，但不应隐式创建 timer delegate handler 或 async output delegate handler；这些属于 EventDelegate。对 `timer_delegate_node`，GraphWrite 应接受 handler reference 或返回 blocked，不能静默制造签名不匹配的 CustomEvent。

### Task 8: Container Action

UE 5.6 的 container callable surface 比常见 first-slice vocabulary 更宽：array 至少包含 add/add_unique/shuffle/shuffle_from_stream/identical/append/insert/remove_index/remove_item/clear/resize/reverse/length/is_empty/is_not_empty/last_index/get/set/swap/find/contains/filter/is_valid_index/random/random_from_stream/sort variants；map 包含 add/remove/find/contains/keys/values/length/is_empty/is_not_empty/clear/get_key_value_by_index/get_last_index；set 包含 add/add_items/remove/remove_items/to_array/clear/length/contains/intersection/union/difference/get_item_by_index/get_last_index/is_empty/is_not_empty。GraphWrite 的 `container_action` 应覆盖这些 UFunction path，至少对 wildcard/custom thunk 操作提供 first-class shape 与 readback。

### Task 9: Review Evidence And Readback Rules

Review 层应按 graph-block surface 收敛：`asset_action` 不需要新增 public asset-action atomic target。所有 generic ops 的成功条件不是“节点已创建”，而是“节点已创建、关键 pins 已重建并读取、链接结果稳定、compile/readback 无错误”。失败应保留 DebugBundle 证据，包括 spawner identity、schema rejection、pin mismatch、wildcard residual、compile diagnostics。

## Recommended Next Plans

| Plan name | Purpose | Scope |
| --- | --- | --- |
| `BlueprintHelper_GraphWrite_ConstructDeconstructSelect_GenericSpawnerPlan_20260525_CN.md` | 合并 Make/Break/SetFieldsInStruct/MakeContainer/Select 的 spawner、pin inference 与 readback 实现。 | `construct`、`deconstruct`、`select`。 |
| `BlueprintHelper_GraphWrite_BroadControlFlow_ActionMenuParityPlan_20260525_CN.md` | 统一 direct K2Node 与 StandardMacros macro spawner，实现 Flow Control menu parity。 | branch、sequence、return、switches、gate/do/loop macros、multi-gate。 |
| `BlueprintHelper_GraphWrite_CreateAssetAction_ProjectSpawnerPlan_20260525_CN.md` | 收敛 create/asset_action 的 projected spawner evidence、expose-on-spawn readback 与 graph_block Review。 | spawn actor、construct object、create widget、async create、asset_action。 |
| `BlueprintHelper_GraphWrite_TransformSchedule_AutocastLatentAsyncPlan_20260525_CN.md` | 统一 transform 与 schedule 的 function/cast/autocast/schema conversion、latent/timer/async ownership。 | dynamic cast、class cast、autoconv、timer、delay、latent、async。 |
| `BlueprintHelper_GraphWrite_ContainerAction_EngineLibraryParityPlan_20260525_CN.md` | 对齐 UE KismetArray/Map/Set library 的 callable surface 与 custom thunk wildcard readback。 | array/map/set 全 operation vocabulary。 |

## Verification status

| Check | Result |
| --- | --- |
| Engine source class baseline | 已抽取并检索 `Source/Editor/BlueprintGraph`、`Source/Editor/Kismet`、`Source/Editor/UMGEditor/Private/Nodes`、`Source/Runtime/Engine/Classes/Kismet`、`Source/Runtime/Engine/Public/TimerManager.h`、`Source/Runtime/Engine/Classes/Engine/LatentActionManager.h` 等路径。 |
| ActionDatabase / ActionFilter / NodeSpawner | 已定位并记录关键源码行。 |
| Construct / Deconstruct / Select | 已定位 MakeStruct、BreakStruct、SetFieldsInStruct、MakeContainer、SplitPin、Select 生命周期。 |
| Broad Flow Control | 已区分 direct K2Node 与 StandardMacros macro-backed entries；macro 精确 pin 需要运行时读取。 |
| Create / Asset / Async | 已定位 SpawnActor、ConstructObject、CreateWidget、AsyncAction、AssetNodeSpawner。 |
| Transform / Conversion | 已定位 DynamicCast、ClassDynamicCast、schema automatic conversion、BlueprintAutocast 相关路径。 |
| Schedule | 已定位 timer by event/handle/function、Delay、DelayUntilNextTick、RetriggerableDelay、MoveComponentTo、AsyncAction。 |
| Container functions | 已枚举 `UKismetArrayLibrary`、`UBlueprintMapLibrary`、`UBlueprintSetLibrary` callable functions。 |
| BlueprintHelper project source comparison | 未能在本容器执行；原因是项目源码目录未随输入提供。报告仅使用任务文档 baseline，并给出项目内应重跑的 comparison commands。 |

## Project-side commands to rerun when BlueprintHelper source is mounted

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "TryBuildSelectPinType|UK2Node_Select|select|AddOption|option_count|wildcard|enum" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
rg -n "make_array|make_map|make_set|spawn_actor|create_widget|construct_object|asset_action|IsSupportedCreateOperation|GenericCreate" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
rg -n "transform_operation|type_promotion|dynamic_cast|class_dynamic_cast|convert|GenericTransform|ProjectedSpawner|ActionDatabase" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
rg -n "schedule_operation|timer_delegate_node|latent_or_async_node|GenericSchedule|handler|signature|latent|async|ProjectedSpawner" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
rg -n "FBlueprintHelperContainerActionVocabulary|array|get|set|add_unique|append|insert|remove|contains|find|length|keys|values|to_array|KismetArrayLibrary|BlueprintMapLibrary|BlueprintSetLibrary" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
rg -n "graph_block|atomic target|asset_action|container_action|generic_schedule|generic_transform|Review evidence|BuildReviewEvidence|CapabilityContract|readback" AgentFaceService\task-core\src BlueprintHelper\Source\BlueprintHelper BlueprintHelper\Develop\Plan
```

## Final decision

`construct`、`deconstruct`、`select`、`container_action` 应进入“通用 spawner + UE pin inference + readback proof”实现计划，目标可设为接近普通 EventGraph / FunctionGraph menu 能力的 80%。`control` 必须先补 macro spawner parity；`create`/`asset_action` 必须先补 async/asset/function-backed create 证据边界；`convert`/`schedule` 必须先把 schema conversion、autocast、latent/async/timer ownership 分清。GraphWrite 的边界应保持为“普通 graph statement 创建与连接”，不要扩张为资产创建、事件 handler 签名生成或非 K2 图域专用编辑器。
