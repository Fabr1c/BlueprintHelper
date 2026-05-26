# GraphWrite Generic Ops UE Editor Capability Source Read Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development for implementation work. This document is a read-only engine-source exploration handoff; do not change BlueprintHelper code while executing this plan.

**Goal:** 判定 `construct`、`deconstruct`、`select`、`control`、`create`、`convert + transform_operation`、`schedule + schedule_operation`、`asset_action`、`container_action` 是否能达到普通 Blueprint EventGraph / FunctionGraph 右键菜单可创建节点的编辑器能力 80% 覆盖，并输出可执行的后续实现边界。

**Architecture:** GraphWrite 不应逐个硬编码节点名称，而应基于 UE ActionDatabase、K2 schema、node spawner、K2Node 生命周期、pin 类型推断和编译回读形成通用能力。现有实现已经有部分 TaskSpec、compiler、ActionResolution、FragmentBuilder 和 capability contract 基础，但这些簇尚未全部达到编辑器同等能力。

**Tech Stack:** UE 5.6 engine source rooted at `E:\UE_5.6\Engine`, BlueprintHelper GraphWrite C++ services, AgentFaceService TaskSpec TypeScript compiler and schema, Automation / CLI smoke tests.

---

## Current Status

> 2026-05-27 同步：本 source-read handoff 是历史探索计划，不再作为当前 unsupported 计数依据。`control`、`schedule.timer_delegate_node` / `schedule.latent_or_async_node`、typed `select` 的当前状态以实现代码和编辑器内 spawn 复验为准；旧 Evidence 结论不再可靠。

结论：这些能力当前没有完全通用化，也没有完全达到编辑器同等能力。部分能力已经有可用主线，但大多仍是 first-slice、固定 vocabulary、局部 spawner projection 或缺少回读证明。

| Capability | 当前覆盖判断 | 主要原因 |
| --- | --- | --- |
| `construct` | 部分覆盖 | Struct / TypeStructure 构造路径存在，但需要确认 MakeStruct、MakeArray、MakeMap、MakeSet、SetFieldsInStruct、split struct pin、native make helper 的编辑器菜单行为。 |
| `deconstruct` | 部分覆盖 | BreakStruct 路径存在，但需要确认 BreakStruct、split struct pin、native break helper、字段可见性和 pin 重建规则。 |
| `select` | 部分覆盖 | 现有 `UK2Node_Select` builder 和 spawner 选择路径存在，但 pin type 推断、wildcard 收敛、enum/object/class/interface/soft 类型和 option readback 还没有证明达到编辑器同等能力。 |
| `control` | 已接通当前 public control 语义，仍需泛化复验 | 当前 TS compiler 与 C++ builder 已覆盖 dedicated control 与 StandardMacros control 的 public shape；旧“只覆盖 `branch`、`sequence`、`return`”判断已过期。是否达到编辑器同等能力仍需按当前通用性 E2E/readback 复验。 |
| `create` | 部分覆盖 | 当前固定支持 `make_array`、`make_map`、`make_set`、`spawn_actor`、`create_widget`、`construct_object`、`asset_action`，但编辑器 action menu 的 create/spawn/construct/async 创建入口更广。 |
| `convert + transform_operation` | 部分覆盖 | `transform_operation` 已有 type promotion / cast / generic transform 基础，但编辑器转换包含 autoconv、dynamic cast、class cast、interface cast、object/class/soft conversions 和 function conversion action。 |
| `schedule + schedule_operation` | 部分覆盖 | 当前 generic schedule 聚焦 `timer_delegate_node`、`latent_or_async_node`；需要确认 timer、delay、timeline、latent call、async action 的菜单入口、handler/signature 边界和回读证明。 |
| `asset_action` | 部分覆盖 | 已收敛到 ActionDatabase projected spawner evidence 与执行期 revalidation，但 Review policy 已决定收窄到 `graph_block` evidence；仍需确认资产相关 action menu 覆盖范围和可调用节点类型。 |
| `container_action` | 部分覆盖 | 现有 first-class vocabulary 覆盖常见 array/map/set 操作，但仍需从 UE KismetArrayLibrary、BlueprintMapLibrary、BlueprintSetLibrary 和 editor menu 确认全部普通蓝图可 call 的容器操作。 |

