# GraphWrite EventDelegate UE Editor Capability Engine Source Read Task

日期：2026-05-25

## 目标

为 GraphWrite `component_bound_event` 与 `delegate_operation=bind|assign|unbind|call|clear` 的完全通用化实现提供 UE 5.6 源码证据，确认普通 Blueprint `EventGraph` / `FunctionGraph` 中编辑器可创建的组件绑定事件、委托绑定、委托赋值、解绑、调用、清空能力边界，并产出一份可用于后续实现的 80% 编辑器操作覆盖矩阵。

本任务只做只读源码探索和证据表导出，不修改 BlueprintHelper、AgentFaceService 或 UE 源码。

## 当前状态判断

结论：当前能力不是完全通用化，也没有达到编辑器同等能力。

源码证据：

| 现状 | 证据 |
| --- | --- |
| Agent-facing TaskSpec 已支持 `component_bound_event`、`delegate.bind`、`delegate.assign`、`delegate.unbind`、`delegate.unbind_all`、`delegate.call` 并 lower 到 `kind=component_bound_event` 或 `kind=delegate + delegate_operation`。 | `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1408`、`AgentFaceService/task-core/src/task/compiler/task-compiler.ts:2091`、`AgentFaceService/task-core/src/task/compiler/task-compiler.ts:3407` |
| C++ resolver 已支持 `UK2Node_AddDelegate`、`UK2Node_AssignDelegate`、`UK2Node_RemoveDelegate`、`UK2Node_CallDelegate`、`UK2Node_ClearDelegate` 和 `UK2Node_ComponentBoundEvent`。 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:50`、`:159`、`:216` |
| use-site evidence reader 明确要求 projected handler/signature evidence，不再扫描自定义事件补上下文。 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:321`、`:345` |
| `assign` 当前 resolver 返回 manual assign factory，builder 里手动 `NewObject<UK2Node_AssignDelegate>`，不是完整 ActionDatabase/NodeSpawner 等价路径。 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp:205`、`BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp:199` |
| delegate target 连接当前会手动创建 `UK2Node_VariableGet` 作为 component getter，说明 binding object 通用路径尚未完全收敛到统一 ActionContext/Field evidence。 | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp:229` |
| public capability contract 仍把 `delegate_component_bound_event` 标为 `discussion-gated`，不是完整 supported contract。 | `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts:130` |

## 根目录约定

本任务所有检索均以 UE 5.6 引擎目录作为根目录执行：

```powershell
$EngineRoot = "E:\UE_5.6\Engine"
Set-Location $EngineRoot
```

文档、结果表和引用中的 UE 源码路径必须使用 `Source/...` 相对路径，例如：

```text
Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp:74
```

不要把 BlueprintHelper 仓库作为 UE 源码检索根。BlueprintHelper 只用于存放最终结果文档。

## 输出文件

新线程完成探索后输出到：

```text
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_EventDelegate_UEEditorCapability_EngineSourceReadResult_20260525_CN.md
```

输出必须包含：

1. 编辑器可见操作表：每个 component-bound/delegate use-site 操作一行。
2. 80% 覆盖建议：哪些操作进入 GraphWrite EventDelegate first-class 能力，哪些交给 BlueprintSignature、FunctionAction、Generic Schedule 或现有非 GraphWrite 工具。
3. 实现边界建议：TaskSpec、ActionContext projection、ActionResolution、FragmentBuilder、readback 各自需要哪些证据。
4. 风险清单：manual assign factory、component getter 拼装、handler/signature projection、compile diagnostics、Review/DebugBundle 的真实缺口。

## 职责边界

### 纳入范围

| 能力 | 说明 |
| --- | --- |
| `component_bound_event` | 普通 Actor Blueprint 组件委托的 event entry use-site，例如组件 overlap/click/hit 等事件入口。 |
| `delegate_operation=bind` | 创建 `Add Delegate` 节点并连接 `Create Event` / `Create Delegate` handler。 |
| `delegate_operation=assign` | 创建 `Assign Delegate` 节点及其自动/关联 handler use-site 行为。 |
| `delegate_operation=unbind` | 创建 `Remove Delegate` 节点并连接指定 handler。 |
| `delegate_operation=call` | 创建 `Call Delegate` 节点，并绑定委托签名参数输入。 |
| `delegate_operation=clear` | 创建 `Clear Delegate` / clear all 节点，不绑定 handler。 |
| binding object | self、component ref、typed object pin、variable get 作为 delegate target 的图内连接。 |

