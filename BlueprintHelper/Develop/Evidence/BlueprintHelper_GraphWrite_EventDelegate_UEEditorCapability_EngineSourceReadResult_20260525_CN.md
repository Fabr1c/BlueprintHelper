# BlueprintHelper GraphWrite EventDelegate UE Editor Capability Engine Source Read Result

日期：2026-05-25  
对象：GraphWrite `component_bound_event` 与 `delegate_operation=bind|assign|unbind|call|clear`  
源码基线：上传的 UE 5.6 引擎源码包，只读检索  
输出性质：源码证据表、能力边界、覆盖矩阵与后续实现建议；未修改 BlueprintHelper、AgentFaceService 或 UE 源码

---

## 0. 总结结论

UE 5.6 普通 Blueprint 中，与本任务范围直接对应的 component-bound / dynamic multicast delegate 图内 use-site 操作按 `public_operation` 去重后为 **6 类**：

1. `component_bound_event`
2. `delegate.bind`
3. `delegate.assign`
4. `delegate.unbind`
5. `delegate.call`
6. `delegate.clear`

建议 GraphWrite EventDelegate first-class 覆盖同样为 **6 类**。在排除 custom/override/native handler declaration、timer/async helper、Animation Blueprint、UMG designer、Details 面板资产编辑等非 GraphWrite 职责后，first-class 覆盖比例为 **6 / 6 = 100.0%**。

关键源码判断：

- UE 5.6 ActionDatabase 已为 `UK2Node_AddDelegate`、`UK2Node_AssignDelegate`、`UK2Node_RemoveDelegate`、`UK2Node_CallDelegate`、`UK2Node_ClearDelegate` 注册 `UBlueprintDelegateNodeSpawner`；`assign` 不需要 BlueprintHelper-local manual factory。证据：`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-770`、`Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp:74-118`。
- `UK2Node_AssignDelegate` 自身限制在 EventGraph，并在 `PostPlacedNewNode` 中自动创建与 delegate signature 匹配的 `UK2Node_CustomEvent` 并连接 delegate pin。证据：`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-116`、`Source/Editor/BlueprintGraph/Classes/K2Node_AssignDelegate.h:17-21`。
- `component_bound_event` 是 `UK2Node_ComponentBoundEvent`，从组件上下文或 binding-specific node spawner 进入，最终写入 `FBlueprintComponentDelegateBinding`。重复事件按 `(ComponentPropertyName, DelegatePropertyName)` 检测。证据：`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:67-110`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:113-129`、`Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2622-2640`。
- 标准 delegate target pin 已支持 self、linked typed object pin、component/variable getter 等通用连接模型；GraphWrite 不应在 EventDelegate builder 中为单个 component 名硬编码创建 getter，应复用 Field / component_ref / linked pin evidence。证据：`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:133-159`。
- Handler / signature 声明不是 EventDelegateActionCluster 的职责。`UK2Node_CreateDelegate` 负责 use-site 校验，但 handler 函数、custom event、signature evidence 应由 BlueprintSignature 或 ActionContext projection 提供；缺失时确定性失败。证据：`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:71-199`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:201-207`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:223-294`。

---

## 1. 计数规则与范围

### 1.1 计数规则

本结果按 **GraphWrite public operation class** 计数，不按编辑器入口计数。也就是说：

- “右键空白图”、“从 delegate 变量/组件拖拽”、“从组件面板 Add Event 菜单”、“binding-specific action menu” 若最终创建同一类 use-site 节点，则合并为一个 operation。
- `Create Event` / `UK2Node_CreateDelegate` 是 `bind`、`unbind` 等操作的 handler use-site helper，不单独计入 EventDelegate first-class operation。
- `UK2Node_CustomEvent`、override/native event declaration、handler function declaration 归 BlueprintSignature，不计入 EventDelegate operation。
- Level actor bound event、Animation Blueprint notify/event、UMG designer widget event、Details 面板属性绑定、timer/async helper 不计入本分母。

### 1.2 纳入范围

| operation | 纳入原因 |
| --- | --- |
| `component_bound_event` | 普通 Actor Blueprint 组件 delegate event entry use-site；UE 通过 `UK2Node_ComponentBoundEvent` 与 component dynamic binding 表达。 |
| `delegate.bind` | 标准 Add Delegate；需要 handler delegate pin。 |
| `delegate.assign` | 标准 Assign Delegate；UE 已提供 node class 与 spawner，并自动创建 attached custom event。 |
| `delegate.unbind` | 标准 Remove Delegate；需要指定 handler delegate pin。 |
| `delegate.call` | 标准 Call Delegate；从 delegate signature 展开参数 pin。 |
| `delegate.clear` | 标准 Clear Delegate / Unbind all；无 handler pin。 |

### 1.3 排除范围

| 排除项 | Owner / 原因 | 证据 |
| --- | --- | --- |
| custom event declaration、override event declaration、native event declaration | BlueprintSignature。EventDelegate 只消费 projected handler/signature evidence。 | `Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:390-456`、`Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:458-536` |
| `UK2Node_CreateDelegate` 独立创建与 handler 选择 | BlueprintSignature / ActionContext projection。它是 bind/unbind helper，不是独立 delegate operation。 | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:45-57`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:71-199` |
| timer delegate node、latent/async delegate helper | GenericSchedule 或 FunctionAction。不是 MC delegate property use-site 主线。 | 本次普通 Blueprint delegate property 检索未把 timer/async helper 落到 `UK2Node_BaseMCDelegate` 节点族。 |
| Animation Blueprint notify/event 专属入口 | excluded。不是普通 Blueprint `EventGraph` / `FunctionGraph` 范围。 | 范围排除。 |
| UMG designer widget event 绑定 | excluded / existing non-GraphWrite tool。不是普通 Blueprint 图内 ActionDatabase 主线。 | 范围排除。 |
| Details 面板属性委托绑定或 asset 内容编辑 | existing non-GraphWrite tool。不是 graph body 节点生成职责。 | 范围排除。 |

---

## 2. 任务 1：ActionDatabase 与 Action Menu 委托操作枚举

### 2.1 ActionDatabase 注册证据

`FBlueprintActionDatabase::AddClassPropertyActions` 遍历 class properties。对 `FMulticastDelegateProperty` 的处理是核心注册点：

| UE 条件 / 分支 | 注册节点 | 结论 | 证据 |
| --- | --- | --- | --- |
| `FMulticastDelegateProperty` 且 `CPF_BlueprintAssignable` | `UBlueprintDelegateNodeSpawner::Create(UK2Node_AddDelegate::StaticClass(), DelegateProperty)` | `delegate.bind` 的 ActionDatabase 主路径。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-747` |
| `FMulticastDelegateProperty` 且 `CPF_BlueprintAssignable` | `MakeAssignDelegateNodeSpawner(DelegateProperty)`，返回 `UBlueprintDelegateNodeSpawner::Create(UK2Node_AssignDelegate::StaticClass(), DelegateProperty)` | `delegate.assign` 已经有 UE spawner，不应保留 GraphWrite-local manual factory。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:150-157`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:744-747` |
| `FMulticastDelegateProperty` 且 `CPF_BlueprintCallable` | `UBlueprintDelegateNodeSpawner::Create(UK2Node_CallDelegate::StaticClass(), DelegateProperty)` | `delegate.call` 仅对 callable delegate property 注册。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:750-754` |
| `FMulticastDelegateProperty` 可见 | `UBlueprintDelegateNodeSpawner::Create(UK2Node_RemoveDelegate::StaticClass(), DelegateProperty)` | `delegate.unbind` 的 ActionDatabase 主路径。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:756-758` |
| `FMulticastDelegateProperty` 可见 | `UBlueprintDelegateNodeSpawner::Create(UK2Node_ClearDelegate::StaticClass(), DelegateProperty)` | `delegate.clear` 的 ActionDatabase 主路径。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:758-760` |
| `FMulticastDelegateProperty` 所在类是 `UActorComponent` | `MakeComponentBoundEventSpawner(DelegateProperty)`，返回 `UBlueprintBoundEventNodeSpawner::Create(UK2Node_ComponentBoundEvent::StaticClass(), DelegateProperty)` | `component_bound_event` 的 binding-specific spawner 主路径。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:283-286`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:761-765` |
| `FMulticastDelegateProperty` 所在类是 `AActor` | actor bound event spawner | 非本任务普通 Actor BP component-bound 范围；Level actor/event path 不纳入 GraphWrite EventDelegate first-class。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:766-769` |