## Read-Only Scope

- Engine root 固定为 `E:\UE_5.6\Engine`。
- 所有源码检索命令必须从 engine root 执行，或在命令中使用 `E:\UE_5.6\Engine` 绝对路径。
- 普通 Blueprint 范围限定为 EventGraph / FunctionGraph 右键菜单可创建节点。
- 不纳入 Animation Blueprint 专属节点、UMG Designer 专属控件树操作、Material graph、Niagara graph、Control Rig graph。
- UMG Blueprint 中普通 graph 可 call 的节点只有在其 action menu 行为与普通 Blueprint graph 共享 K2 action 机制时才纳入。
- 现有职责重叠原则：已有非 GraphWrite 工具负责的专用创建、绑定、导入、资产级操作不强行扩进 GraphWrite；GraphWrite 只保留通用 graph statement 创建和连接职责。

## Files To Inspect

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp`

## Engine Source Search Baseline

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "class UK2Node_MakeStruct|class UK2Node_BreakStruct|class UK2Node_SetFieldsInStruct|class UK2Node_Select|class UK2Node_IfThenElse|class UK2Node_ExecutionSequence|class UK2Node_Switch|class UK2Node_MultiGate|class UK2Node_DoOnce|class UK2Node_ForEachElementInEnum" Source\Editor\BlueprintGraph
rg -n "class UBlueprintActionDatabase|class UBlueprintNodeSpawner|class UBlueprintFunctionNodeSpawner|class UBlueprintAssetNodeSpawner|FBlueprintActionFilter|GetGraphContextActions|GetContextMenuActions" Source\Editor\BlueprintGraph
rg -n "KismetArrayLibrary|BlueprintMapLibrary|BlueprintSetLibrary|KismetSystemLibrary|TimerManager|LatentActionManager" Source\Runtime\Engine
rg -n "K2Node_CreateWidget|K2Node_SpawnActorFromClass|K2Node_GenericCreateObject|K2Node_AsyncAction|K2Node_DynamicCast|K2Node_ClassDynamicCast" Source
```

Expected:

- 输出覆盖 `Source\Editor\BlueprintGraph\Classes` 与 `Source\Editor\BlueprintGraph\Private` 内的 K2Node、schema、action database、action filter 文件。
- 输出覆盖 `Source\Runtime\Engine\Classes\Kismet` 和 `Source\Runtime\Engine\Private` 内 array/map/set/system/timer/latent 库文件。
- 输出覆盖 `Source\Editor\UMGEditor\Private\Nodes` 内 CreateWidget 节点文件。

## Task 1: Action Menu And Spawner Ground Truth

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes\BlueprintActionDatabase.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionDatabase.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes\BlueprintNodeSpawner.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintNodeSpawner.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionFilter.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/EdGraphSchema_K2.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/EdGraphSchema_K2.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Locate action database population paths**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "RefreshAll|RefreshAssetActions|AddBlueprintAction|CollectAllActions|GetAllActions|FBlueprintActionDatabaseRegistrar" Source\Editor\BlueprintGraph
```

Expected: commands identify the registrar flow that creates action entries for native functions, macros, casts, variables, events, assets and node spawners.

- [ ] **Step 2: Locate graph context action filtering**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "GetGraphContextActions|FBlueprintActionFilter|IsFiltered|Context.Targets|CanSpawnUnderSpecifiedSchema|ActionMenu" Source\Editor\BlueprintGraph Source\Editor\Kismet
```

Expected: commands identify which filters make a node appear in a normal Blueprint EventGraph / FunctionGraph right-click menu.

- [ ] **Step 3: Record required evidence fields**

Write a table in the result document with these columns:

| Field | Meaning |
| --- | --- |
| `node_class` | `UK2Node` class selected by the editor action menu. |
| `spawner_class` | `UBlueprintNodeSpawner` subclass or factory path used by the action. |
| `action_source` | Function, macro, asset, struct, enum, cast, async proxy or schema action source. |
| `filter_reason` | Why the action is legal in EventGraph / FunctionGraph. |
| `menu_signature` | Stable data that can be projected into TaskSpec evidence without hardcoding display text. |
| `readback_signature` | Stable data observable after node creation and pin reconstruction. |

## Task 2: Construct And Deconstruct

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MakeStruct.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MakeStruct.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_BreakStruct.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_BreakStruct.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_SetFieldsInStruct.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_SetFieldsInStruct.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MakeContainer.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MakeContainer.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MakeArray.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MakeArray.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MakeMap.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MakeMap.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MakeSet.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MakeSet.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Compare make and break node lifecycle**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "AllocateDefaultPins|ReallocatePinsDuringReconstruction|ExpandNode|CanBeMade|CanBeBroken|ShowPinForProperties|FOptionalPinManager|StructType|ContainerType" Source\Editor\BlueprintGraph\Classes Source\Editor\BlueprintGraph\Private
```

Expected: output identifies how UE decides visible fields, optional pins, default pin types, reconstruction, and compiler expansion for MakeStruct, BreakStruct, SetFieldsInStruct and MakeContainer variants.

- [ ] **Step 2: Verify normal graph menu availability**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "Make Struct|Break Struct|Set members in|Make Array|Make Map|Make Set|StructOperation|MakeStructureDefaultValue" Source\Editor\BlueprintGraph Source\Editor\Kismet Source\Runtime\Engine
```

Expected: output identifies user-facing action registration and filters for ordinary Blueprint graphs.

- [ ] **Step 3: Produce coverage matrix**

Write rows for:

| Node family | Must record |
| --- | --- |
| Make struct | Struct source, visible fields, hidden fields, default values, wildcard or concrete pin behavior. |
| Break struct | Struct source, output field pins, hidden field policy, split pin overlap. |
| Set fields in struct | Mutable struct target pin, selected fields, reconstruction behavior. |
| Make container | Array, map, set element/key/value type inference and add-pin behavior. |
| Split struct pin | Whether GraphWrite should create a K2Node or delegate to pin split operations outside this cluster. |

## Task 3: Select

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_Select.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_Select.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/EdGraphSchema_K2.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Read select pin inference**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "UK2Node_Select|IndexPinType|ReturnValuePin|OptionPin|AddOptionPin|RemoveOptionPin|NotifyPinConnectionListChanged|PostReconstructNode|Wildcard" Source\Editor\BlueprintGraph
```

Expected: output identifies how select chooses index type, option type, return type, wildcard promotion, enum expansion and reconstruction.

- [ ] **Step 2: Compare with BlueprintHelper select builder**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "TryBuildSelectPinType|UK2Node_Select|select|AddOption|option_count|wildcard|enum" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
```

Expected: output shows whether BlueprintHelper still uses a fixed type list or whether it delegates type behavior to UE pin inference.

- [ ] **Step 3: Produce select acceptance rules**

Write a table with rows for boolean select, integer select, enum select, wildcard select, object select, class select, soft object select and interface select. For each row, record whether GraphWrite can create the node, link inputs, let UE infer pin type, read back final type, and fail when wildcard does not settle.

## Task 4: Broad Flow Control

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_IfThenElse.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_IfThenElse.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_ExecutionSequence.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_ExecutionSequence.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_Switch.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_Switch.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_SwitchEnum.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_SwitchEnum.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_SwitchInteger.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_SwitchInteger.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_SwitchString.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_SwitchString.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MultiGate.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MultiGate.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_DoOnceMultiInput.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_DoOnceMultiInput.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_MacroInstance.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_MacroInstance.cpp`
- Read: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_BroadControlFlowPlan_20260525_CN.md`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Enumerate Flow Control menu actions**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "Branch|Sequence|Switch on|Do Once|Do N|Flip Flop|For Loop|For Loop With Break|For Each Loop|For Each Loop With Break|While Loop|Gate|MultiGate|Delay Until Next Tick" Source\Editor\BlueprintGraph Source\Editor\Kismet
```

Expected: output identifies K2Node classes and macro library entries used by ordinary Blueprint Flow Control actions.

- [ ] **Step 2: Separate node classes from macro instances**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "K2Node_MacroInstance|MacroGraphReference|StandardMacros|BlueprintMacroLibrary|ForEachLoop|ForLoop|WhileLoop|Gate|FlipFlop" Source\Editor\BlueprintGraph Source\Editor\Kismet Source\Runtime\Engine
```