### 排除范围

| 项 | Owner / 原因 |
| --- | --- |
| custom event declaration、override event declaration、native event declaration | 由 BlueprintSignature 创建或确认；GraphWrite/EventDelegate 只消费 projected handler/signature evidence。 |
| timer delegate node、latent/async delegate helper | 由 Generic Schedule 或 FunctionAction 负责，不混入 EventDelegateActionCluster。 |
| Animation Blueprint notify/event 专属入口 | 不属于当前普通 Blueprint `EventGraph` / `FunctionGraph` 范围。 |
| UMG designer widget event 绑定 | 非普通 Blueprint 图内 action menu 能力，除非源码证据证明它在普通 `EventGraph` 中以同一 K2 delegate use-site 形式出现。 |
| Details 面板属性委托绑定或 asset 内容编辑 | 不是 graph body 节点生成职责。 |

## 任务 1：ActionDatabase 与 Action Menu 委托操作枚举

目标：确认 UE 5.6 如何注册、过滤、展示 component-bound event 与 delegate operation 节点，并统计普通 Blueprint 图内可见的 action 全集。

读取路径：

```text
Source/Editor/BlueprintGraph/Public/BlueprintActionDatabase.h
Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp
Source/Editor/BlueprintGraph/Public/BlueprintActionFilter.h
Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintDelegateNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintBoundEventNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintBoundNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintBoundNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2_Actions.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2_Actions.cpp
```

建议检索：

```powershell
rg -n "UBlueprintDelegateNodeSpawner|UBlueprintBoundEventNodeSpawner|UBlueprintBoundNodeSpawner|UK2Node_AddDelegate|UK2Node_AssignDelegate|UK2Node_RemoveDelegate|UK2Node_CallDelegate|UK2Node_ClearDelegate|UK2Node_ComponentBoundEvent" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "RegisterClassFactoryActions|RegisterStructActions|GetGraphContextActions|GetContextMenuActions|FBlueprintActionFilter|IsFilteredOut|BindingSpecific|BindingSet|FBindingObject" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "AddDelegate|AssignDelegate|RemoveDelegate|ClearDelegate|CallDelegate|BoundEvent|ComponentBoundEvent|Create Event|CreateDelegate" Source/Editor/BlueprintGraph Source/Editor/Kismet -g "*.h" -g "*.cpp"
```

交付内容：

| 字段 | 说明 |
| --- | --- |
| `editor_entry` | 右键空白图、从组件面板事件菜单、从 delegate pin 拖拽、从变量/组件拖拽等。 |
| `public_operation` | `component_bound_event`、`delegate.bind`、`delegate.assign`、`delegate.unbind`、`delegate.call`、`delegate.clear`。 |
| `ue_spawner` | `UBlueprintDelegateNodeSpawner`、`UBlueprintBoundEventNodeSpawner`、`UBlueprintBoundNodeSpawner` 或其他。 |
| `ue_node_class` | `UK2Node_*`。 |
| `graph_context_filter` | EventGraph、FunctionGraph、Actor Blueprint、component context、pin context 等限制。 |
| `source_path` | `Source/...:line`。 |

## 任务 2：component_bound_event 组件事件入口能力