属性可见性不是只看 `CPF_BlueprintVisible`。对 delegate，`IsPropertyBlueprintVisible` 还把 `CPF_BlueprintAssignable` 与 `CPF_BlueprintCallable` 纳入可见条件。证据：`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:578-586`。

### 2.2 NodeSpawner 与 binding-specific 过滤

| 机制 | 结论 | 证据 |
| --- | --- | --- |
| `UBlueprintDelegateNodeSpawner::Create` 保存 delegate property 与 node class，spawn 后通过 `SetFromProperty` 设置 `UK2Node_BaseMCDelegate::DelegateReference` | 所有 delegate operation 应优先走同一 UE spawner evidence。 | `Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp:74-88`、`Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp:105-118` |
| `UBlueprintBoundEventNodeSpawner::Create` 保存 `EventDelegate`，菜单文本为 `Add {delegate}` | component-bound event 是 binding-specific spawner，不是普通 free action。 | `Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:70-88` |
| bound node spawner 若没有 binding object，则 `Invoke` 返回 null | component-bound event 需要 binding set / binding object evidence。 | `Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:108-115` |
| ActionFilter 过滤无绑定对象的 binding-specific spawner | 普通图空白上下文无法无上下文创建 component-bound event；必须从组件 / binding object 上下文进入。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:1279-1288`、`Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:2239-2240` |
| ActionFilter 调用 `NodeCDO->IsCompatibleWithGraph` | EventGraph / FunctionGraph 限制必须使用 node class 真实规则，不应由 GraphWrite 猜测。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:1218-1238`、`Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:2228-2229` |

### 2.3 编辑器可见操作表

| editor_entry | public_operation | ue_spawner / api | ue_node_class | graph_context_filter | source_path |
| --- | --- | --- | --- | --- | --- |
| 组件树 / SCS 组件右键 `Add Event` 子菜单；或 binding-specific ActionDatabase entry | `component_bound_event` | `UBlueprintBoundEventNodeSpawner`；组件面板直接调用 `FKismetEditorUtilities::CreateNewBoundEventForComponent` | `UK2Node_ComponentBoundEvent` | 需要 component binding object；delegate owner class 与 component class compatible；EventGraph only；Actor Blueprint component property only；重复事件被阻止/转为 View | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:761-765`、`Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:119-194`、`Source/Editor/Kismet/Private/SSCSEditor.cpp:4388-4420`、`Source/Editor/Kismet/Private/SSCSEditor.cpp:4492-4587` |
| 右键空白图 / delegate property action / delegate 变量拖拽菜单 `Bind` | `delegate.bind` | `UBlueprintDelegateNodeSpawner` | `UK2Node_AddDelegate` | `CPF_BlueprintAssignable`；EventGraph 与 FunctionGraph；target class 过滤；delegate input pin 必须连接 handler delegate | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:744-747`、`Source/Editor/Kismet/Private/BPDelegateDragDropAction.cpp:107-115`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:334-362` |
| 右键空白图 / delegate property action / delegate 变量拖拽菜单 `Assign` | `delegate.assign` | `UBlueprintDelegateNodeSpawner`；`UK2Node_AssignDelegate::PostPlacedNewNode` 自动创建 attached custom event | `UK2Node_AssignDelegate` | `CPF_BlueprintAssignable`；EventGraph only；Blueprint 必须支持 EventGraphs | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:744-747`、`Source/Editor/Kismet/Private/BPDelegateDragDropAction.cpp:133-154`、`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-116` |
| 右键空白图 / delegate property action / delegate 变量拖拽菜单 `Unbind` | `delegate.unbind` | `UBlueprintDelegateNodeSpawner` | `UK2Node_RemoveDelegate` | EventGraph 与 FunctionGraph；delegate input pin 必须连接 handler delegate；常见 UI 在 `CPF_BlueprintAssignable` delegate 变量拖拽菜单下展示 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:756-758`、`Source/Editor/Kismet/Private/BPDelegateDragDropAction.cpp:117-123`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:403-431` |
| 右键空白图 / delegate property action / delegate 变量拖拽菜单 `Call` | `delegate.call` | `UBlueprintDelegateNodeSpawner` | `UK2Node_CallDelegate` | `CPF_BlueprintCallable`；EventGraph 与 FunctionGraph；参数 pin 来自 delegate signature | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:750-754`、`Source/Editor/Kismet/Private/BPDelegateDragDropAction.cpp:95-104`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-490` |
| 右键空白图 / delegate property action / delegate 变量拖拽菜单 `Unbind all` | `delegate.clear` | `UBlueprintDelegateNodeSpawner` | `UK2Node_ClearDelegate` | EventGraph 与 FunctionGraph；无 handler pin；清空目标 object 上该 delegate 的全部绑定 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:758-760`、`Source/Editor/Kismet/Private/BPDelegateDragDropAction.cpp:125-131`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:379-393` |

---

## 3. 任务 2：component_bound_event 组件事件入口能力

### 3.1 组件面板入口与 delegate 枚举

SCS 组件右键菜单在满足编辑条件、选中组件 class 可生成事件时添加 `Add Event` 子菜单。证据：`Source/Editor/Kismet/Private/SSCSEditor.cpp:4388-4420`。

`BuildMenuEventsSection` 对选中组件 class 的 `FMulticastDelegateProperty` 进行枚举，只对 `CPF_BlueprintAssignable` delegate 暴露 Add/View event 项。它对每个选中组件先查找对应 component property，再用 `FindBoundEventForComponent` 检查是否已有事件；已有则展示 View 入口，否则展示 Add 入口。证据：`Source/Editor/Kismet/Private/SSCSEditor.cpp:4492-4559`。

`ConstructEvent` 获取 `Blueprint->SkeletonGeneratedClass` 上的 component property，然后调用 `FKismetEditorUtilities::CreateNewBoundEventForComponent`；没有 property 或已有事件时不创建。证据：`Source/Editor/Kismet/Private/SSCSEditor.cpp:4575-4587`。

### 3.2 创建、初始化、动态绑定

| 步骤 | UE 行为 | GraphWrite 结论 | 证据 |
| --- | --- | --- | --- |
| 事件创建 | `CreateNewBoundEventForComponent` 委托给 `CreateNewBoundEventForClass(Component->GetClass(), EventName, Blueprint, ComponentProperty)` | GraphWrite 需要 component class、delegate property、component property 三类证据，不应只传组件 display name。 | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2554-2559` |
| 找 delegate property | `CreateNewBoundEventForClass` 在 component class 上 `FindFProperty<FMulticastDelegateProperty>` | delegate property source 是 component class，不是 owner Blueprint class。 | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2562-2596` |
| 创建节点 | 目标图是 `Blueprint->GetLastEditedUberGraph()`，spawn `UK2Node_ComponentBoundEvent` | component-bound event 必然落在 EventGraph / Ubergraph。 | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2562-2596` |
| 初始化节点 | `InitializeComponentBoundEventParams` 写入 `ComponentPropertyName`、`DelegatePropertyName`、`DelegateOwnerClass`、`EventReference`、`CustomFunctionName` | readback 必须验证节点上的 component property、delegate property、owner class 与 signature。 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:96-110`、`Source/Editor/BlueprintGraph/Classes/K2Node_ComponentBoundEvent.h:34-44` |
| 注册动态绑定 | `RegisterDynamicBinding` 创建 `FBlueprintComponentDelegateBinding`，填入 component property、delegate property、function name | 成功判定不能只看节点存在；还需确认 dynamic binding facts。 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:113-129` |
| delegate 有效性 | `IsDelegateValid` 要求 BP generated class 上存在 object component property，delegate owner class 中存在或可 remap 目标 delegate property | Instance-only component 不满足稳定 component property path 时应 excluded 或 diagnostic-only。 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:165-174`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:211-214` |

### 3.3 重复事件规则

| 场景 | UE 行为 | 证据 |
| --- | --- | --- |
| 从 spawner 查找已有 node | `UBlueprintBoundEventNodeSpawner::FindPreExistingEvent` 对 component binding 调用 `FindBoundEventForComponent(Blueprint, EventDelegate->GetFName(), BoundObject.GetFName())` | `Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:119-140` |
| 从 Kismet utilities 查找已有 node | 扫描所有 `UK2Node_ComponentBoundEvent`，匹配 `ComponentPropertyName` 与 `DelegatePropertyName` | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2622-2640` |
| 粘贴 / 重命名 | `CanPasteHere` 若已有同 component/delegate event 则不允许；Rename 时若产生重复组件事件则报错 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:67-80`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:131-154` |