Expected: output separates direct node classes from macro-backed Flow Control entries so GraphWrite can use spawner identity instead of hardcoded node construction.

- [ ] **Step 3: Produce implementation classification**

Write rows for branch, sequence, return, switch enum, switch int, switch string, switch name, switch object, do once, do n, gate, multi-gate, flip flop, for loop, for loop with break, foreach loop, foreach loop with break and while loop. For each row, record node class or macro source, required pins, add-pin behavior, graph type legality, readback signature and whether `BlueprintHelper_GraphWrite_BroadControlFlowPlan_20260525_CN.md` already covers it.

## Task 5: Create And Asset Action

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_SpawnActorFromClass.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_SpawnActorFromClass.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_GenericCreateObject.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_GenericCreateObject.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\UMGEditor\Private\Nodes/K2Node_CreateWidget.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\UMGEditor\Private\Nodes/K2Node_CreateWidget.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/BlueprintAssetNodeSpawner.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/BlueprintAssetNodeSpawner.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionDatabase.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Enumerate create nodes from editor source**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "SpawnActor|CreateWidget|ConstructObject|GenericCreateObject|AsyncAction|Create.*Object|Spawn.*Object|Factory|BlueprintAssetNodeSpawner" Source\Editor\BlueprintGraph Source\Editor\UMGEditor Source\Runtime\Engine Source\Runtime\UMG
```

Expected: output identifies create nodes, asset-backed nodes, async create actions and editor filters.

- [ ] **Step 2: Compare BlueprintHelper create operations**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "make_array|make_map|make_set|spawn_actor|create_widget|construct_object|asset_action|IsSupportedCreateOperation|GenericCreate" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
```

Expected: output shows the current fixed create operation list and any asset_action projection service dependency.

- [ ] **Step 3: Decide GraphWrite-owned create coverage**

Write rows for make array, make map, make set, spawn actor from class, construct object from class, create widget, async action create, asset action node and function-backed create node. For each row, record whether GraphWrite should own it, whether another tool already owns it, required spawner evidence, required TaskSpec shape and readback proof.

## Task 6: Convert And Transform Operation

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_DynamicCast.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_DynamicCast.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_ClassDynamicCast.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_ClassDynamicCast.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/BlueprintFunctionNodeSpawner.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/BlueprintFunctionNodeSpawner.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionDatabase.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/EdGraphSchema_K2.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Locate cast and conversion action creation**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "DynamicCast|ClassDynamicCast|Autocast|BlueprintAutocast|DeterminesOutputType|CompactNodeTitle|Conversion|Promotable|TypePromotion|CanCreateConnection|TryCreateConnection|CreateAutomaticConversionNodeAndConnections" Source\Editor\BlueprintGraph Source\Editor\Kismet Source\Runtime\Engine
```

Expected: output identifies cast nodes, function conversion nodes, schema automatic conversion and type promotion hooks.

- [ ] **Step 2: Compare BlueprintHelper transform support**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "transform_operation|type_promotion|dynamic_cast|class_dynamic_cast|convert|GenericTransform|ProjectedSpawner|ActionDatabase" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
```

Expected: output shows which transform operations require projected spawner evidence and which are still explicit special cases.

- [ ] **Step 3: Produce transform coverage table**