目标：确认组件绑定事件在编辑器中如何从 SCS/native/inherited component 暴露、如何避免重复事件、如何绑定到 `UK2Node_ComponentBoundEvent`。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_ComponentBoundEvent.h
Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_Event.h
Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintBoundEventNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp
Source/Editor/Kismet/Public/SSCSEditor.h
Source/Editor/Kismet/Private/SSCSEditor.cpp
Source/Editor/Kismet/Public/SMyBlueprint.h
Source/Editor/Kismet/Private/SMyBlueprint.cpp
Source/Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h
Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp
Source/Runtime/Engine/Classes/Engine/SimpleConstructionScript.h
Source/Runtime/Engine/Classes/Engine/SCS_Node.h
```

建议检索：

```powershell
rg -n "UK2Node_ComponentBoundEvent|InitializeComponentBoundEventParams|RegisterDynamicBinding|GetTargetDelegateProperty|IsDelegateValid|CanPasteHere|FindPreExistingEvent|BindToNode|IsBindingCompatible" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "Add Event|AddComponentEvent|ComponentBoundEvent|GetEventDelegate|OnSelectionUpdated|FSCSEditorTreeNode|Subobject|SCS_Node|GetVariableName" Source/Editor/Kismet Source/Editor/UnrealEd Source/Runtime/Engine -g "*.h" -g "*.cpp"
rg -n "SimpleConstructionScript|FindSCS_Node|GetSCSVariableNameList|IsSCSComponentProperty|NativeComponent|InheritedSCS|InheritableComponentHandler" Source/Editor/Kismet Source/Editor/UnrealEd Source/Runtime/Engine -g "*.h" -g "*.cpp"
```

交付内容：

| 字段 | 说明 |
| --- | --- |
| `component_kind` | SCS、inherited SCS、native inherited、component template、instance-only。 |
| `delegate_property_source` | component class、component template、owner class、dynamic binding source。 |
| `preexisting_event_rule` | UE 如何检测重复 bound event。 |
| `binding_evidence` | GraphWrite 需要的 component path、owner class、component field path、delegate property path。 |
| `graphwrite_scope` | first-class、diagnostic-only、excluded。 |

## 任务 3：delegate bind / assign / unbind / call / clear 节点族

目标：确认每个 delegate operation 的 UE node class、spawner、pin contract、compile handler，评估 GraphWrite 当前 manual assign factory 是否可以收敛到 UE spawner evidence。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_BaseMCDelegate.h
Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_AddDelegate.h
Source/Editor/BlueprintGraph/Classes/K2Node_AssignDelegate.h
Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_RemoveDelegate.h
Source/Editor/BlueprintGraph/Classes/K2Node_CallDelegate.h
Source/Editor/BlueprintGraph/Classes/K2Node_ClearDelegate.h
Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.h
Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintDelegateNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp
```

建议检索：

```powershell
rg -n "UK2Node_AddDelegate|UK2Node_AssignDelegate|UK2Node_RemoveDelegate|UK2Node_CallDelegate|UK2Node_ClearDelegate|UK2Node_BaseMCDelegate|FMulticastDelegateProperty|GetDelegatePin|GetTargetDelegateProperty" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "Create\\(TSubclassOf<UK2Node_BaseMCDelegate>|Invoke|GetDelegateProperty|SetFromProperty|AllocateDefaultPins|PostPlacedNewNode|IsCompatibleWithGraph" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "CreateNodeHandler|KCHandler|DelegateNodeHandlers|RegisterNet|Compile|ValidateNodeDuringCompilation|MessageLog" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

交付内容：

| operation | 必须确认的问题 |
| --- | --- |
| `bind` | 是否总是 `UK2Node_AddDelegate + UK2Node_CreateDelegate`，目标 pin 和 delegate pin 如何连接。 |
| `assign` | 是否 ActionDatabase 已注册 `UBlueprintDelegateNodeSpawner`，是否需要手动创建 attached custom event，GraphWrite 是否可以避免 manual factory。 |
| `unbind` | 单个 handler 解绑的 create delegate 连接规则，是否存在按 object/function 的编辑器变体。 |
| `call` | 参数 pin 如何从 delegate signature 展开，返回/输出参数是否存在限制。 |
| `clear` | clear all 是否没有 handler，是否存在 clear object 或 clear delegate list 的其他编辑器入口。 |

## 任务 4：handler / signature / CreateDelegate 证据链

目标：确认 handler 函数、custom event、Create Event 节点与 delegate signature 的编辑器校验规则，并保持 BlueprintSignature 与 EventDelegate 的职责拆分。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_CreateDelegate.h
Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_CustomEvent.h
Source/Editor/BlueprintGraph/Private/K2Node_CustomEvent.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_Event.h
Source/Editor/BlueprintGraph/Private/K2Node_Event.cpp
Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/KismetCompiler/Public/KismetCompiler.h
Source/Editor/KismetCompiler/Private/KismetCompiler.cpp
```