GraphWrite 建议：

- 默认策略应是 deterministic fail 或 return existing reference；不要 silent create duplicate。
- 后续若需要 replace/merge 策略，应作为独立架构决策进入 Safety/Review，而不是 EventDelegate builder 默默处理。

### 3.4 组件种类覆盖表

| component_kind | delegate_property_source | preexisting_event_rule | binding_evidence | graphwrite_scope | source_path |
| --- | --- | --- | --- | --- | --- |
| SCS component | component class 上的 `FMulticastDelegateProperty`；component property 是 SCS 变量生成的 `FObjectProperty` | `(ComponentPropertyName, DelegatePropertyName)` 唯一；已有则 View / fail | `blueprint_path`、`component_variable_name`、`component_property_path`、`component_class_path`、`delegate_property_path` | first-class | `Source/Editor/Kismet/Private/SSCSEditor.cpp:1055-1081`、`Source/Runtime/Engine/Classes/Engine/SCS_Node.h:149-156`、`Source/Runtime/Engine/Classes/Engine/SCS_Node.h:205-212`、`Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2622-2640` |
| inherited SCS component | inherited SCS node / instanced inherited component resolves to a variable name / component template | 同上 | `inherited_component_path`、`component_variable_name`、`component_property_path`、`declaring_blueprint_or_class` | first-class if component property resolves | `Source/Editor/Kismet/Public/SSCSEditor.h:530-552`、`Source/Editor/Kismet/Public/SSCSEditor.h:594-623`、`Source/Editor/Kismet/Private/SSCSEditor.cpp:1055-1081` |
| native inherited component | editable native component property exists；component node can represent native/inherited component | 同上 | native `FObjectProperty` path、component class、delegate property path | first-class if property is visible/resolvable; otherwise diagnostic-only | `Source/Editor/Kismet/Private/SSCSEditor.cpp:1440-1446`、`Source/Editor/Kismet/Public/SSCSEditor.h:594-623` |
| component template | template can be used to recover variable name via `FComponentEditorUtils::FindVariableNameGivenComponentInstance` | 同上 | template object path + resolved variable/property path | first-class only after stable component property projection | `Source/Editor/Kismet/Private/SSCSEditor.cpp:1055-1081` |
| construction-script-created / actor instance component | source may redirect to SCS template if variable name matches; otherwise unstable | No stable BP component property for instance-only component | instance object path is insufficient for serialized component-bound event | diagnostic-only / excluded | `Source/Editor/Kismet/Private/SSCSEditor.cpp:4840-4860`、`Source/Editor/Kismet/Public/SSCSEditor.h:559-577`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:165-174` |

---

## 4. 任务 3：delegate bind / assign / unbind / call / clear 节点族

### 4.1 BaseMCDelegate 共同 pin contract

`UK2Node_BaseMCDelegate` 保存 `DelegateReference`，节点可解析 `FMulticastDelegateProperty`，支持 `GetDelegateSignature()` 与 `GetDelegatePin()`。证据：`Source/Editor/BlueprintGraph/Classes/K2Node_BaseMCDelegate.h:20-69`。

`AllocateDefaultPins` 共同创建：

- exec 输入/输出。
- `Target` self/object pin，类型是 delegate owner class；friendly name 为 `Target`。

证据：`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`。

`UK2Node_BaseMCDelegate::IsCompatibleWithGraph` 允许 `GT_Ubergraph` 与 `GT_Function`，然后再走 Super 限制。证据：`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`。

### 4.2 operation 逐项结论

| operation | UE node class | spawner | pin contract | compile handler / validation | GraphWrite 实现建议 | source_path |
| --- | --- | --- | --- | --- | --- | --- |
| `bind` | `UK2Node_AddDelegate` | `UBlueprintDelegateNodeSpawner` | Base pins + delegate input pin，`PC_Delegate`，`bIsConst=true`，`bIsReference=true`，signature 填到 `PinSubCategoryMemberReference` | `FKCHandler_AddRemoveDelegate(KCST_AddMulticastDelegate)`；delegate input 未连接时报错 | first-class；需要 handler evidence，通常创建/连接 `UK2Node_CreateDelegate` 或使用 existing event/custom event delegate output | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:334-362`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:165-185`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:187-221` |
| `assign` | `UK2Node_AssignDelegate` | `UBlueprintDelegateNodeSpawner`，ActionDatabase 已注册 | 语义上是 AddDelegate + attached custom event | EventGraph only；`PostPlacedNewNode` 创建 unique custom event 并连接 delegate output 到 delegate input | first-class，但必须决策：是否允许 UE 自动 custom event，还是仅在 BlueprintSignature 已投影 handler 时使用；无论如何不保留 manual assign factory | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280`、`Source/Editor/BlueprintGraph/Classes/K2Node_AssignDelegate.h:17-21`、`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-116` |
| `unbind` | `UK2Node_RemoveDelegate` | `UBlueprintDelegateNodeSpawner` | Base pins + delegate input pin，和 AddDelegate 同样需要 handler delegate | `FKCHandler_AddRemoveDelegate(KCST_RemoveMulticastDelegate)`；delegate input 未连接时报错 | first-class；需要与 bind 同一 handler function/object evidence；未发现普通编辑器存在“按 object 清除单个 function”之外的单独 GraphWrite 主线 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:403-431`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:165-221` |
| `call` | `UK2Node_CallDelegate` | `UBlueprintDelegateNodeSpawner` | Base pins + signature 参数输入 pin；非 out 参数或 ref 参数成为 input pin | `ValidateNodeDuringCompilation` 要求 `CPF_BlueprintCallable`；编译为 `KCST_CallDelegate`；signature metadata 有额外限制 | first-class；TaskSpec 需提供 signature 参数默认值/links；readback 验证 pin 类型与 link/default | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-490`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:373-432`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:444-482` |
| `clear` | `UK2Node_ClearDelegate` | `UBlueprintDelegateNodeSpawner` | Base pins only；无 handler delegate pin | `FKCHandler_ClearDelegate` 生成 `KCST_ClearMulticastDelegate`，对 self pin link 或 unlinked self 均可 | first-class；语义应命名为 clear all / unbind all；不要伪造 handler | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:379-393`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:334-368` |

### 4.3 `assign` manual factory 结论

UE 5.6 源码直接否定 “assign 需要 BlueprintHelper 手动 new node” 的必要性：

- ActionDatabase 通过 `MakeAssignDelegateNodeSpawner` 返回 `UBlueprintDelegateNodeSpawner::Create(UK2Node_AssignDelegate::StaticClass(), DelegateProperty)`。证据：`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280`。
- `UK2Node_AssignDelegate` 的类注释说明它就是带 attached custom event 的 AddDelegate。证据：`Source/Editor/BlueprintGraph/Classes/K2Node_AssignDelegate.h:17-21`。
- 节点放置后自动创建 custom event 并连接 delegate output。证据：`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:88-116`。

因此，BlueprintHelper 后续实现应把 `assign` 收敛到 UE spawner/action evidence，并把是否允许自动创建 handler declaration 作为 BlueprintSignature/EventDelegate 边界决策，而不是由 FragmentBuilder 手动 `NewObject<UK2Node_AssignDelegate>`。

---

## 5. 任务 4：handler / signature / CreateDelegate 证据链

### 5.1 `UK2Node_CreateDelegate` 校验规则

`UK2Node_CreateDelegate` 创建：

- input object pin：`PN_Self` / `Object`。
- output delegate pin：`PC_Delegate` / `OutputDelegate`，friendly name `Event`。

