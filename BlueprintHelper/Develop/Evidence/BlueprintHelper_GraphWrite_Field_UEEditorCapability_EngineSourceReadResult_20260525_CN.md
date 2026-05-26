# BlueprintHelper GraphWrite Field UE Editor Capability Engine Source Read Result

日期：2026-05-25  
任务：GraphWrite `get` / `set` / `get_property` / `set_property` / `component_ref` / `field_access` UE 5.6 编辑器能力源码证据探索  
范围：只读 UE 5.6 引擎源码；不修改 BlueprintHelper、AgentFaceService 或 UE 源码。  
源码根目录约定：本文所有证据路径均使用 `Source/...` 相对路径。

---

## 0. 清洗后结论摘要

本次清洗以 BlueprintHelper 当前能力边界为准：插件不实现偏 UI 的交互路径，例如 Action Menu、右键菜单、变量拖拽、pin-drag、拖拽 Pin、Pin 菜单呼出等。它们只保留为 UE 编辑器入口证据或上游映射来源，不能直接建模为 GraphWrite 可执行 statement。

当前约束为：`TaskSpec` 是全局共享上下文；每个 `statement[]` 内部是独立执行上下文；不能依赖编辑器瞬态选择、拖拽事件、右键菜单状态、modifier key 或跨 statement 隐式状态。若后续需要扩展共享上下文，必须先单独讨论。

```text
BlueprintHelper first-class Field statement = 17
UI-only/editor-entry evidence = 4
support/readback-only = 2
other cluster = 3
diagnostic/deferred = 2
```

原始 84.0% 是“引擎编辑器入口覆盖率”口径，不再作为 BlueprintHelper TaskSpec 直接可实现率。清洗后，适合提取为 BlueprintHelper first-class 能力的是 member/local/component/object-pin/struct-property/nested property path 等稳定语义；UI 入口只负责映射到这些稳定语义，而不是成为独立能力。

---

## 1. 源码探索方法与证据边界

| 项 | 结论 |
|---|---|
| 检索根 | UE 5.6 引擎源码根目录，本文写作时只保留 `Source/...` 相对路径。 |
| 读取模块 | `Editor/BlueprintGraph`、`Editor/Kismet`、`Editor/UnrealEd`、`Editor/KismetCompiler`、`Runtime/Engine`。 |
| 计数单位 | 一个“编辑器可见操作”按用户入口 + 目标节点族 + GraphWrite owner 决策计一行；同一底层节点若在编辑器中有不同入口且 GraphWrite 需要不同 TaskSpec/readback 证据，拆成多行。 |
| 未计入分母 | Class Defaults / Details 面板属性编辑、DataAsset/DataTable 资产属性写入、UMG Designer 层级属性编辑、Animation Blueprint 专属图、make/break 作为独立 FunctionAction/ContainerAction 语义。 |
| BlueprintHelper 实现源码限制 | 本次输入未包含 `BlueprintHelper/Source` 的 Field 实现源码；因此“当前 BlueprintHelper Field 实现缺陷”部分不做插件源文件级定位，只基于 UE 5.6 源码证据列出后续实现前必须补齐的真实缺口。 |
| BlueprintHelper 清洗口径 | UI 入口、右键菜单、拖拽、Pin 菜单、编辑器选中态只作为证据或上游映射来源；TaskSpec/statement 只能接收稳定的图、节点、pin、field、owner、path、link、readback 语义。 |

---

## 2. 任务 1：Action Menu、右键、拖拽、pin-drag 入口证据表

本节只保留为 UE 编辑器入口证据。BlueprintHelper 不实现这些 UI 事件本身；可执行语义必须由上游映射为结构化 `field.*` statement。

| editor_entry | filter_context | spawner_class | node_class / api | required_graphwrite_evidence | source_path | BlueprintHelper 清洗标记 |
|---|---|---|---|---|---|---|
| 空白图右键 Action Menu | `FBlueprintActionContext` 收集 Blueprint、Graph、Pins、SelectedObjects；`MakeContextMenu` 基于 context sensitive、target mask 和 filter flags 生成菜单。 | `UBlueprintVariableNodeSpawner`、`UBlueprintFieldNodeSpawner`、其他 NodeSpawner | `UK2Node_VariableGet`、`UK2Node_VariableSet`、struct operation 节点等 | Blueprint、target graph、context sensitive、graph schema、self class、selected object、dragged pins。 | `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:531-570`; `Source/Editor/BlueprintGraph/Public/BlueprintActionFilter.h:47-83`; `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:500-563` | 【不适合直接实现】右键菜单是 UI 入口；应映射为具体 `field.*` statement。 |
| 从 pin 拖出菜单 | `DraggedFromPins` 进入 Context.Pins；object/interface/self pin 会投影为 target class；sibling object 输出 pin 也参与 target class。 | `UBlueprintVariableNodeSpawner`、`UBlueprintFunctionNodeSpawner` | 外部 target pin 的 variable get/set；function call/call-on-member 分流。 | 起始 pin ref、pin type、pin direction、owning node、object class、sibling output class、target pin link 预期。 | `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:594-597`; `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:278-309` | 【不适合直接实现】pin-drag 是 UI 投影入口；只可提取为显式 `target_pin_ref` / `owner_class` / `expected_link`。 |
| My Blueprint 变量选择后右键 | `SelectionAsVar()` 的 `FProperty` 放入 `SelectedObjects`，用于过滤和绑定变量行为。 | `UBlueprintVariableNodeSpawner` | `UK2Node_VariableGet` / `UK2Node_VariableSet` | selected variable property、owner Blueprint、member guid、self/external context。 | `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:598-603`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:43-111` | 【不适合直接实现】`SelectedObjects` 是编辑器选择态；TaskSpec 必须携带稳定 field 标识。 |
| SCS/组件树选择后右键 | 组件树选中节点的 `VariableName` 在 `SkeletonGeneratedClass` 上查找 `FObjectProperty` 并放入 `SelectedObjects`。 | 对已有组件引用：`UBlueprintVariableNodeSpawner`；不是 `UBlueprintComponentNodeSpawner`。 | `UK2Node_VariableGet` / `UK2Node_VariableSet`；Add Component 另行排除。 | selected component variable name、skeleton class property、component guid/name、ordinary graph context。 | `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:604-613`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747`; `Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp:78-260` | 【不适合直接实现】组件树选择态不是 statement 输入；只可映射为 `component_ref`。 |
| 变量面板拖拽到 graph panel | Ctrl 生成 Get，Alt 生成 Set；否则打开菜单显示 `Get {0}` / `Set {0}`；本地变量限制在同一 Blueprint/function graph。 | schema API 调用变量节点生成；内部仍走 `ConfigureVarNode`。 | `UK2Node_VariableGet` / `UK2Node_VariableSet` | source property、drop graph、modifier key、local variable scope、writable/readable state。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:424-552`; `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:555-574` | 【不适合直接实现】拖拽和 modifier key 不建模；转为确定的 `get` / `set` statement。 |
| 变量拖拽到 pin | `RequestVariableDropOnPin` 校验可否 dropped；兼容则生成 Get 或 Set 并自动连线。 | schema API + `ConfigureVarNode` | `UK2Node_VariableGet` / `UK2Node_VariableSet` | target pin ref、pin direction、pin type、drop graph、source variable property、预期 link。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:288-348`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7640` | 【不适合直接实现】drop 事件不建模；转为 `get/set + target_pin_ref + expected link`。 |
| pin 菜单 Promote / Split / Recombine | schema 在 pin context 菜单添加 Promote to Variable、Split Struct Pin、Recombine Struct Pin。 | schema API | promote variable；split/recombine pin API | target pin type、pin direction、struct splittable state、linked pins/defaults。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:1506-1556`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:1370-1413`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7338-7557` | 【support-only】Pin 菜单不做用户 statement；Split/Recombine 仅服务 `get_property` / `set_property` readback。 |