建议检索：

```powershell
rg -n "UK2Node_CreateDelegate|SetFunction|GetFunctionName|GetDelegateSignature|GetScopeClass|IsValid|ValidationAfterFunctionsAreCreated|Create Event|CreateDelegate" Source/Editor/BlueprintGraph Source/Editor/KismetCompiler -g "*.h" -g "*.cpp"
rg -n "SignatureFunction|FMemberReference|ResolveSimpleMemberReference|PinSubCategoryMemberReference|DelegateSignature|CustomEvent|UserDefinedEvent" Source/Editor/BlueprintGraph Source/Runtime -g "*.h" -g "*.cpp"
rg -n "CanCreateConnection|ArePinsCompatible|PC_Delegate|PC_MCDelegate|CreateDelegate|MessageLog\\.Error|MessageLog\\.Warning" Source/Editor/BlueprintGraph Source/Editor/KismetCompiler -g "*.h" -g "*.cpp"
```

交付内容：

| 字段 | 说明 |
| --- | --- |
| `handler_source` | custom event、function、existing event entry、Create Event 节点。 |
| `signature_match_rule` | 参数、const/ref、return、owner class、object pin 的匹配规则。 |
| `signature_evidence` | `delegate_signature`、`delegate_signature_function_path`、`handler_function_path`、`signature_evidence_id` 是否足够。 |
| `owner_boundary` | BlueprintSignature 负责声明/查找，EventDelegate 只消费 use-site evidence 的边界是否成立。 |
| `failure_diagnostic` | 缺 handler、签名不匹配、scope 错误时 UE 编译/编辑器诊断。 |

## 任务 5：binding object 与 target pin 通用化

目标：确认 delegate target 可以来自 self、component reference、变量、typed object pin、函数返回值时，UE 编辑器如何筛选和连接；判断 GraphWrite 是否应复用 Field/component_ref evidence，而不是在 EventDelegate builder 手动创建 getter。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_VariableGet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintVariableNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/BlueprintComponentNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp
Source/Editor/BlueprintGraph/Private/BlueprintNodeSpawnerUtils.cpp
```

建议检索：

```powershell
rg -n "FBindingObject|BindingSet|TargetPin|PN_Self|GetDelegatePin|GetObjectInPin|TryCreateConnection|CanCreateConnection|ArePinsCompatible" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "BlueprintVariableNodeSpawner|BlueprintComponentNodeSpawner|CreateFromMemberOrParam|CreateFromComponent|VariableReference|SetSelfMember|ComponentProperty" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "SelfContext|bSelfContext|ContextTarget|FromPin|PinType|TargetObject|ComponentReference" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

交付内容：

| binding source | UE 表达 | GraphWrite 建议 |
| --- | --- | --- |
| self delegate property | target pin 使用 self 或无显式连接。 | EventDelegate 直接 spawner。 |
| component delegate property | component getter 或 binding object。 | 复用 Field `component_ref` evidence。 |
| variable object delegate | variable getter + target pin。 | 复用 Field get evidence。 |
| linked typed object pin | 从上游 pin 直接连接 target。 | 需要 ActionContext linked pin evidence。 |
| function return object | call result + target pin。 | 由 FunctionAction/Field composition 提供 upstream pin。 |

## 任务 6：Graph 类型、Blueprint 类型与编辑器限制

目标：确定哪些 delegate/component-bound 操作在 `EventGraph`、`FunctionGraph`、普通 Actor Blueprint、普通 UObject Blueprint 中可创建，哪些只在特定图或特定资产类型可见。

读取路径：

```text
Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp
Source/Editor/BlueprintGraph/Private/K2Node_AssignDelegate.cpp
Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp
Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp
```

建议检索：