证据：`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:45-57`。

`IsValid` 的核心校验：

| 校验点 | 规则 | 失败诊断 / 影响 | 证据 |
| --- | --- | --- | --- |
| handler function name | `SelectedFunctionName` 不能是 `NAME_None` | invalid；compile handler 也会报 missing function/event name | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:71-81`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:228-233` |
| delegate output pin | 必须存在 | invalid | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:83-91` |
| delegate signature | 通常从 output delegate pin 的 linked delegate pin 推导；不能缺失 | compile 报 unable to determine expected signature / No delegate signature | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:93-126`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:235-240`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:267-290` |
| object scope class | 从 object pin 解析 scope class；未连接时 self；连接时使用 linked object pin class | invalid if unresolved | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:128-147`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:357-405` |
| handler function resolution | 在 scope class 上按 name/guid 找 function | invalid if missing | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:149-161` |
| signature compatibility | delegate signature 与 found function 必须 `IsSignatureCompatibleWith` | compile 后 `Signature Error` | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:162-170`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:201-207` |
| function can be used in delegate | 通过 `UEdGraphSchema_K2::FunctionCanBeUsedInDelegate` | invalid if function type不允许 | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:171-178` |
| authority-only | 若 delegate authority-only，handler function 也必须是 `FUNC_BlueprintAuthorityOnly` | invalid | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:180-196` |

### 5.2 Handler source 边界表

| handler_source | UE 表达 | signature_match_rule | signature_evidence 是否足够 | owner_boundary | failure_diagnostic | source_path |
| --- | --- | --- | --- | --- | --- | --- |
| custom event | `UK2Node_CustomEvent` 可从 function signature 创建 pins，delegate output 可连接 Add/Remove/Assign delegate pin | 参数按 signature function 复制；非 out 或 ref 参数成为输出 pin；可根据 linked delegate signature reconstruct | `delegate_signature_function_path` + `handler_function_path/custom_event_name` + `signature_evidence_id` 足够；但声明创建属 BlueprintSignature | BlueprintSignature 创建/确认 custom event；EventDelegate 只连接 use-site | 缺 signature 或签名不兼容由 `CreateDelegate` / compiler 报错 | `Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:425-456`、`Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:458-536` |
| existing function | `UK2Node_CreateDelegate::SetFunction` 指向已有 function | delegate signature 与 function signature 必须 compatible；function 必须允许用作 delegate | 足够，前提是 function owner/scope class projection 明确 | BlueprintSignature / ActionContext 查找；EventDelegate 只消费 resolved function | `Signature Error`、missing function/event name、scope unresolved | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:149-178`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:201-207`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:505-508` |
| existing event entry | `UK2Node_Event` 有 delegate output pin；事件本身创建 pins from signature | EventReference 指向 signature function；delegate output 可以被 compiler 展开为 `UK2Node_CreateDelegate` | 若已有 event 节点 readback 能提供 event delegate pin + function name，则可作为 use-site evidence | Event entry declaration 属 BlueprintSignature；EventDelegate 不创建 override/native event | duplicate/override/missing event function 由 event validation 报错 | `Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:345-368`、`Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:370-392`、`Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:717-741` |
| `Create Event` node | `UK2Node_CreateDelegate`，object pin + function name + output delegate pin | 见上；signature 从连接的 Add/Remove delegate pin 推导 | 足够；需要 `handler_function_path`、`object_scope_class`、`delegate_signature` | EventDelegate 可创建 use-site `CreateDelegate`，但不能创建 handler declaration | 缺 function name、缺 signature、object scope 错误 | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:45-57`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:331-405`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:223-294` |

### 5.3 Owner boundary 判断

结论：任务文件要求的职责拆分成立。

- EventDelegateActionCluster 应只负责 delegate/component-bound use-site：Add/Assign/Remove/Call/Clear/ComponentBoundEvent 节点、target pin、delegate property、handler delegate pin 的连接。
- Handler declaration、handler function path、handler signature、custom event declaration/override/native event declaration 应由 BlueprintSignature 或 ActionContext projection 提供。
- 缺 handler/signature evidence 时，EventDelegate 不应扫描图内 custom events 猜测，也不应创建新 handler declaration，除非后续架构明确允许 `assign` 使用 UE 自动 attached custom event 且把这条行为登记为 BlueprintSignature 侧 effect。

---

## 6. 任务 5：binding object 与 target pin 通用化

### 6.1 UE target pin 与编译模型

BaseMCDelegate 创建的 target pin 是 self/object pin，类型为 delegate owner class。证据：`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`。

当从 typed object output pin 拖拽创建 delegate node 时，`UK2Node_BaseMCDelegate::AutowireNewNode` 会：

1. 判断 FromPin 的 object class 是否是 delegate owner class 或其子类。
2. 调整 target pin 类型。
3. 调用 `TryCreateConnection(FromPin, TargetPin)`。
4. 将 delegate reference 设置为非 self context，并显示 target pin。

证据：`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300`。

编译阶段，`FKCHandlerDelegateHelper::RegisterMultipleSelfAndMCDelegateProperty` 对 target/self pin 的处理是：

- target pin 未连接：使用 self pin 作为 delegate owner。
- target pin 已连接：对每个 linked target pin 创建 delegate inner term。

证据：`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:133-159`。

### 6.2 Variable / component getter 的复用证据

非 delegate property 会注册 variable get/set spawner；variable get 节点通过 `SetFromProperty` 记录 property reference。证据：`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:771-775`、`Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:94-109`、`Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:87-91`。

variable node 也支持从 typed object output pin autowire target/self pin，逻辑与 delegate node 相似。证据：`Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:976-1025`。

### 6.3 Binding source 表

| binding source | UE 表达 | GraphWrite 建议 | 必要 evidence | source_path |
| --- | --- | --- | --- | --- |
| self delegate property | target pin 未连接或 hidden self；delegate reference 是 self context | EventDelegate 直接使用 `UBlueprintDelegateNodeSpawner`，不创建 getter | `delegate_property_path`、`owner_class_path`、`self_context=true` | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:141-149` |
| component delegate property | component getter / binding object output 连接 target pin；或 component-bound event 使用 `FObjectProperty` binding | 复用 Field `component_ref` evidence；不要在 EventDelegate builder 内按 component name 手动 `UK2Node_VariableGet` | `component_property_path`、`component_class_path`、`delegate_property_path`、可选 `component_getter_node_ref` | `Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:144-170`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300` |
| variable object delegate | `UK2Node_VariableGet` 输出 object 连接 target pin | 复用 Field get evidence；EventDelegate 只消费 linked output pin | `variable_property_path`、`getter_node_ref/output_pin_ref`、`target_class_path` | `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:94-109`、`Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:976-1025` |
| linked typed object pin | 上游 object output pin 直接连接 target pin | 需要 ActionContext linked pin evidence；不要额外创建 getter | `upstream_node_ref`、`output_pin_ref`、`pin_type_class_path`、`delegate_owner_class_path` | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300` |
| function return object | function call result pin 连接 target pin | 由 FunctionAction/Field composition 创建上游 function call；EventDelegate 只连接 target | `function_call_node_ref`、`return_pin_ref`、`return_class_path` | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:152-157` |

结论：GraphWrite EventDelegate builder 不应内置“如果是 component 名就手动 new `UK2Node_VariableGet`”的特殊逻辑；正确做法是 ActionContext 先把 binding object 投影为 self/component_ref/variable_get/linked_pin/function_return 等统一 evidence，EventDelegate 只执行目标 pin 连接与 readback 验证。

---

## 7. 任务 6：Graph 类型、Blueprint 类型与编辑器限制

### 7.1 图类型来源

`UEdGraphSchema_K2::GetGraphType`：

- Blueprint `UbergraphPages` 返回 `GT_Ubergraph`。
- Blueprint `FunctionGraphs` 返回 `GT_Function`。
- Macro library / macro graph 返回 `GT_Macro`。

证据：`Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:3261-3294`。

### 7.2 Blueprint 类型来源

`FBlueprintEditorUtils::DoesSupportEventGraphs` 只对 `BPTYPE_Normal` 与 `BPTYPE_LevelScript` 返回 true。证据：`Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3426-3430`。

`FBlueprintEditorUtils::DoesSupportComponents` 要求存在 SCS、Actor based、且不是 MacroLibrary / FunctionLibrary。证据：`Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3441-3447`。

### 7.3 Operation 限制表

| operation | EventGraph | FunctionGraph | Actor Blueprint | UObject Blueprint | 限制来源 |
| --- | --- | --- | --- | --- | --- |
| `component_bound_event` | 是。`UK2Node_ComponentBoundEvent` 继承 event node；事件节点只兼容 `GT_Ubergraph`。创建工具也使用 `GetLastEditedUberGraph`。 | 否。`UK2Node_Event::IsCompatibleWithGraph` 只允许 `GT_Ubergraph`。 | 是，但必须有 resolvable component `FObjectProperty` 与 `CPF_BlueprintAssignable` component delegate。 | 否，普通 UObject Blueprint 不支持组件树/SCS component property；component-bound event 需要 component property。 | `Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:514-523`、`Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2562-2596`、`Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3441-3447` |
| `delegate.bind` | 是。BaseMCDelegate 兼容 `GT_Ubergraph`。 | 是。BaseMCDelegate 兼容 `GT_Function`。 | 是，若 delegate property 可见且目标 class 过滤通过。 | 是，若 `BPTYPE_Normal` 且 delegate property 可见/目标 class 过滤通过；不依赖 SCS。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-770` |
| `delegate.assign` | 是，且必须在 EventGraph；Blueprint 支持 EventGraphs。 | 否。`UK2Node_AssignDelegate::IsCompatibleWithGraph` 要求 `GT_Ubergraph`。 | 是，若 normal Blueprint/EventGraph 且 delegate property `BlueprintAssignable`。 | 是，若 normal UObject Blueprint 支持 EventGraphs 且 delegate property 可见；不依赖组件。 | `Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-85`、`Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3426-3430` |
| `delegate.unbind` | 是。 | 是。 | 是，若 delegate property 可见/目标 class 过滤通过。 | 是，若 delegate property 可见/目标 class 过滤通过。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:403-431` |
| `delegate.call` | 是。 | 是。 | 是，若 `CPF_BlueprintCallable` 且 delegate signature 允许 call。 | 是，若 `CPF_BlueprintCallable` 且 delegate property 可见/目标 class 过滤通过。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-490` |
| `delegate.clear` | 是。 | 是。 | 是，若 delegate property 可见/目标 class 过滤通过。 | 是，若 delegate property 可见/目标 class 过滤通过。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:379-393` |

---

## 8. 任务 7：readback、compile diagnostics 与成功判定

### 8.1 成功判定标准

| evidence | 通过标准 | 必要 source_path |
| --- | --- | --- |
| node class | 生成节点类与 operation 对应：`UK2Node_ComponentBoundEvent` / `UK2Node_AddDelegate` / `UK2Node_AssignDelegate` / `UK2Node_RemoveDelegate` / `UK2Node_CallDelegate` / `UK2Node_ClearDelegate`。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-770` |
| delegate property | 对 `UK2Node_BaseMCDelegate`：`GetProperty()` 解析到预期 `FMulticastDelegateProperty`；对 component-bound event：`GetTargetDelegateProperty()` 解析到预期 property。 | `Source/Editor/BlueprintGraph/Classes/K2Node_BaseMCDelegate.h:51-58`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:211-214` |
| binding object | self target 未连接时按 self；object target 已连接时 linked pin 类型必须兼容 delegate owner class；component-bound event 必须有 component `FObjectProperty`。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:133-159` |
| create delegate | `bind` / `unbind` 的 delegate input pin 必须连接 `UK2Node_CreateDelegate` output 或 event/custom event delegate output；`UK2Node_CreateDelegate::GetFunctionName()` 指向 projected handler function；object pin scope 正确。 | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:407-420`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:505-508`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:165-185` |
| assign handler | 若使用 UE `UK2Node_AssignDelegate` 默认行为，readback 要确认 attached custom event output 已连接 delegate input；若项目策略禁止 EventDelegate 创建 handler declaration，则 assign 必须消费 BlueprintSignature 提供的 handler/use-site evidence。 | `Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:88-116` |
| call inputs | `UK2Node_CallDelegate` 参数 pin 与 delegate signature 的 non-out/ref 参数一致；默认值与 links 符合 TaskSpec。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-468` |
| dynamic binding | `component_bound_event` 编译注册时写入 `FBlueprintComponentDelegateBinding`，其中 component property、delegate property、function name 匹配预期。 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:113-129` |
| compile result | Blueprint 编译无与该 statement 相关的 error；warning 不直接阻断，但进入 DebugBundle。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:45-101`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:201-207`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:156-163` |