Write rows for object dynamic cast, class dynamic cast, interface cast, enum to string/name/text, numeric conversion, string/name/text conversion, object to soft object, class to soft class, automatic conversion inserted during link, and function-backed conversion. For each row, record editor action source, GraphWrite TaskSpec shape, required evidence, link behavior and readback check.

## Task 7: Schedule And Schedule Operation

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Classes/K2Node_AsyncAction.h`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private/K2Node_AsyncAction.cpp`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Classes/Kismet/KismetSystemLibrary.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/KismetSystemLibrary.cpp`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Public/TimerManager.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/TimerManager.cpp`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine/LatentActionManager.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/LatentActionManager.cpp`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Enumerate timer, latent and async menu actions**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "SetTimer|ClearTimer|PauseTimer|UnPauseTimer|Delay|RetriggerableDelay|MoveComponentTo|AsyncAction|BlueprintAsyncActionBase|Latent|LatentInfo|FLatentActionInfo|WorldContext" Source\Editor\BlueprintGraph Source\Editor\Kismet Source\Runtime\Engine Source\Runtime\UMG
```

Expected: output identifies timer function actions, latent function actions and async proxy node actions.

- [ ] **Step 2: Compare current schedule operation boundaries**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "schedule_operation|timer_delegate_node|latent_or_async_node|GenericSchedule|handler|signature|latent|async|ProjectedSpawner" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
```

Expected: output shows that GraphWrite does not create handler/signature for delegate schedule nodes and relies on projection/evidence for supported generic schedule paths.

- [ ] **Step 3: Produce schedule ownership table**

Write rows for timer by function name, timer by event/delegate, clear timer, pause timer, delay, retriggerable delay, latent function call, async action node, timeline-like scheduler and tick-like scheduler. For each row, record whether GraphWrite should create only the call node, whether another cluster owns handler/signature creation, required ActionDatabase evidence and readback proof.

## Task 8: Container Action

**Files:**
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Classes/Kismet/KismetArrayLibrary.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/KismetArrayLibrary.cpp`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Classes/Kismet/BlueprintMapLibrary.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/BlueprintMapLibrary.cpp`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Classes/Kismet/BlueprintSetLibrary.h`
- Read: `E:\UE_5.6\Engine\Source\Runtime\Engine\Private/BlueprintSetLibrary.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionFilter.cpp`
- Read: `E:\UE_5.6\Engine\Source\Editor\BlueprintGraph\Private\BlueprintActionDatabase.cpp`
- Read: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Enumerate callable container functions**

Run:

```powershell
Set-Location 'E:\UE_5.6\Engine'
rg -n "UFUNCTION|Array_|Map_|Set_|GenericArray_|GenericMap_|GenericSet_|CustomThunk|ArrayParm|MapParam|SetParam|AutoCreateRefTerm|BlueprintCallable|BlueprintPure" Source\Runtime\Engine\Classes\Kismet Source\Runtime\Engine\Private
```

Expected: output identifies every Blueprint-callable array, map and set operation plus wildcard/template metadata.

- [ ] **Step 2: Compare with BlueprintHelper vocabulary**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "FBlueprintHelperContainerActionVocabulary|array|get|set|add_unique|append|insert|remove|contains|find|length|keys|values|to_array|KismetArrayLibrary|BlueprintMapLibrary|BlueprintSetLibrary" BlueprintHelper\Source\BlueprintHelper AgentFaceService\task-core\src\task
```

Expected: output shows the exact current vocabulary and whether any engine-callable operation is missing.

- [ ] **Step 3: Produce container coverage table**

Write rows for each callable engine operation. Use these columns:

| Column | Meaning |
| --- | --- |
| `collection_kind` | `array`, `map` or `set`. |
| `operation` | Public GraphWrite operation name. |
| `engine_function` | Exact `UFunction` path. |
| `callable_in_normal_graph` | Whether the editor action menu exposes it in EventGraph / FunctionGraph. |
| `overlaps_function_action` | Whether FunctionAction can already call it without first-class container semantics. |
| `needs_first_class_shape` | Whether pin inference, wildcard template binding or readback requires dedicated `container_action`. |
| `readback_success` | Required proof after creation and link. |

## Task 9: Review Evidence And Readback Rules

**Files:**
- Read: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Read: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_AssetActionReviewPolicy_GraphBlockPlan_20260525_CN.md`
- Read: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Create report section: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