```powershell
rg -n "IsCompatibleWithGraph|IsAllowableBlueprintVariableType|FunctionGraph|Ubergraph|EventGraph|CanPasteHere|CanUserDeleteNode|IsEventGraph|IsDelegateValid" Source/Editor/BlueprintGraph Source/Editor/UnrealEd -g "*.h" -g "*.cpp"
rg -n "bIsBindingSpecificSpawner|IsFilteredOut|Context.Blueprints|Context.Graphs|GraphType|Schema" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "BlueprintType|ActorBased|IsActorBased|SupportsEventGraphs|SupportsComponents|SupportsDelegates" Source/Editor/BlueprintGraph Source/Editor/UnrealEd Source/Runtime -g "*.h" -g "*.cpp"
```

交付内容：

| operation | EventGraph | FunctionGraph | Actor Blueprint | UObject Blueprint | 限制来源 |
| --- | --- | --- | --- | --- | --- |
| `component_bound_event` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |
| `delegate.bind` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |
| `delegate.assign` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |
| `delegate.unbind` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |
| `delegate.call` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |
| `delegate.clear` | 写实际结论。 | 写实际结论。 | 写实际结论。 | 写实际结论。 | `Source/...:line`。 |

## 任务 7：readback、compile diagnostics 与成功判定

目标：定义 GraphWrite 生成后如何证明 component-bound/delegate use-site 成功，而不是只证明 resolver 找到候选。

读取路径：

```text
Source/Editor/BlueprintGraph/Private/DelegateNodeHandlers.cpp
Source/Editor/BlueprintGraph/Private/K2Node_ComponentBoundEvent.cpp
Source/Editor/BlueprintGraph/Private/K2Node_CreateDelegate.cpp
Source/Editor/BlueprintGraph/Private/K2Node_MCDelegate.cpp
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/KismetCompiler/Public/KismetCompiler.h
Source/Editor/KismetCompiler/Private/KismetCompiler.cpp
```

建议检索：

```powershell
rg -n "ValidateNodeDuringCompilation|ValidationAfterFunctionsAreCreated|MessageLog\\.Error|MessageLog\\.Warning|RegisterDynamicBinding|DelegateBinding|ComponentDelegateBinding|CreateNodeHandler" Source/Editor/BlueprintGraph Source/Editor/KismetCompiler -g "*.h" -g "*.cpp"
rg -n "GetDelegatePin|GetDelegateOutPin|GetObjectInPin|GetFunctionName|GetTargetDelegateProperty|LinkedTo|DefaultValue|PinType|PinSubCategoryMemberReference" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "Compile|FKismetCompilerContext|DynamicBindingObjects|FBlueprintComponentDelegateBinding|FBlueprintDelegateBinding" Source/Editor/BlueprintGraph Source/Editor/KismetCompiler Source/Runtime -g "*.h" -g "*.cpp"
```

交付内容：

| evidence | 通过标准 |
| --- | --- |
| node class | 节点类匹配 operation 对应 UE 节点族。 |
| delegate property | node 上的 delegate property 解析到预期 `FMulticastDelegateProperty`。 |
| binding object | component/self/object target 连接到正确 target pin 或 binding set。 |
| create delegate | bind/assign/unbind 的 `UK2Node_CreateDelegate` 指向 projected handler function。 |
| call inputs | `delegate.call` 参数 pin 与 delegate signature 一致，默认值和 link 正确。 |
| dynamic binding | `component_bound_event` 注册到正确 component delegate binding。 |
| compile result | Blueprint 编译无与该 statement 相关的 error；warning 进入 DebugBundle。 |

## 任务 8：80% 编辑器能力覆盖矩阵

目标：按普通 Blueprint 用户常见操作频率和架构价值，确定 GraphWrite EventDelegate first-class 能力优先级，避免为了低频特殊入口硬编码。

建议检索：