### 8.2 关键 compile diagnostics 来源

| 来源 | 诊断行为 | source_path |
| --- | --- | --- |
| Delegate property lookup | target pin 无 scope 报 `Event Dispatcher has no property`；找不到 delegate property 报 `Could not find an event-dispatcher...`；signature 不匹配报 `Wrong Event Dispatcher. Refresh node`。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:45-101` |
| Add/Remove delegate | delegate input pin 未连接报 `Event Dispatcher pin is not connected`。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:165-185` |
| CreateDelegate | 缺 function name、缺 signature、output delegate 未连接均报错；object pin未连接使用 self literal。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:223-294` |
| Clear delegate | 无 handler；按 target/self pin 生成 `KCST_ClearMulticastDelegate`。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:334-368` |
| Call delegate | 缺 signature 报 `Cannot find signature function`；signature metadata 包含 `DefaultToSelf`、`WorldContext`、`AutoCreateRefTerm` 报错；owner 非 authoritative class 报 warning。 | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:373-432`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:444-482` |
| Component-bound event | 缺 component/delegate property 时编译 warning；dynamic binding 写入 runtime binding object。 | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:113-129`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:156-163` |

---

## 9. 任务 8：80% 编辑器能力覆盖矩阵

> 说明：`coverage_priority=P0` 是本任务建议进入 GraphWrite EventDelegate first-class 的 operation。`P1/P2/excluded` 是为了完整描述 80% 编辑器能力边界与证据需求，并不表示单独新增 public operation。