**任务 1 结论：** GraphWrite 不能只接受 `variable_name`，但也不能把 UI 入口上下文当成直接执行输入。BlueprintHelper 只应消费 TaskSpec 全局上下文与 statement-local 结构化上下文；ActionResolution 再保存或校验 `FMemberReference`、pin type、link/default、split state。

---

## 3. 任务 2：变量、本地变量、参数、返回值覆盖

| 操作 | UE 表达与判断 | GraphWrite 结论 | source_path |
|---|---|---|---|
| member variable get/set | Class property actions 遍历 `BlueprintVisible` 的 `FProperty`，分别创建 getter 和 setter；spawner post-spawn 调 `SetFromProperty`，底层存 `FMemberReference`。 | P0。TaskSpec 需要 `field_name` + owner/self 语义；readback 必须验证 node class、`FMemberReference`、property pin type。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-779`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:43-111`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:87-132` |
| inherited/native member get/set | Action database 对类和 Blueprint skeleton class 注册 member actions；非 self context 会保存 owner class / member guid；节点解析时通过 skeleton/generated class resolve property。 | P1。不能只保存变量名；必须保存/校验 owner class、member guid 或 stable owner path。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:1568-1592`; `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:1649-1656`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:447-580` |
| local variable get/set | Function graph action 遍历 `UK2Node_FunctionEntry::LocalVariables`，spawner 设置 `LocalVarOuter`，最终 `SetLocalMember(LocalVar.VarName, FunctionGraph->GetName(), LocalVar.VarGuid)`。 | P0。TaskSpec 必须包含 function graph/scope；ActionContext 可从 target graph 投影，不能跨函数图生成。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:846-906`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:115-167`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:183-253`; `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:555-574` |
| function input parameter get | Function graph action 对 `FUNC_Parm` 且非 out/ref 的参数创建 `UK2Node_VariableGet` spawner；参数作为 variable node 暴露，而不是从 entry pin 直接读取。 | P1。GraphWrite 可归入 `get`，但 scope 是 function graph / UFunction 参数，readback 要验证 parameter property 和 function owner。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:846-868`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:43-111` |
| function output/return set | Function terminator 创建 Return Node；输出参数是 Return Node 的输入 pins，属于 control/return 节点，不是变量 set。 | excluded。归 Control/Return cluster，不作为 Field set 生成。 | `Source/Editor/BlueprintGraph/Private/K2Node_FunctionResult.cpp:156-219`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:3339-3428` |
| by-ref variable set | `UK2Node_VariableSetRef` 是 wildcard target by-ref + value 的特殊节点，注册为全局 node action；类型由连接 pin 推断。 | P2/deferred。可作为诊断或后续专门能力；不应混入普通 `set` 的 `UK2Node_VariableSet` readback。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableSetRef.cpp:165-175`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSetRef.cpp:243-345` |
| readonly/private/not visible | `UK2Node_VariableGet/Set` 编译校验 private/not BlueprintVisible/not readable/writable，并给出 error。 | 不生成或生成后必须失败回滚；DebugBundle 写入 reason。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456` |

---

## 4. 任务 3：`component_ref` 真实编辑器范围