```powershell
rg -n "Add Event|Assign|Bind|Unbind|Call|Clear|Delegate|ComponentBound|Create Event|OnClicked|OnBeginOverlap|OnEndOverlap|OnHit" Source/Editor/BlueprintGraph Source/Editor/Kismet Source/Runtime/Engine -g "*.h" -g "*.cpp"
rg -n "MenuBuilder\\.AddMenuEntry|AddMenuEntry|FBlueprintEditorCommands|FMyBlueprintCommands|GetGraphContextActions|GetContextMenuActions" Source/Editor/Kismet Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

输出矩阵字段：

| 字段 | 说明 |
| --- | --- |
| `capability_id` | 例如 `event_delegate.component_bound_event`、`event_delegate.delegate_bind`。 |
| `editor_operation` | 编辑器中用户看到或执行的操作名称。 |
| `graph_scope` | `EventGraph`、`FunctionGraph`、或二者均可。 |
| `blueprint_scope` | Actor Blueprint、UObject Blueprint、component owner、其他。 |
| `ue_node_class` | 具体 UE 节点类。 |
| `spawner_or_api` | NodeSpawner、ActionDatabase、schema action、或动态 binding API。 |
| `minimum_task_spec_context` | TaskSpec 必须显式给出的最小上下文。 |
| `action_context_projection` | 可从 Blueprint/graph/linked pin/Signature/Field 自动投影的证据。 |
| `readback_evidence` | 运行后可验证证据。 |
| `coverage_priority` | `P0` / `P1` / `P2` / `excluded`。 |
| `owner_cluster` | EventDelegateAction、BlueprintSignature、FunctionAction、GenericSchedule、existing non-GraphWrite tool、excluded。 |
| `reason` | 纳入或排除理由。 |
| `source_path` | `Source/...:line`。 |

优先级规则：

| 优先级 | 纳入条件 |
| --- | --- |
| `P0` | 普通 Blueprint 最常见且 EventDelegate use-site 主线必须支持：component bound event、bind、assign、unbind、clear、call 的标准 dynamic multicast delegate。 |
| `P1` | 为 80% 编辑器能力必要：component/native/inherited binding object、typed target pin、CreateDelegate handler signature 校验、call 参数 readback。 |
| `P2` | 通用化重要但实现风险较高：非 self target、函数返回对象 target、复杂 owner class 消歧、重复 bound event replacement/merge 策略。 |
| `excluded` | custom/override/native event declaration、timer/async delegate helper、动画/UMG 专属入口、非 graph body 编辑。 |

## 最终结论要求

新线程结果文档末尾必须给出以下结论：

| 结论项 | 写法要求 |
| --- | --- |
| UE 5.6 普通 Blueprint delegate/component-bound 编辑器操作总数 | 写实际数字，并说明计数规则。 |
| 建议 GraphWrite first-class 覆盖数量 | 写实际数字，并列出纳入的 capability id。 |
| 可达到的编辑器操作覆盖比例 | 写实际百分比，并说明分母是否排除了非 GraphWrite 职责。 |
| 仍由其他 owner 保留职责的数量 | 写实际数字，并列出 owner cluster/tool。 |
| 当前 BlueprintHelper EventDelegate 实现与目标覆盖之间的真实缺陷 | 写源码证据支持的缺陷列表，每项包含 source path。 |
| 后续实现前必须讨论的架构决策 | 写需要用户决策的问题列表，每项包含影响范围。 |

填表时必须写源码证据支持的实际结论，不使用空泛或未落证据的描述。

## 后续实现边界

源码探索结果用于后续 GraphWrite EventDelegate 通用化实现，但本任务不实现。后续实现必须遵守：

1. EventDelegateActionCluster 只负责 delegate/component-bound use-site，不创建或查找 custom/override/native handler declaration。
2. handler 声明、handler signature、handler function path 由 BlueprintSignature 或 ActionContext projection 提供；缺 evidence 时确定性失败。
3. `assign` 如果 UE ActionDatabase 已有可用 spawner，应优先收敛到 spawner evidence，不保留 GraphWrite-local manual factory。
4. binding object 应复用 Field/component_ref/linked typed pin evidence，不在 EventDelegate builder 中为单个 component 名硬编码 getter。
5. 成功判定必须基于 node class、delegate property、binding object、CreateDelegate handler、pin links/defaults、dynamic binding、compile diagnostics。
6. Review evidence 保持 graph_block 级别；delegate operation 细节进入 DebugBundle/readback facts，不新增 per-delegate Review target 粒度。