| capability_id | editor_operation | graph_scope | blueprint_scope | ue_node_class | spawner_or_api | minimum_task_spec_context | action_context_projection | readback_evidence | coverage_priority | owner_cluster | reason | source_path |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `event_delegate.component_bound_event` | Add Event for component delegate | EventGraph | Actor Blueprint with SCS/native/inherited component property | `UK2Node_ComponentBoundEvent` | `UBlueprintBoundEventNodeSpawner` / `FKismetEditorUtilities::CreateNewBoundEventForComponent` | component ref、delegate property、target EventGraph、duplicate policy | component `FObjectProperty`、component class、delegate owner class、binding set | node class、component property、delegate property、dynamic binding、compile warnings | P0 | EventDelegateAction | 普通 Actor BP 组件事件入口主线。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:761-765`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:96-129` |
| `event_delegate.delegate_bind` | Bind Event to delegate | EventGraph / FunctionGraph | Actor Blueprint / UObject Blueprint if delegate property visible | `UK2Node_AddDelegate` + handler delegate source | `UBlueprintDelegateNodeSpawner` | delegate property、target object、handler evidence、exec placement | delegate property spawner、target pin、handler signature | node class、delegate property、target pin、delegate input link、CreateDelegate handler、compile result | P0 | EventDelegateAction | Dynamic multicast bind 主线。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:744-747`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:334-362` |
| `event_delegate.delegate_assign` | Assign delegate | EventGraph only | Normal BP with EventGraphs; Actor / UObject if delegate property visible | `UK2Node_AssignDelegate` + attached custom event | `UBlueprintDelegateNodeSpawner` | delegate property、target object、handler policy、EventGraph | delegate spawner、signature evidence、optional BlueprintSignature custom event | node class、delegate property、attached event or projected handler link、compile result | P0 | EventDelegateAction + BlueprintSignature boundary | UE 已有 spawner；manual factory 应移除。是否允许自动 custom event 是架构决策。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280`、`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-116` |
| `event_delegate.delegate_unbind` | Unbind event from delegate | EventGraph / FunctionGraph | Actor Blueprint / UObject Blueprint if delegate property visible | `UK2Node_RemoveDelegate` + handler delegate source | `UBlueprintDelegateNodeSpawner` | delegate property、target object、handler evidence、exec placement | delegate spawner、target pin、handler signature | node class、delegate property、target pin、delegate input link、CreateDelegate handler、compile result | P0 | EventDelegateAction | 单 handler 解绑主线。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:403-431`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:165-221` |
| `event_delegate.delegate_call` | Call delegate | EventGraph / FunctionGraph | Actor Blueprint / UObject Blueprint if delegate property `BlueprintCallable` | `UK2Node_CallDelegate` | `UBlueprintDelegateNodeSpawner` | delegate property、target object、call arg links/defaults | delegate signature、target pin、arg pin type/default evidence | node class、delegate property、target pin、signature pins、arg values/links、compile result | P0 | EventDelegateAction | Callable delegate 调用主线。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:750-754`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-490` |
| `event_delegate.delegate_clear` | Unbind all / Clear delegate | EventGraph / FunctionGraph | Actor Blueprint / UObject Blueprint if delegate property visible | `UK2Node_ClearDelegate` | `UBlueprintDelegateNodeSpawner` | delegate property、target object、exec placement | delegate spawner、target pin | node class、delegate property、target pin、no handler pin、compile result | P0 | EventDelegateAction | clear all 是独立 UE node；无 handler。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:379-393`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:334-368` |
| `event_delegate.binding_object_component_ref` | component target for delegate node | EventGraph / FunctionGraph | Actor Blueprint with component property | `UK2Node_VariableGet` or existing component ref pin feeding BaseMC target | Field / component spawner + target pin connection | component ref id、delegate property、target operation | component property path、getter output pin、delegate owner class | target pin link resolves to component object; compile no target error | P1 | EventDelegateAction + Field | 80% 常见组件 delegate 需要；但 getter 创建应由 Field/component_ref 提供。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:133-159` |
| `event_delegate.binding_object_variable_ref` | variable object target for delegate node | EventGraph / FunctionGraph | Actor / UObject | `UK2Node_VariableGet` feeding target pin | Field variable get + target connection | variable ref id、delegate property | variable property path、getter output pin | target pin link + delegate property resolution on target class | P1 | EventDelegateAction + Field | 避免 EventDelegate builder 硬编码 getter。 | `Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:94-109`、`Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp:976-1025` |
| `event_delegate.binding_object_linked_pin` | typed object pin target | EventGraph / FunctionGraph | Any BP where upstream pin type compatible | Existing upstream node output pin | ActionContext linked pin | target pin ref、delegate property | pin type class、owner class compatibility | target pin link to expected upstream pin | P1 | EventDelegateAction + ActionContext | 从 pin 拖拽是编辑器主线，GraphWrite 应支持。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300` |
| `event_delegate.handler_signature_validation` | Create Event / CreateDelegate handler use-site validation | EventGraph / FunctionGraph, depending on operation | Actor / UObject | `UK2Node_CreateDelegate` | CreateDelegate node or event/custom event delegate output | handler function path、delegate signature、object scope | BlueprintSignature evidence、scope class、signature function | `GetFunctionName`、object pin、delegate out link、signature compatibility | P1 | BlueprintSignature + EventDelegateAction | 没有这个证据，bind/unbind 不能可靠通过 compile。 | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:71-199`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:223-294` |
| `event_delegate.call_pin_readback` | call delegate args | EventGraph / FunctionGraph | Actor / UObject | `UK2Node_CallDelegate` | Delegate spawner + signature pin expansion | argument values/links | signature function、pin map | pin type/default/link exact match | P1 | EventDelegateAction | call 成功不能只看节点存在；必须读回参数。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:441-468` |
| `event_delegate.binding_object_function_return_target` | function return object target | EventGraph / FunctionGraph | Any compatible BP | upstream function call return pin feeding target | FunctionAction + target pin connection | function call spec + delegate operation spec | function return pin evidence | target pin link | P2 | FunctionAction + EventDelegateAction | 通用化重要，但应由 FunctionAction/Field composition 创建上游 pin。 | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300` |
| `event_delegate.complex_owner_disambiguation` | target class / owner class ambiguity resolution | EventGraph / FunctionGraph | Actor / UObject | BaseMCDelegate target pin | ActionContext + ActionFilter equivalent | explicit owner class and delegate path | target class hierarchy, property owner | resolver candidate trace | P2 | EventDelegateAction | 多 class/继承同名 delegate 时需要更强消歧。 | `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:1044-1078`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:45-101` |
| `event_delegate.preexisting_bound_event_strategy` | duplicate component event handling | EventGraph | Actor BP components | `UK2Node_ComponentBoundEvent` | Kismet utilities / bound event spawner | duplicate policy | existing event node evidence | fail/existing/replacement result | P2 | EventDelegateAction + Safety/Review | replacement/merge 风险高，应先讨论。 | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2622-2640`、`Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:67-80` |
| `blueprint_signature.custom_event_declaration` | Create Custom Event / handler declaration | EventGraph | Normal BP | `UK2Node_CustomEvent` | Custom event spawner | event name、signature | function signature / event declaration evidence | declaration exists, pins match signature | excluded | BlueprintSignature | EventDelegate 不创建 handler declaration。 | `Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:390-536` |
| `blueprint_signature.override_native_event_declaration` | override/native event entry | EventGraph | Depends on class | `UK2Node_Event` variants | Event action spawners | event signature | override/native event metadata | event node validation | excluded | BlueprintSignature | 声明入口不是 delegate use-site。 | `Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:345-392`、`Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:514-523` |
| `generic_schedule.timer_delegate` | timer delegate helper | EventGraph / FunctionGraph | Actor / UObject | timer/function helper nodes | FunctionAction / GenericSchedule | timer spec | function/delegate evidence | helper node compile result | excluded | GenericSchedule | 不混入 EventDelegateActionCluster。 | 范围排除 |
| `function_action.async_delegate_helper` | async/latent delegate callback helper | Varies | Varies | async action nodes | FunctionAction | async call spec | callback pin evidence | node links/compile result | excluded | FunctionAction | 不是 delegate property node family。 | 范围排除 |
| `excluded.animation_blueprint_events` | AnimBP notify/event | Anim graphs/EventGraph variants | Animation Blueprint | specialized nodes | specialized editor | anim context | anim asset evidence | anim compile result | excluded | excluded | 非普通 Blueprint 范围。 | 范围排除 |
| `excluded.umg_designer_events` | UMG designer widget event binding | Widget Blueprint designer/EventGraph | Widget Blueprint | UMG specialized paths | UMG editor | widget tree event | widget field evidence | widget event link | excluded | excluded / existing non-GraphWrite tool | 非普通 BP action menu 主线。 | 范围排除 |
| `excluded.details_panel_delegate_binding` | Details panel property event binding | editor details UI | asset/editor specific | may create graph event | existing editor UI | asset detail selection | property editor context | UI side effects | excluded | existing non-GraphWrite tool | 不是 GraphWrite graph body 节点生成职责。 | 范围排除 |

---

## 10. 实现边界建议

### 10.1 TaskSpec

TaskSpec 应显式表达以下最小上下文：

| operation | TaskSpec 必填 | TaskSpec 可选 | 禁止/不应由 EventDelegate 猜测 |
| --- | --- | --- | --- |
| `component_bound_event` | `blueprint_ref`、`graph_ref` or EventGraph intent、`component_ref`、`delegate_property_path`、duplicate policy | placement、desired event name hint | 根据 display name 猜 component property；扫描 custom event 补 handler |
| `delegate.bind` | `delegate_property_path`、`target_object_ref`、`handler_function_path` or `handler_evidence_id`、exec placement | create_delegate object override、node placement | 自动创建 handler declaration；从图内同名 custom event 猜测 |
| `delegate.assign` | `delegate_property_path`、`target_object_ref`、handler policy、EventGraph target | allow UE auto attached custom event / require projected handler | manual `NewObject<UK2Node_AssignDelegate>` bypassing spawner evidence |
| `delegate.unbind` | `delegate_property_path`、`target_object_ref`、`handler_function_path` or `handler_evidence_id`、exec placement | create_delegate object override | 把 clear all 误作 unbind single handler |
| `delegate.call` | `delegate_property_path`、`target_object_ref`、argument map、exec placement | default values / linked pin refs | 用 signature 之外的 pin name 猜测 |
| `delegate.clear` | `delegate_property_path`、`target_object_ref`、exec placement | none | 构造 handler pin |

### 10.2 ActionContext projection

ActionContext 应负责投影：

1. 当前 Blueprint、graph、graph type、schema、supports EventGraphs/components。
2. delegate property candidate：owner class、property path、flags、signature function。
3. target object evidence：self、component_ref、variable_get、linked typed pin、function return pin。
4. handler/signature evidence：handler function path、scope class、signature function path、custom event declaration ref。
5. component-bound evidence：component property path、component class、SCS/native/inherited source、existing bound event result。
6. callable/assignable flags 与 graph compatibility filter result。

### 10.3 ActionResolution

ActionResolution 应：

- 对 delegate node family 统一走 `UBlueprintDelegateNodeSpawner` evidence。
- 对 `assign` 优先使用 UE ActionDatabase/NodeSpawner，不再返回 BlueprintHelper-local manual assign factory。
- 对 component-bound event 使用 `UBlueprintBoundEventNodeSpawner` 或与 `FKismetEditorUtilities::CreateNewBoundEventForComponent` 等价的 evidence；必须要求 binding object。
- 把 graph compatibility 作为 resolver 阶段 hard constraint：`assign` / `component_bound_event` 不进入 FunctionGraph。
- 把 duplicate component-bound event 作为 resolver/readback result：found existing、blocked、replace requested 三态。

### 10.4 FragmentBuilder

FragmentBuilder 应：

- 只负责节点实例化、pin 连接、exec 链接、默认值写入。
- 不创建 handler declaration，除非架构明确将 `assign` 自动 custom event 作为允许行为并同步 BlueprintSignature/Review side effect。
- 不手动创建 component getter；当 target 是 component/variable/function return，消费上游 Field/FunctionAction 提供的 output pin evidence。
- 对 bind/unbind，若 handler evidence 指示需要 `UK2Node_CreateDelegate`，创建 CreateDelegate use-site 节点并设置 function name/object pin；若 handler evidence 已提供 event/custom event delegate output pin，则直接连接。
- 对 call，从 delegate signature map 构造参数 pin default/link，不接受未识别参数。

### 10.5 Readback

Readback facts 应包含：

| fact | 内容 |
| --- | --- |
| `node_class` | actual class、expected class、node guid/ref |
| `delegate_property` | property name、owner class、flags、signature function |
| `binding_object` | self/component/variable/linked pin/function return，target pin link facts |
| `handler` | CreateDelegate node ref、function name、object pin、output delegate pin；或 event/custom event delegate output ref |
| `call_args` | expected signature pin map、actual pin default/link |
| `component_dynamic_binding` | component property、delegate property、function name |
| `compile_diagnostics` | related errors/warnings keyed by node guid/statement id |
| `debug_bundle` | resolver candidate trace、ActionDatabase/spawner source、graph filter decision |

### 10.6 Review / DebugBundle

Review target 粒度保持 graph_block 级别，不新增 per-delegate Review target。delegate operation 细节进入 DebugBundle/readback facts。DebugBundle 至少应记录：

- selected UE spawner class。
- selected UE node class。
- delegate property path 与 signature function path。
- target binding object projection source。
- handler/signature evidence id。
- graph compatibility result。
- duplicate component-bound event handling result。
- compile diagnostic correlation。

---

## 11. 风险清单

| risk_id | 风险 | 源码依据 | 影响 | 建议 |
| --- | --- | --- | --- | --- |
| R1 | `assign` 继续使用 manual factory，绕过 UE ActionDatabase/NodeSpawner 行为 | UE 已有 `MakeAssignDelegateNodeSpawner` 与 `UK2Node_AssignDelegate::PostPlacedNewNode` | 与编辑器行为不等价，容易漏自动 custom event、graph compatibility、analytics/dirty 标记等路径 | 收敛到 `UBlueprintDelegateNodeSpawner`；assign manual factory 删除或仅作 legacy fallback |
| R2 | component getter 在 EventDelegate builder 内硬编码创建 | UE target pin 与 variable/component getter 是通用 Field/ActionContext composition | component/native/inherited/variable/linked pin 无法通用，readback 难归因 | EventDelegate 只消费 binding object evidence；getter 由 Field/component_ref 生成 |
| R3 | handler/signature projection 缺失导致 bind/unbind 生成不可靠 | `UK2Node_CreateDelegate::IsValid` 与 compiler 对 function name/signature/object scope 有硬校验 | 编译报错或 silent wrong handler | 缺 handler/signature evidence 时 deterministic failure；错误进入 DebugBundle |
| R4 | component-bound duplicate event 处理不明确 | UE 通过 `(ComponentPropertyName, DelegatePropertyName)` 查重，paste/rename 也防重 | 重复事件导致编译/运行时绑定歧义 | 默认 fail 或 return existing；replace/merge 后置决策 |
| R5 | `delegate.call` 只验证节点存在，不验证参数 pin | `UK2Node_CallDelegate` 从 signature 展开 pin，并有 callable/metadata 编译约束 | 参数 default/link 错误无法被 statement 级 readback 捕获 | readback 检查 signature pin map、defaults、links |
| R6 | compile diagnostics 无法关联到 statement | UE diagnostics 来自 multiple handlers：DelegateNodeHandlers、CreateDelegate、ComponentBoundEvent | 用户只看到失败，Agent 无法定位 | DebugBundle 以 node guid/operation id 关联 MessageLog |
| R7 | Review 粒度扩张到 per-delegate target | Review 设计应保持 graph_block 级别 | 审核 UI 和 rollback 粒度过细、复杂化 | delegate detail 放 DebugBundle/readback，不扩展 Review target |
| R8 | FunctionGraph 中错误允许 assign/component-bound event | `assign` 与 event node 只允许 Ubergraph；BaseMCDelegate 其他节点允许 FunctionGraph | 生成不可创建/不可编译节点 | Resolver 阶段 enforce graph compatibility |
| R9 | UObject Blueprint 与 Actor Blueprint 组件能力混淆 | `DoesSupportComponents` 要求 Actor based + SCS；普通 delegate nodes 不依赖 SCS | 在 UObject BP 试图创建 component-bound event | component-bound event scope 仅 Actor component property；delegate ops 可支持 UObject |

---

## 12. 当前 BlueprintHelper EventDelegate 与目标覆盖之间的真实缺陷

> 本节的 BlueprintHelper / AgentFaceService 路径来自任务输入文件的“当前状态判断”。本次任务只读 UE 源码包，没有修改或重新检索 BlueprintHelper 仓库。

| defect_id | 缺陷 | 输入证据路径 | UE 5.6 对照证据 | 修复方向 |
| --- | --- | --- | --- | --- |
| D1 | `assign` 当前 resolver 返回 manual assign factory，builder 手动 `NewObject<UK2Node_AssignDelegate>`，不是 ActionDatabase/NodeSpawner 等价路径 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:205`、`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp:199` | UE ActionDatabase 已通过 `MakeAssignDelegateNodeSpawner` 返回 `UBlueprintDelegateNodeSpawner::Create(UK2Node_AssignDelegate::StaticClass(), DelegateProperty)`；`PostPlacedNewNode` 自动创建 attached custom event | 删除/收敛 manual factory；resolver 返回 spawner evidence；builder 只 invoke spawner/连接 evidence |
| D2 | delegate target 连接当前会手动创建 `UK2Node_VariableGet` 作为 component getter，binding object 未统一 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp:229` | UE BaseMCDelegate target pin 可从 typed object pin autowire；compiler 对 self/linked target 均有通用处理 | 复用 Field/component_ref/linked pin evidence；EventDelegate 不再硬编码 getter |
| D3 | use-site evidence reader 明确要求 projected handler/signature evidence，但当前能力未证明完整 ActionContext projection / deterministic failure / DebugBundle | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:321`、`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:345` | `UK2Node_CreateDelegate` 对 function name、scope class、signature compatibility 有强校验；compile handler 对缺 signature/handler 报错 | ActionContext 统一投影 handler/signature；缺 evidence 直接失败并输出 compile-like diagnostic |
| D4 | Public capability contract 仍把 `delegate_component_bound_event` 标为 `discussion-gated`，不是完整 supported contract | `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts:130` | UE 源码表明 component-bound event、bind/assign/unbind/call/clear 均有明确 node/spawner/validation 边界 | 在实现完成后更新 capability contract 为 supported，并列出 graph/BP 限制 |
| D5 | TaskSpec 虽已支持 `component_bound_event` 与 delegate operations，但当前 C++ resolver 支持 node class 不等于编辑器等价能力 | `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1408`、`AgentFaceService/task-core/src/task/compiler/task-compiler.ts:2091`、`AgentFaceService/task-core/src/task/compiler/task-compiler.ts:3407`；`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:50`、`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:159`、`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:216` | UE 等价能力还需要 ActionDatabase filter、binding object、handler signature、readback 与 compile diagnostics | Resolver/Builder/Readback 补齐 spawner evidence、binding evidence、diagnostic facts |
| D6 | Review/DebugBundle 对 delegate operation 细节真实缺口尚未补齐 | 任务输入风险项要求覆盖 Review/DebugBundle 缺口 | UE diagnostics 分散在 DelegateNodeHandlers、CreateDelegate、ComponentBoundEvent | graph_block 级 Review 保持不变；operation facts 进入 DebugBundle/readback |