- [ ] **Step 1: Normalize evidence levels**

Run:

```powershell
Set-Location 'D:\UEProjects\Template\Plugins\BlueprintHelper'
rg -n "graph_block|atomic target|asset_action|container_action|generic_schedule|generic_transform|Review evidence|BuildReviewEvidence|CapabilityContract|readback" AgentFaceService\task-core\src BlueprintHelper\Source\BlueprintHelper BlueprintHelper\Develop\Plan
```

Expected: output shows public contract evidence, runtime Review evidence and docs that must stay aligned.

- [ ] **Step 2: Apply graph_block Review policy**

Record that `asset_action` does not require a public asset-action atomic target for this phase. Successful GraphWrite graph mutation should prove Review at `graph_block` surface level, while DebugBundle can keep lower-level diagnostic evidence.

- [ ] **Step 3: Define readback pass criteria**

Use this table in the result document:

| Capability | Minimum readback proof |
| --- | --- |
| `construct` | Node class, struct/container type, visible pins, linked pins, compiled graph without struct pin reconstruction errors. |
| `deconstruct` | Node class, source type, output field pins, linked consumers, compiled graph without break/split type errors. |
| `select` | Node class, option count, final index type, final result type, all option pins matching result type. |
| `control` | Node or macro source, exec pin names, dynamic pin count where applicable, graph type legality. |
| `create` | Node class, class/object/widget/asset source, spawn/create pins, return pin type and linked success path. |
| `convert` | Node class or function source, input type, output type, cast success pins where applicable. |
| `schedule` | Node class or function source, latent/timer/async metadata, handler/signature ownership result, continuation pins. |
| `asset_action` | Selected spawner identity, graph-block Review target, created node class and stable pins. |
| `container_action` | Engine function path, collection type, wildcard replacement, linked pins and compile result. |

## Result Document Requirements

The new thread should write:

`BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`

Required sections:

- `Executive Conclusion`: one paragraph stating which capabilities are already close to 80%, which are below 80%, and which are blocked by missing engine facts.
- `Editor Action Inventory`: all editor menu entries found from ActionDatabase / schema / node spawner inspection.
- `BlueprintHelper Current Coverage`: direct comparison with current TaskSpec schema, compiler, resolver, builder and capability contract.
- `Coverage Matrix`: one table per capability with editor behavior, current support, missing work and proposed owner.
- `Implementation Boundary`: precise statement of which work belongs to GraphWrite, FunctionAction, EventDelegate, Asset tooling or future non-GraphWrite tools.
- `Evidence And Readback Matrix`: exact evidence that can be provided by TaskSpec, resolver, fragment builder, graph readback and compile output.
- `Recommended Next Plans`: separate implementation plan names for construct/deconstruct/select, broad control, create/asset_action, transform/schedule and container_action if the result proves the work should be split.

## Verification Commands For This Task Document

Run from `D:\UEProjects\Template\Plugins\BlueprintHelper` after editing this task file:

```powershell
$terms = @('T'+'BD', 'TO'+'DO', 'place'+'holder', '<'+'number'+'>', '<'+'percentage'+'>', '<'+'defect'+'>', '<'+'decision'+'>', '待'+'定', '占'+'位')
Select-String -Path 'BlueprintHelper\Develop\Plan\BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadTask_20260525_CN.md' -Pattern $terms
rg -n "[ \t]$" BlueprintHelper\Develop\Plan\BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadTask_20260525_CN.md
git diff --check -- BlueprintHelper\Develop\Plan\BlueprintHelper_GraphWrite_GenericOps_UEEditorCapability_EngineSourceReadTask_20260525_CN.md
```

Expected:

- First command returns no matches.
- Second command returns no matches.
- `git diff --check` returns no warnings.