| component_kind | ordinary_blueprint_graph_visible | spawner_class | member_reference_shape | graphwrite_scope | source_path |
|---|---:|---|---|---|---|
| SCS component | 是。组件在 Skeleton/Generated class 上表现为 `FObjectProperty`，右键/拖拽可作为变量 Get。 | `UBlueprintVariableNodeSpawner` | component variable name + SCS `VariableGuid` + owner Blueprint skeleton/generated class。 | `component_ref` P0；生成 `UK2Node_VariableGet`。 | `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3654-3686`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747`; `Source/Runtime/Engine/Classes/Engine/SCS_Node.h:68-156` |
| inherited SCS component | 是。SCS node 可通过 inherited handler 获取 actual template；图内引用仍应落到组件变量 property。 | `UBlueprintVariableNodeSpawner` | owner class + inherited component variable name/guid；template 仅作为 component metadata/readback 辅助。 | `component_ref` P1；支持 inherited resolution。 | `Source/Runtime/Engine/Private/SCS_Node.cpp:29-53`; `Source/Editor/Kismet/Public/SSCSEditor.h:123-196`; `Source/Editor/Kismet/Public/SSCSEditor.h:359-376` |
| native inherited component | 是，只要 native class 暴露为 BlueprintVisible component `FObjectProperty`，ActionDatabase 会为 class property 注册 getter/setter。 | `UBlueprintVariableNodeSpawner` | native owner class + property name；可能无 Blueprint `NewVariables` guid。 | `component_ref` P1；owner class 是必需证据。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-779`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` |
| UCS / AddComponent template | 普通图内可创建的是 Add Component 节点，不是已有 component ref；模板查找通过 UCS component template。 | `UBlueprintComponentNodeSpawner` | component key/template name；不是 `FMemberReference` component variable。 | 排除 `component_ref`；归组件树/组件创建工具或 FunctionAction 附属。 | `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7749-7784`; `Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp:78-260` |
| instance-only component | 普通 Blueprint class graph 内没有稳定 class property；运行时实例引用需函数/事件上下文传入。 | 无稳定 component variable spawner | instance pointer/path 不可作为 Blueprint class graph 稳定证据。 | 诊断：`not_class_component_property`。 | `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747`; `Source/Runtime/Engine/Classes/Engine/SCS_Node.h:149-156` |

**任务 3 结论：** `component_ref` 的 first-class 实现不应调用 `UBlueprintComponentNodeSpawner`。该 spawner 生成 `UK2Node_AddComponent`，不是“引用已有组件”。已有 SCS/native/inherited 组件在普通 graph 内应按 component `FObjectProperty` 生成 `UK2Node_VariableGet`，并以 SCS/name/guid/owner class 做 readback。

---

## 5. 任务 4：`field_access` 与 object-pin 成员访问边界

| 问题 | UE 源码证据 | GraphWrite 结论 | source_path |
|---|---|---|---|
| typed object pin 访问成员变量 | pin-drag 会把 object/self/interface pin 的 class 加入 action filter target classes；变量 spawner 会根据 target class 调整 TargetPin 类型；非 self 变量节点有 visible Target pin。 | P2 `field_access`。生成外部 `UK2Node_VariableGet/Set`，并连接 typed object pin 到 Target pin；TaskSpec 需要 `target_pin_ref + owner_class/member + field_name`。 | `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:197-230`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207` |
| component pin 访问成员属性 | component ref 本身是 object pin；继续访问其 property 与 typed object pin member access 同构。 | P2。表达为 `component_ref + property_path` 或 `field_access(target=component_pin)`；不要走组件树属性编辑工具。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` |
| 函数返回对象访问成员 | 函数返回 pin 是 typed object output pin；field access 可直接把返回 pin 连接到 variable node Target pin。若需要多次复用或 exec ordering，才需要临时变量。 | P2。默认不强制 temp；只在多消费者/readability 或 linked pin conflict 时由 FragmentBuilder 选择 temp。 | `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302` |
| 成员函数和成员字段重叠 | function spawner 对 object property binding 可能生成 `UK2Node_CallFunctionOnMember`，其扩展为中间 function call + variable get；字段 access 本身仍是 variable node。 | 成员函数归 FunctionAction；Field 只负责字段/property 节点链。不要把 function call 当作 `field_access` 成功。 | `Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp:92-154`; `Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp:460-514`; `Source/Editor/BlueprintGraph/Private/K2Node_CallFunctionOnMember.cpp:30-130` |
| ambiguous member name | 仅 `field_name` 不足以区分 self/inherited/native/local/parameter/object target；UE 通过 owner class、local var outer、member guid、target pin type 和 skeleton class resolve。 | ActionResolution 必须形成 stable field path：`scope_kind + owner_class + member_name + member_guid? + target_pin_type?`。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:183-253`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:447-580` |

---

## 6. 任务 5：复杂 property path 与 struct member 实现方式

| property path 类型 | 预期 UE 表达 | GraphWrite 建议 | source_path |
|---|---|---|---|
| simple member variable | `UK2Node_VariableGet/Set`，变量 pin type 从 `FProperty` 转换，self/external target pin 由 `FMemberReference` 决定。 | `get/set` P0。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:87-132`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:97-145` |
| struct read member | 右键 pin 可 Split Struct Pin；编译时 split output pin 会扩展为 `UK2Node_BreakStruct`；也可直接放置 BreakStruct。 | `get_property` P1；FragmentBuilder 可生成 BreakStruct 或使用 split pin，但 readback 必须识别 parent/subpin。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:1370-1413`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7201-7335`; `Source/Editor/BlueprintGraph/Private/K2Node_BreakStruct.cpp:182-323` |
| struct write member | `UK2Node_SetFieldsInStruct` 提供 by-ref StructRef/StructOut 和可见 member input pins；也可通过 make/set chain。 | `set_property` P1；优先 SetFieldsInStruct，复杂嵌套由 dedicated write fragment 组合。 | `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:161-250`; `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:287-414` |
| nested struct path | 多级 split/make/break/set members；split state 由 ParentPin/SubPins 和 `RestoreSplitPins` 维护。 | P2 dedicated property path fragment；禁止散落硬编码分支。 | `Source/Editor/BlueprintGraph/Private/K2Node.cpp:824-870`; `Source/Editor/BlueprintGraph/Private/K2Node.cpp:1506-1585`; `Source/Editor/BlueprintGraph/Private/K2Node_StructOperation.cpp:53-147` |
| object property path | object target + member variable node；如果后续字段还是 struct，则接 struct fragment。 | P2 `field_access` + `property_path` 组合；由目标 pin type 和 owner class 证据决定。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207`; `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750` |
| split pin roundtrip | `SplitPin` 会隐藏 parent pin、生成 subpins、复制 default；`RecombinePin` 恢复 parent default 并清除 subpins。 | 作为 property fragment/readback 支撑能力，不建议作为单独用户 statement first-class。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7338-7557` |
| unsupported/private/readonly path | ActionFilter 不可见，或编译阶段 `ValidateNodeDuringCompilation` 报 private/not visible/read-only。 | 不生成或生成后判失败；返回 reason，DebugBundle 收集。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456`; `Source/Editor/BlueprintGraph/Private/K2Node_BreakStruct.cpp:281-323` |

---

## 7. 任务 6：readback、typed pin、linked pin 与成功判定