---

## 13. 后续实现前必须讨论的架构决策

| decision_id | 需要决策的问题 | 选项 | 影响范围 |
| --- | --- | --- | --- |
| A1 | `delegate.assign` 是否允许使用 UE 默认 attached custom event side effect？ | 选项 1：允许，并把自动 custom event 作为 BlueprintSignature side effect 记录；选项 2：禁止 EventDelegate 创建 handler declaration，要求已有 projected handler；选项 3：分 profile 控制 | TaskSpec、BlueprintSignature、FragmentBuilder、Review/DebugBundle |
| A2 | component-bound duplicate event 默认策略是什么？ | fail、return existing、replace、merge | EventDelegate resolver、Safety profile、Review、readback |
| A3 | binding object evidence 的统一 ID 形态是什么？ | `component_ref`、`field_get_ref`、`linked_pin_ref`、`function_return_ref` 单独字段；或统一 `binding_object_evidence_id` | ActionContext、Field、FunctionAction、FragmentBuilder |
| A4 | handler/signature evidence 的最小契约是什么？ | 只接受 `handler_function_path` + `delegate_signature_function_path`；或接受 event node ref/custom event ref/CreateDelegate ref | BlueprintSignature、EventDelegateAction、readback |
| A5 | `delegate.clear` public name 是否统一为 `clear` 还是 `unbind_all`？ | API 保持 `clear`，readback/DebugBundle 标注 UE title `Unbind all`；或 public schema 同时支持 alias | TaskSpec compiler、capability contract、文档 |
| A6 | FunctionGraph 中 delegate operations 的安全策略是否与 EventGraph 相同？ | BaseMCDelegate bind/unbind/call/clear 可支持 FunctionGraph；assign/component-bound 禁止；或保守第一版仅 EventGraph | ActionResolution filter、capability matrix、测试 |
| A7 | compile diagnostics 如何关联到 GraphWrite statement？ | node guid -> statement id map；MessageLog scrape；compiler result structured facts | Readback、DebugBundle、用户错误报告 |
| A8 | P2 target，如 function return object target，是否由 EventDelegate 自动创建上游节点？ | 禁止，必须由 FunctionAction/Field composition 提供；或允许 compound operation | FunctionAction、GraphWrite orchestration、rollback |

---

## 14. 最终结论

| 结论项 | 实际结论 |
| --- | --- |
| UE 5.6 普通 Blueprint delegate/component-bound 编辑器操作总数 | **6 类**。计数规则：按本任务 `public_operation` 去重，只统计普通 Blueprint 图内 component-bound/dynamic multicast delegate use-site：`component_bound_event`、`delegate.bind`、`delegate.assign`、`delegate.unbind`、`delegate.call`、`delegate.clear`。不把 `Create Event`、custom event declaration、override/native event declaration、timer/async helper、Animation BP、UMG designer、Details 面板编辑计入分母。 |
| 建议 GraphWrite first-class 覆盖数量 | **6 类**：`event_delegate.component_bound_event`、`event_delegate.delegate_bind`、`event_delegate.delegate_assign`、`event_delegate.delegate_unbind`、`event_delegate.delegate_call`、`event_delegate.delegate_clear`。 |
| 可达到的编辑器操作覆盖比例 | **100.0%（6/6）**，分母已排除非 GraphWrite 职责。若把 excluded 的设计器入口、handler declaration、timer/async helper 作为“编辑器入口”额外计入，GraphWrite EventDelegate 不应追求 100%，因为这些属于 BlueprintSignature、FunctionAction、GenericSchedule、existing non-GraphWrite tool 或 excluded。 |
| 仍由其他 owner 保留职责的数量 | **5 个 owner cluster/tool**：`BlueprintSignature`（handler/custom/override/native declaration 与 signature）、`FunctionAction`（function call / async helper / function return target composition）、`GenericSchedule`（timer delegate）、`existing non-GraphWrite tool`（Details/asset/editor UI）、`excluded`（Animation BP、UMG designer 等非普通 Blueprint 范围）。 |
| 当前 BlueprintHelper EventDelegate 实现与目标覆盖之间的真实缺陷 | **6 项**：D1 manual assign factory；D2 component getter 拼装；D3 handler/signature projection 与 deterministic diagnostic 缺口；D4 public capability contract discussion-gated；D5 TaskSpec/resolver 支持 node class 但未达到 ActionDatabase/filter/readback 等价；D6 Review/DebugBundle operation facts 缺口。详见第 12 节。 |
| 后续实现前必须讨论的架构决策 | **8 项**：assign 自动 custom event side effect、duplicate component-bound event 策略、binding object evidence ID、handler/signature最小契约、clear vs unbind_all 命名、FunctionGraph 安全策略、compile diagnostic 关联、P2 upstream target composition。详见第 13 节。 |

---

## 15. 源码证据索引

| 主题 | source_path |
| --- | --- |
| Delegate operation ActionDatabase 注册 | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:721-770` |
| Assign delegate spawner factory | `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:150-157`、`Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:275-280` |
| Delegate node spawner 创建与 property 设置 | `Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp:74-118` |
| Bound event spawner | `Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp:70-194` |
| Binding-specific filter | `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:1279-1288`、`Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:2239-2240` |
| BaseMCDelegate graph compatibility / target pin / autowire | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:71-78`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:95-114`、`Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:252-300` |
| Add/Remove/Clear/Call delegate nodes | `Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp:334-490` |
| Assign delegate behavior | `Source/Editor/BlueprintGraph/Classes/K2Node_AssignDelegate.h:17-21`、`Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp:73-116` |
| Delegate compiler handlers | `Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:45-101`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:133-221`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:223-368`、`Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp:373-482` |
| CreateDelegate validation | `Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:45-57`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:71-199`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:201-207`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:331-405`、`Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp:505-508` |
| CustomEvent signature behavior | `Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp:390-536` |
| EventGraph restriction for event nodes | `Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp:514-523` |
| ComponentBoundEvent initialize/dynamic binding/validation | `Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp:67-214` |
| SCS component event menu | `Source/Editor/Kismet/Private/SSCSEditor.cpp:4388-4420`、`Source/Editor/Kismet/Private/SSCSEditor.cpp:4492-4587` |
| CreateNewBoundEventForComponent / FindBoundEventForComponent | `Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2554-2596`、`Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2622-2640` |
| Graph type / EventGraph support / component support | `Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:3261-3294`、`Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:3426-3447` |
| SCS node variable property semantics | `Source/Runtime/Engine/Classes/Engine/SCS_Node.h:149-156`、`Source/Runtime/Engine/Classes/Engine/SCS_Node.h:205-212` |