| evidence | 通过标准 | source_path |
|---|---|---|
| node class | 节点类必须属于预期族：`UK2Node_VariableGet`、`UK2Node_VariableSet`、`UK2Node_BreakStruct`、`UK2Node_SetFieldsInStruct`；`UK2Node_CallFunctionOnMember` 不可冒充 Field。 | `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:43-111`; `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:161-185`; `Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp:460-514` |
| member reference | `FMemberReference` 能 resolve 到目标 property；local scope 必须包含 function graph scope 和 guid；external member 需 owner class。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:447-580` |
| pin type | data pin type 必须与 TaskSpec 或 linked typed pin projection 一致；变量 pin type 来自 property-to-pin 转换。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:93-132`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:4100-4116` |
| linked pin | 需要连接的 input/output pin 已连接到预期节点；连接合法性由 `CanCreateConnection` / `TryCreateConnection` / compiler validate link 共同确认。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302`; `Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-854` |
| default value | 未连接 literal/default 必须保留在 pin default；split pin 会复制 parent default 到 subpins，recombine 恢复 parent default。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7338-7557`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:93-132` |
| split state | `ParentPin` / `SubPins` 与 property path 一致；reconstruct 后 `RestoreSplitPins` 能复原 split pin 和 wires。 | `Source/Editor/BlueprintGraph/Private/K2Node.cpp:824-870`; `Source/Editor/BlueprintGraph/Private/K2Node.cpp:1506-1585` |
| compile result | Blueprint 编译无该 statement 相关 error；warning 进入 DebugBundle。Compiler 会报 pin mismatch、多连接非法、变量不可读/不可写、struct 不可 break/set。 | `Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-920`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456`; `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:225-250` |

---

## 8. 任务 7：BlueprintHelper 清洗矩阵与实现优先级

| # | capability_id | editor_operation | graph_scope | node_family | ue_node_class / api | spawner_or_api | minimum_task_spec_context | action_context_projection | readback_evidence | coverage_priority | owner_cluster | reason | source_path |
|---:|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `field.member_get` | Get Blueprint member variable | EventGraph / FunctionGraph | variable | `UK2Node_VariableGet` | `UBlueprintVariableNodeSpawner::CreateFromMemberOrParam` | blueprint, graph, variable name, optional owner | self class, member guid, skeleton property | node class, `FMemberReference`, output pin type | P0 | FieldVariableAction | 最常见 Field read。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-779`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:43-111` |
| 2 | `field.member_set` | Set Blueprint member variable | EventGraph / FunctionGraph | variable | `UK2Node_VariableSet` | `UBlueprintVariableNodeSpawner::CreateFromMemberOrParam` | blueprint, graph, variable name, writable expected | self class, member guid, skeleton property | exec pins, input/output data pins, writable validation | P0 | FieldVariableAction | 最常见 Field write。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-779`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:97-145` |
| 3 | `field.inherited_member_get` | Get inherited/native member | EventGraph / FunctionGraph | variable | `UK2Node_VariableGet` | class member actions | owner class + field name/guid | parent/native class actions, target class | external `FMemberReference`, target/self pin | P1 | FieldVariableAction | 主路径必需；解决父类/原生属性。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:1568-1592`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610` |
| 4 | `field.inherited_member_set` | Set inherited/native writable member | EventGraph / FunctionGraph | variable | `UK2Node_VariableSet` | class member actions | owner class + field + writable | parent/native class, pin type | writable validation, exec/data pins | P1 | FieldVariableAction | 父类属性写入不能只凭名称。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:1568-1592`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456` |
| 5 | `field.sparse_data_get` | Get sparse class data property | EventGraph / FunctionGraph | variable-like | `UK2Node_VariableGet` | sparse data action | owner class + sparse data field | class sparse data object | getter only, no setter | P1 | FieldVariableAction | UE 源码明确 setter 暂未注册。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:782-806` |
| 6 | `field.function_param_get` | Get function input parameter | FunctionGraph | variable | `UK2Node_VariableGet` | parameter property spawner | function graph, param name, owner function | function graph, function property | param property resolves, output pin type | P1 | FieldVariableAction | UE 将 input param 表达为 variable get。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:846-868` |
| 7 | `field.local_get` | Get local variable | FunctionGraph | local variable | `UK2Node_VariableGet` | `CreateFromLocal` | function graph scope, local name/guid | target graph function entry local vars | local scope `FMemberReference`, output pin type | P0 | FieldVariableAction | 函数图常见。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:846-906`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:115-167` |
| 8 | `field.local_set` | Set local variable | FunctionGraph | local variable | `UK2Node_VariableSet` | `CreateFromLocal` | function graph scope, local name/guid | target graph function entry local vars | local scope, exec pins, input/output data pins | P0 | FieldVariableAction | 函数图常见。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:846-906`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:183-253` |
| 9 | `field.drag_get` | My Blueprint 变量拖拽 Get | EventGraph / FunctionGraph | variable drag | `UK2Node_VariableGet` | schema `SpawnVariableGetNode` | variable source, drop graph, graph position | selected property, modifier key | created get node and optional link/default | excluded UI-entry | UI entry evidence | 【不适合】拖拽是 UI 触发；BlueprintHelper 只保留为 `field.member_get` / `field.local_get` 等语义映射来源。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:424-443`; `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:470-552` |
| 10 | `field.drag_set` | My Blueprint 变量拖拽 Set | EventGraph / FunctionGraph | variable drag | `UK2Node_VariableSet` | schema `SpawnVariableSetNode` | variable source, drop graph, writable expected | selected property, modifier key, graph supports impure | set node, exec/data pins, writable validation | excluded UI-entry | UI entry evidence | 【不适合】拖拽和 modifier key 不作为 statement；上游应映射为确定的 `set` 语义。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:424-468`; `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:470-552` |
| 11 | `field.pin_drag_get` | Drop variable on input pin, create Get and connect | EventGraph / FunctionGraph | linked variable | `UK2Node_VariableGet` + link | `RequestVariableDropOnPin` + `ConfigureVarNode` | variable property, target pin ref, expected link | pin type/direction, graph schema | node + link to target input pin | excluded UI-entry | UI entry evidence | 【不适合】pin 拖拽事件不建模；可执行语义必须显式提供 `target_pin_ref` 与 `expected link`。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:288-348`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302` |
| 12 | `field.pin_drag_set` | Drop variable on output/exec pin, create Set and connect | EventGraph / FunctionGraph | linked variable | `UK2Node_VariableSet` + link | `RequestVariableDropOnPin` + `ConfigureVarNode` | variable property, source pin ref, expected link | pin type/direction, graph schema | node + link to set input/value/exec as applicable | excluded UI-entry | UI entry evidence | 【不适合】pin 拖拽事件不建模；连接驱动 set 只保留为显式 link/schema validation。 | `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:288-348`; `Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-920` |
| 13 | `field.object_pin_member_get` | 从 typed object pin 读取成员 | EventGraph / FunctionGraph | field_access | `UK2Node_VariableGet` with Target pin | variable spawner filtered by target pin class | target pin ref, owner class, field | pin object class, target classes | Target pin linked, member resolves, output type | P2 | FieldVariableAction | 通用化关键，需 ActionContext projection。 | `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207` |
| 14 | `field.object_pin_member_set` | 从 typed object pin 写入成员 | EventGraph / FunctionGraph | field_access | `UK2Node_VariableSet` with Target pin | variable spawner filtered by target pin class | target pin ref, owner class, field, value | pin object class, target classes | Target/value/exec pins linked, writable validation | P2 | FieldVariableAction | 复杂但属于 Field，不归 FunctionAction。 | `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:197-230`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456` |
| 15 | `field.component_ref_get` | Get component reference | EventGraph / FunctionGraph | component variable | `UK2Node_VariableGet` | `UBlueprintVariableNodeSpawner` | component name/guid/owner BP | SCS/native component `FObjectProperty` | member ref resolves to component object property | P0 | FieldVariableAction | 普通 Actor BP 最常见组件引用。 | `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3654-3686`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` |
| 16 | `field.component_ref_set` | Set component variable/property reference | EventGraph / FunctionGraph | component variable | `UK2Node_VariableSet` | `UBlueprintVariableNodeSpawner` | component property + writable intent | component property writable state | set node or diagnostic if read-only | P2 | FieldVariableAction | 支持可写属性；常见组件引用本身多为只读，应诊断。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` |
| 17 | `field.component_property_get` | 从 component pin 读取成员属性 | EventGraph / FunctionGraph | field_access + component | `UK2Node_VariableGet` with Target pin | target class filtered variable spawner | component pin ref, property path | component output pin type | Target link + member output pin type | P2 | FieldVariableAction | `component_ref + property_path`。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207`; `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750` |
| 18 | `field.component_property_set` | 从 component pin 写入成员属性 | EventGraph / FunctionGraph | field_access + component | `UK2Node_VariableSet` with Target pin | target class filtered variable spawner | component pin ref, property path, value | component output pin type | Target/value/exec links + writable validation | P2 | FieldVariableAction | 与 typed object pin 同构。 | `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456` |
| 19 | `control.function_return_write` | Set function output / Return Node | FunctionGraph | terminator | `UK2Node_FunctionResult` | schema creates terminator | function graph, out params | function signature | return node pins | excluded | Control/Return | 【不适合】不是 Field set，归 Control/Return。 | `Source/Editor/BlueprintGraph/Private/K2Node_FunctionResult.cpp:156-219`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:3339-3428` |
| 20 | `function.selected_component_call` | 选中组件后调用成员函数 | EventGraph / FunctionGraph | call-on-member | `UK2Node_CallFunctionOnMember` | `UBlueprintFunctionNodeSpawner` | function member + component property | selected component property | call node expands to function + variable get | excluded | FunctionAction | 【不适合】函数调用归 FunctionAction；选中组件是 UI 入口。 | `Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp:92-154`; `Source/Editor/BlueprintGraph/Private/K2Node_CallFunctionOnMember.cpp:63-130` |
| 21 | `component.add_component_node` | Add Component action/menu | EventGraph / Construction-related graph | component creation | `UK2Node_AddComponent` | `UBlueprintComponentNodeSpawner` | component class/template | action menu component class | add component template/name | excluded | existing component tool / FunctionAction | 【不适合】UI action 分流；不是引用已有组件。 | `Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp:78-260` |
| 22 | `field.split_struct_pin_support` | Split Struct Pin | EventGraph / FunctionGraph | split pin | schema split API | `UEdGraphSchema_K2::SplitPin` | pin ref, struct type, path element | pin direction, struct metadata | ParentPin/SubPins, default/link migration | support/readback-only | FieldVariableAction support | 【support-only】作为 property fragment/readback 支撑，不作为独立 user statement。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:1370-1413`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7338-7464` |
| 23 | `field.recombine_struct_pin_support` | Recombine Struct Pin | EventGraph / FunctionGraph | split pin | schema recombine API | `UEdGraphSchema_K2::RecombinePin` | parent/subpin refs | current split state | subpins removed, parent defaults restored | support/readback-only | FieldVariableAction support | 【support-only】支撑 roundtrip/readback，不新增 statement。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7466-7557` |
| 24 | `field.struct_member_get` | Break struct / read struct member | EventGraph / FunctionGraph | break struct | `UK2Node_BreakStruct` | `UBlueprintFieldNodeSpawner` / schema split expansion | struct pin/ref + member path | struct type metadata | break node or split subpin, output link/default | P1 | FieldVariableAction | 主路径 property read 必需。 | `Source/Editor/BlueprintGraph/Private/K2Node_BreakStruct.cpp:182-323`; `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7201-7335` |
| 25 | `field.struct_member_set` | Set Members in Struct / write struct member | EventGraph / FunctionGraph | set fields in struct | `UK2Node_SetFieldsInStruct` | `UBlueprintFieldNodeSpawner` / struct op | struct ref, member path, value, exec context | struct type metadata | StructRef/StructOut/value/exec pins, warnings | P1 | FieldVariableAction | 主路径 property write 必需。 | `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:161-250`; `Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp:287-414` |
| 26 | `field.nested_property_path` | Nested `Object.Property.SubProperty` | EventGraph / FunctionGraph | property path fragment | variable + break/set/split chain | dedicated FragmentBuilder | target root + stable field path segments | target pin/root object + struct metadata | whole chain readback, node/link/subpin path | P2 | FieldVariableAction | 通用化关键；不可散落硬编码。 | `Source/Editor/BlueprintGraph/Private/K2Node.cpp:824-870`; `Source/Editor/BlueprintGraph/Private/K2Node_StructOperation.cpp:53-147` |
| 27 | `field.by_ref_set` | Set variable by ref | EventGraph / FunctionGraph | by-ref set | `UK2Node_VariableSetRef` | registered node action | target by-ref pin, value, exec | linked pin type | wildcard resolved, by-ref target/value pins | deferred/needs-discussion | FieldVariableAction diagnostic/deferred | 【deferred，不纳入 first-class】类型完全由连接推断；若需要跨 statement 共享 inferred pin context，必须先扩展上下文协议。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableSetRef.cpp:165-345` |
| 28 | `field.unsupported_path_diagnostic` | Private/readonly/not visible/missing field | EventGraph / FunctionGraph | diagnostic | no generation or failing node validation | resolver + compiler diagnostic | field path + reason | property metadata, action filter | reason, compile diagnostics, no success claim | excluded diagnostic | FieldVariableAction diagnostic | 【diagnostic-only】不生成能力，不应伪装生成成功。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:552-580` |

---

## 9. BlueprintHelper 能力提取建议

### 9.1 进入 BlueprintHelper first-class Field 能力

按 TaskSpec/statement 可稳定表达的语义清洗后，建议 first-class 覆盖以下 **17 个** capability id：

```text
field.member_get
field.member_set
field.inherited_member_get
field.inherited_member_set
field.sparse_data_get
field.function_param_get
field.local_get
field.local_set
field.object_pin_member_get
field.object_pin_member_set
field.component_ref_get
field.component_ref_set
field.component_property_get
field.component_property_set
field.struct_member_get
field.struct_member_set
field.nested_property_path
```

其中 P0 必须优先实现：`field.member_get`、`field.member_set`、`field.local_get`、`field.local_set`、`field.component_ref_get`。P1 覆盖 inherited/native、function parameter、struct member get/set。P2 覆盖 object-pin/member access、component pin property、nested property path。

### 9.2 UI 入口仅作映射证据，不作为 first-class 用户 statement

| capability_id | 清洗后处理 |
|---|---|
| `field.drag_get` | 不实现拖拽；上游可把拖拽结果映射为 `field.member_get` / `field.local_get` / `field.component_ref_get` 等稳定语义。 |
| `field.drag_set` | 不实现拖拽或 modifier key；上游必须映射为确定的 `field.member_set` / `field.local_set` 等稳定语义。 |
| `field.pin_drag_get` | 不实现 Pin 拖拽；若需要连接，statement 必须显式携带 `target_pin_ref`、`expected link`、schema validation 期望。 |
| `field.pin_drag_set` | 不实现 Pin 拖拽；连接驱动写入必须作为 statement-local link/schema 语义表达。 |

### 9.3 保留为 support/readback，不作为单独 first-class 用户 statement

| capability_id | 处理 |
|---|---|
| `field.split_struct_pin_support` | 作为 `get_property` / `set_property` 的内部实现和 readback 支撑；用户 DSL 不单独暴露为 Field statement。 |
| `field.recombine_struct_pin_support` | 作为 roundtrip/修复/重建支撑；不把 recombine 当业务 Field 写入。 |

### 9.4 保留现有工具或其他 cluster 职责

| capability_id | owner_cluster | 原因 |
|---|---|---|
| `control.function_return_write` | Control/Return | Return Node 是函数终止节点，不是 Field set。 |
| `function.selected_component_call` | FunctionAction | 成员函数调用可使用 `UK2Node_CallFunctionOnMember`，但字段 access 只负责变量/property。 |
| `component.add_component_node` | Blueprint Component Tools / AddComponent action | `UBlueprintComponentNodeSpawner` 创建 Add Component 节点，不是已有 component ref。 |

### 9.5 诊断与延期

| capability_id | 处理 |
|---|---|
| `field.unsupported_path_diagnostic` | 对 private、BlueprintReadOnly、not BlueprintVisible、missing local scope、unknown struct、unsupported instance-only component 返回失败 reason；不生成或不声明成功。 |
| `field.by_ref_set` | 第一阶段建议返回 `unsupported_by_ref_set_deferred`。若要依赖跨 statement 推断出的 linked pin 上下文，必须先讨论并扩展 TaskSpec 上下文协议。 |

---

## 10. 实现边界建议

### 10.0 TaskSpec / statement 上下文契约

- `TaskSpec` 是唯一全局共享上下文；每条 `statement[]` 必须自带完成该语义所需的稳定引用。
- statement 不能读取或假设 UI 瞬态状态，包括右键菜单、拖拽源、drop 目标、当前选中对象、modifier key、临时 Pin 菜单状态。
- statement 不能依赖上一条 statement 的隐式本地状态；如需复用前序产物，必须通过 TaskSpec/readback 中明确命名的 node/pin/path id 传递。
- 若能力必须引入共享 symbol table、跨 statement 临时变量、批量 pin inference cache 或 UI selection bridge，应先作为架构扩展议题与用户讨论。

### 10.1 TaskSpec 最小字段

| 能力族 | TaskSpec 必需字段 |
|---|---|
| member get/set | `blueprint_ref`、`graph_ref`、`field_name`、可选 `owner_class`、可选 `member_guid`、`mode=get/set`。 |
| inherited/native | `owner_class` 必填或可由 linked pin/target pin 精确投影；不能只用名称。 |
| local get/set | `graph_ref` 必须指向 function graph；`scope_name` 或 `function_name`、`local_name`、可选 `local_guid`。 |
| function param get | `function_name`、`param_name`、`param_flags` 或 resolved `FProperty` evidence。 |
| component_ref | `component_name`、可选 `component_guid`、`component_owner_class`、`component_kind`；禁止使用 template path 单独作为 graph ref。 |
| field_access | `target_pin_ref` 或 root expression、`owner_class`、`field_name`、可选 `member_guid`、expected target pin type；不得从 UI pin-drag 隐式继承。 |
| get_property/set_property | `root`、`field_path[]`、每段 `owner_type/member_name/member_guid?/struct_guid?`、expected read/write mode、linked pins/defaults；不得依赖跨 statement 隐式 path cache。 |

### 10.2 ActionContext projection

在一条已解析 statement 内，GraphWrite 可以从 TaskSpec 与当前图状态自动补齐：

```text
Blueprint / GeneratedClass / SkeletonGeneratedClass
target graph / graph schema / function graph scope
self class / parent class / target pin class
component property / SCS node variable name / SCS VariableGuid
explicit linked pin type / explicit linked pin direction / sibling object output pin class
struct metadata / split-pin capability / hidden/visible property state
```

不能自动补齐或不能硬编码：

```text
单个变量名、单个组件名、单个函数名、用户实例对象路径、Details 面板属性路径、DataAsset 资产内容路径、
右键菜单状态、拖拽源、drop 目标、DraggedFromPins、SelectedObjects、modifier key。
```

### 10.3 ActionResolution

ActionResolution 应产生统一 resolved fact：

```json
{
  "field_kind": "member|local|param|component|object_pin|struct_path",
  "node_family": "variable|get|set|break_struct|set_fields_in_struct|split_pin",
  "owner_class": "...",
  "member_name": "...",
  "member_guid": "...",
  "local_scope": "...",
  "target_pin_type": "...",
  "expected_node_class": "...",
  "expected_pin_type": "...",
  "expected_links": []
}
```

### 10.4 FragmentBuilder / Composer

| fragment | 生成策略 |
|---|---|
| VariableGetFragment | 生成 `UK2Node_VariableGet`；配置 `FMemberReference`；连接 optional target pin。 |
| VariableSetFragment | 生成 `UK2Node_VariableSet`；配置 `FMemberReference`；连接 exec/value/target pin。 |
| ComponentRefFragment | 本质是 component `FObjectProperty` 的 VariableGetFragment；附加 component metadata readback。 |
| ObjectFieldAccessFragment | root object pin -> variable target pin；可追加 struct fragment。 |
| StructReadFragment | 优先 BreakStruct 或 split pin 读；输出目标 member pin。 |
| StructWriteFragment | 优先 SetFieldsInStruct；nested path 用 read-modify-write fragment chain。 |
| PropertyPathFragment | 维护 full path、intermediate nodes、links、split state；禁止散落条件分支。 |

### 10.5 readback facts

每条 statement 应回写：

```text
created_nodes[]: node_guid, node_class, node_title, source capability
member_reference: member_name, owner_class, member_guid, local_scope, resolve_result
pins[]: pin_name, direction, pin_type, default_value, linked_to[], parent_pin, subpins[]
links[]: from_node/from_pin -> to_node/to_pin, schema_response
split_state[]: root_pin, subpins, hidden parent, restored/recombined state
compile_diagnostics[]: severity, message, node_guid, pin_guid, capability_id
```

---

## 11. 风险清单

| 风险 | 影响 | 源码证据 | 处理建议 |
|---|---|---|---|
| 只用 `variable_name` 缓存 resolver | inherited/native/local/parameter/object target 同名字段会歧义；local scope 丢失。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`; `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:183-253` | cache key 使用 `scope_kind + owner_class + member_name + guid + target pin type`。 |
| UI 入口被提升为 statement | 拖拽、右键菜单、SelectedObjects、modifier key 无法在 CLI/TaskSpec 中稳定重放，会把 UI 瞬态误当业务语义。 | `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:531-613`; `Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp:288-552` | UI 入口只做证据；执行层只接受稳定 TaskSpec 字段与 statement-local 引用。 |
| typed pin 推断缺失 | object-pin field access 生成结果与显式连接语义不一致。 | `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207` | statement 必须携带 target pin 和 projected target class；不得从 pin-drag UI 状态隐式继承。 |
| linked pin 只做字符串连接 | schema 会拒绝方向、类型、self、多连接等非法连接；运行后才暴露错误。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302`; `Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-920` | 写入前后都调用 schema compatibility/readback；DebugBundle 收集 schema response。 |
| component_ref 误用 `UBlueprintComponentNodeSpawner` | 会创建 Add Component 节点，而不是已有组件引用。 | `Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp:78-260`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` | component_ref 只走 component `FObjectProperty` variable get/set。 |
| nested property path 拆成临时特殊分支 | 多级 struct/object path 难以 readback、rollback、review、重建。 | `Source/Editor/BlueprintGraph/Private/K2Node.cpp:824-870`; `Source/Editor/BlueprintGraph/Private/K2Node_StructOperation.cpp:53-147` | 引入 dedicated PropertyPathFragment。 |
| split pin roundtrip 不完整 | 重建后 wire/default 丢失；Review 看到节点但语义已错。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7338-7557`; `Source/Editor/BlueprintGraph/Private/K2Node.cpp:824-870` | readback 必须记录 ParentPin/SubPins/default/link。 |
| 编译诊断未绑定 statement | private/readonly/not visible/missing local variable 会被声明为成功。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:552-580` | compile diagnostics 按 node_guid/capability_id 归因。 |
| Review/DebugBundle 粒度错配 | 若新增 per-field Review target，会破坏 graph_block 级审阅；若没有 Field facts，调试不足。 | 任务边界要求 Review evidence 保持 graph_block 级别；Field 细节进 DebugBundle/readback facts。 | Review target 不细分 field；DebugBundle 包含 field facts。 |

---

## 12. 当前 BlueprintHelper Field 实现与目标覆盖之间的真实缺陷

> 限制：本次输入未包含 `BlueprintHelper/Source` 的 Field 实现源码，因此以下不是对插件具体文件的静态代码审计结果，而是基于 UE 5.6 编辑器源码证据推导出的“后续实现若缺失则必然无法达到目标覆盖”的真实缺口。实现前必须用 BlueprintHelper 源码逐项复核。

| 缺陷/缺口 | 为什么是真实缺口 | UE 源码证据 |
|---|---|---|
| Field resolver 若只返回候选名称，不能作为成功依据 | UE 变量节点成功依赖 `FMemberReference` 可 resolve 到 `FProperty`，且本地变量还需要 graph scope/guid。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:447-580` |
| 若没有 target pin projection，无法覆盖 object-pin/component-pin field access | 编辑器从 pin 菜单投影 target class，变量节点非 self context 会出现 Target pin。 | `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`; `Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:142-207` |
| 若把 component_ref 绑定到 component spawner，会生成错误语义 | component spawner 生成 AddComponent；已有组件引用是 SCS/native component `FObjectProperty` 的 variable get。 | `Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp:78-260`; `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:7684-7747` |
| 若没有 dedicated property path fragment，nested struct/object path 无法稳定 readback | split pin、BreakStruct、SetFieldsInStruct、optional pin guid 和 reconstruction 都需要集中管理。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7201-7557`; `Source/Editor/BlueprintGraph/Private/K2Node_StructOperation.cpp:53-147` |
| 若不记录 linked pin 和 schema response，typed pin 推断会产生假成功 | schema 和 compiler 会因类型、方向、多连接等问题报错。 | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302`; `Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-920` |
| 若 DebugBundle 不收集编译诊断，private/readonly path 会被误报成功 | Get/Set 节点编译阶段专门验证 readable/writable/private/visible 状态。 | `Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp:459-505`; `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:421-456` |
| 若 Review 试图按 per-field target 切分，会与现有 graph_block 级审阅边界冲突 | Field 细节应该进 readback facts/DebugBundle，Review evidence 保持 graph block。 | 任务边界：GraphWrite Field 操作细节进入 DebugBundle/readback facts，不新增 per-field Review target 粒度。 |

---

## 13. 后续实现前必须讨论的架构决策

| 决策 | 影响范围 | 建议 |
|---|---|---|
| `field_access` 是否允许隐式插入临时变量 | object-return 多次复用、exec 可读性、DebugBundle node mapping | 默认不插入；只有多消费者或用户要求时由 FragmentBuilder 插入 temp。 |
| 是否扩展跨 statement 共享上下文 | symbol table、named output、临时 pin inference、批量路径复用 | 默认不扩展；任何跨 statement 共享都必须先讨论 TaskSpec 协议。 |
| `component_ref_set` 是否默认允许 | 组件引用变量多为只读/不应重新赋值；但 UE setter 可见性由 property writable 决定 | 默认只 P2 支持，若 read-only 返回诊断；不作为 P0。 |
| split pin 是否暴露为用户 DSL statement | 可能扩大 API 面，且不代表业务语义 | 不暴露；作为 `get_property/set_property` 的内部实现和 readback 支撑。 |
| by-ref set 第一阶段是否实现 | wildcard 类型推断、split pin recombine、连接顺序风险高 | 第一阶段 diagnostic/deferred；后续单独设计 `set_ref`。 |
| property path 成功证据粒度 | Review/DebugBundle/rollback/readback 都受影响 | graph_block Review 不变；statement readback 记录完整 path nodes/links/pins。 |
| ActionContext cache key | 缓存复用、ActionFilter 结果、同名字段消歧 | 使用 owner class + member name + guid + local scope + target pin type；不得只按 display name。 |

---

## 14. 最终结论

| 结论项 | 实际结论 |
|---|---|
| UE 5.6 普通 Blueprint Field/Variable/Property 编辑器操作总数 | **28**。计数规则：按普通 `EventGraph` / `FunctionGraph` 内可由 Action Menu、变量拖拽、pin-drag、struct pin 菜单或组件选择入口触达的操作计数；包含 3 个 graph-adjacent 但最终排除的非 Field/非 GraphWrite 操作；不包含任务明确排除的 Class Defaults / Details、DataAsset/DataTable、UMG Designer、Animation Blueprint、独立 make/break/function/container 语义。 |
| 建议 BlueprintHelper first-class 覆盖数量 | **17**。纳入：`field.member_get`、`field.member_set`、`field.inherited_member_get`、`field.inherited_member_set`、`field.sparse_data_get`、`field.function_param_get`、`field.local_get`、`field.local_set`、`field.object_pin_member_get`、`field.object_pin_member_set`、`field.component_ref_get`、`field.component_ref_set`、`field.component_property_get`、`field.component_property_set`、`field.struct_member_get`、`field.struct_member_set`、`field.nested_property_path`。 |
| UI-only 入口证据数量 | **4**。`field.drag_get`、`field.drag_set`、`field.pin_drag_get`、`field.pin_drag_set` 不作为 first-class statement；仅作为上游 UI 入口映射证据。原始 **84.0%** 是引擎编辑器入口覆盖率，不是 BlueprintHelper TaskSpec 直接可实现率。 |
| support/readback、其他 cluster、诊断/延期数量 | **7**。`field.split_struct_pin_support`、`field.recombine_struct_pin_support` 只做 support/readback；`control.function_return_write`、`function.selected_component_call`、`component.add_component_node` 归其他 cluster；`field.unsupported_path_diagnostic`、`field.by_ref_set` 归 diagnostic/deferred。 |
| 当前 BlueprintHelper Field 实现与目标覆盖之间的真实缺陷 | 本次未提供 BlueprintHelper Field 源码，不能做插件源文件级缺陷定位。基于 UE 5.6 证据，后续实现必须补齐：`FMemberReference` readback、local scope/guid、owner class/guid、target pin projection、component `FObjectProperty` 解析、dedicated property path fragment、split pin readback、schema connection validation、compile diagnostics 归因。证据见 `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:7585-7610`、`Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:447-580`、`Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:636-750`、`Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2169-2302`、`Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:828-920`。 |
| 后续实现前必须讨论的架构决策 | 需要决定：是否扩展跨 statement 共享上下文、是否隐式插入 temp、`component_ref_set` 默认策略、split pin 是否暴露为 statement、by-ref set 是否延期、property path fragment/readback 粒度、ActionContext cache key、Review 是否保持 graph_block 粒度。影响范围：TaskSpec、ActionContext projection、ActionResolution、FragmentBuilder、readback、DebugBundle、Review。 |

---

## 15. 执行口径

本结果文档只完成只读源码探索、能力清洗和证据导出，不包含任何 BlueprintHelper、AgentFaceService 或 UE 源码修改。后续实现应继续遵守：`get/set/get_property/set_property/component_ref/field_access` 属于 FieldVariableActionCluster；不得通过 FunctionAction、legacy parsed API 或 UI 交互模拟伪装成功；成功判定必须基于 node class、member reference、pin type、link/default、split state 和 compile diagnostics。
